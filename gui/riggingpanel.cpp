/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "riggingpanel.h"
#include "localized_unit_labels.h"

#include <cmath>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "colorstore.h"
#include "columnutils.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "hoist_load_recalculation_prompt.h"
#include "mainwindow.h"
#include "rigging_extra_weight_settings.h"
#include "rigging_weight_validation.h"
#include "table_column_indices.h"
#include "units/unit_label_utils.h"
#include "units/units.h"

namespace {
constexpr const char *UNASSIGNED_POSITION = "Unassigned";

using RiggingColumn = RiggingTableColumns::Column;

// Converts a rigging column to its stable model index.
constexpr int ColumnIndex(RiggingColumn column) {
  return TableColumnIndices::ToIndex(column);
}

float RoundUpToNextFiveKg(float valueKg) {
  constexpr float kFiveKgStep = 5.0f;
  constexpr float kSafetyMarginFactor = 0.05f;

  const float totalWithSafetyMargin = valueKg + (valueKg * kSafetyMarginFactor);
  return std::ceil(totalWithSafetyMargin / kFiveKgStep) * kFiveKgStep;
}

bool IsRedCell(const ColorfulDataViewListStore *store, int row, int col) {
  if (!store || row < 0 || col < 0)
    return false;

  const size_t rowIndex = static_cast<size_t>(row);
  const size_t colIndex = static_cast<size_t>(col);
  if (rowIndex >= store->cellAttrs.size())
    return false;
  if (colIndex >= store->cellAttrs[rowIndex].size())
    return false;

  const wxDataViewItemAttr &attr = store->cellAttrs[rowIndex][colIndex];
  return attr.HasColour() && attr.GetColour() == *wxRED;
}

Units::WeightUnitSystem ResolveWeightUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
}

wxString BuildRiggingTooltipForColumn(int modelColumn) {
  const auto column = TableColumnIndices::FromIndex<RiggingColumn>(modelColumn);
  if (!column)
    return wxString();
  switch (*column) {
  case RiggingColumn::FixtureWeight:
    return "Fixture weight includes zero or missing values in this position.";
  case RiggingColumn::TrussWeight:
    return "Truss weight includes zero or missing values in this position.";
  case RiggingColumn::HoistWeight:
    return "Hoist weight includes zero or missing values in this position.";
  case RiggingColumn::ExtraWeight:
    return "Extra weight was auto-calculated and must be validated by the "
           "user.";
  case RiggingColumn::TotalWeight:
  case RiggingColumn::RoundedTotalWeight:
    return "Total weight includes values that need attention (missing weights "
           "or pending validation).";
  case RiggingColumn::Position:
  case RiggingColumn::Fixtures:
  case RiggingColumn::Trusses:
  case RiggingColumn::Hoists:
  case RiggingColumn::Count:
    return wxString();
  }
  return wxString();
}

void SetTableAndChildTooltips(wxDataViewListCtrl *table,
                              const wxString &tooltip) {
  if (!table)
    return;

  table->SetToolTip(tooltip);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    if (wxWindow *child = it->GetData())
      child->SetToolTip(tooltip);
  }
}

wxPoint NormalizeMousePositionForTable(wxDataViewListCtrl *table,
                                       const wxMouseEvent &event) {
  wxPoint position = event.GetPosition();
  wxWindow *sourceWindow = dynamic_cast<wxWindow *>(event.GetEventObject());
  if (!table || !sourceWindow || sourceWindow == table)
    return position;

  return table->ScreenToClient(sourceWindow->ClientToScreen(position));
}

template <typename Owner>
void BindTableHoverEvents(wxDataViewListCtrl *table, Owner *owner,
                          void (Owner::*onMouseMove)(wxMouseEvent &),
                          void (Owner::*onMouseLeave)(wxMouseEvent &)) {
  if (!table || !owner)
    return;

  auto bindEvents = [&](wxWindow *window) {
    if (!window)
      return;
    window->Unbind(wxEVT_MOTION, onMouseMove, owner);
    window->Unbind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
    window->Bind(wxEVT_MOTION, onMouseMove, owner);
    window->Bind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
  };

  bindEvents(table);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    bindEvents(it->GetData());
  }
}
} // namespace

static RiggingPanel *s_instance = nullptr;

RiggingPanel::RiggingPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  store = new ColorfulDataViewListStore();
  table =
      new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxDV_ROW_LINES | wxDV_VERT_RULES);
  table->AssociateModel(store);
  store->DecRef();
  BindTableHoverEvents(table, this, &RiggingPanel::OnMouseMove,
                       &RiggingPanel::OnMouseLeave);
  table->CallAfter([this]() {
    BindTableHoverEvents(table, this, &RiggingPanel::OnMouseMove,
                         &RiggingPanel::OnMouseLeave);
  });
  table->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED,
              &RiggingPanel::OnItemValueChanged, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &RiggingPanel::OnItemActivated,
              this);
  table->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
              &RiggingPanel::OnItemContextMenu, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_EDITING_STARTED,
              &RiggingPanel::OnItemEditingStarted, this);
  table->Bind(wxEVT_DATAVIEW_ITEM_EDITING_DONE,
              &RiggingPanel::OnItemEditingDone, this);
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  table->AppendTextColumn(_("Position"), wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(_("Fixtures"), wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(_("Trusses"), wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(_("Hoists"), wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Fixture Weight"), weightSuffix),
                          wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Truss Weight"), weightSuffix),
                          wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Hoists Weight"), weightSuffix),
                          wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Extra Weight"), weightSuffix),
                          wxDATAVIEW_CELL_EDITABLE, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Total Weight"), weightSuffix),
                          wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn(ui::LocalizedLabelWithUnit(_("Rounded Total Weight +5%"), weightSuffix),
                          wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);

  ColumnUtils::EnforceMinColumnWidth(table);

  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

RiggingPanel *RiggingPanel::Instance() { return s_instance; }

void RiggingPanel::SetInstance(RiggingPanel *panel) { s_instance = panel; }

namespace {
void AutoSizeColumns(wxDataViewListCtrl *table) {
  if (!table)
    return;

  const unsigned int columnCount = table->GetColumnCount();
  for (unsigned int i = 0; i < columnCount; ++i)
    table->GetColumn(i)->SetWidth(wxCOL_WIDTH_AUTOSIZE);
}
} // namespace

void RiggingPanel::RefreshData() {
  if (!table || !store)
    return;

  struct Totals {
    int fixtures = 0;
    int trusses = 0;
    int hoists = 0;
    float fixtureWeight = 0.0f;
    float trussWeight = 0.0f;
    float hoistWeight = 0.0f;
    float extraWeight = 0.0f;
    bool hasAutoUnvalidatedExtraWeight = false;
    bool hasZeroWeightFixture = false;
    bool hasZeroWeightTruss = false;
    bool hasZeroWeightHoist = false;
  };

  std::map<std::string, Totals> rows;
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  if (table->GetColumnCount() >= 10) {
    table->GetColumn(ColumnIndex(RiggingColumn::FixtureWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Fixture Weight"), weightSuffix));
    table->GetColumn(ColumnIndex(RiggingColumn::TrussWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Truss Weight"), weightSuffix));
    table->GetColumn(ColumnIndex(RiggingColumn::HoistWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Hoists Weight"), weightSuffix));
    table->GetColumn(ColumnIndex(RiggingColumn::ExtraWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Extra Weight"), weightSuffix));
    table->GetColumn(ColumnIndex(RiggingColumn::TotalWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Total Weight"), weightSuffix));
    table->GetColumn(ColumnIndex(RiggingColumn::RoundedTotalWeight))
        ->SetTitle(ui::LocalizedLabelWithUnit(_("Rounded Total Weight +5%"), weightSuffix));
  }
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto &scene = cfg.GetScene();
  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
          cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  for (const auto &[uuid, fixture] : scene.fixtures) {
    std::string pos = fixture.positionName.empty() ? UNASSIGNED_POSITION
                                                   : fixture.positionName;
    auto &entry = rows[pos];
    entry.fixtures++;
    if (!gui::rigging::AccumulateValidPhysicalWeightKg(
            fixture.weightKg, entry.fixtureWeight))
      entry.hasZeroWeightFixture = true;
  }

  for (const auto &[uuid, truss] : scene.trusses) {
    std::string pos =
        truss.positionName.empty() ? UNASSIGNED_POSITION : truss.positionName;
    auto &entry = rows[pos];
    entry.trusses++;
    if (!gui::rigging::AccumulateValidPhysicalWeightKg(truss.weightKg,
                                                       entry.trussWeight))
      entry.hasZeroWeightTruss = true;
  }

  for (const auto &[uuid, support] : scene.supports) {
    std::string pos = support.positionName.empty() ? UNASSIGNED_POSITION
                                                   : support.positionName;
    auto &entry = rows[pos];
    entry.hoists++;
    if (!gui::rigging::AccumulateValidPhysicalWeightKg(support.weightKg,
                                                       entry.hoistWeight))
      entry.hasZeroWeightHoist = true;
  }

  for (auto &[position, totals] : rows) {
    if (const auto it = extraWeights.find(position); it != extraWeights.end()) {
      totals.extraWeight = it->second.valueKg;
      totals.hasAutoUnvalidatedExtraWeight = it->second.requiresValidation;
    }
  }

  // Clear the custom store through one path so rows and colour attributes stay
  // synchronized.
  store->DeleteAllItems();
  rowPositions.clear();
  rowPositions.reserve(rows.size());
  for (const auto &[position, totals] : rows) {
    float totalWeight = totals.fixtureWeight + totals.trussWeight +
                        totals.hoistWeight + totals.extraWeight;
    float roundedFivePercentIncrease = RoundUpToNextFiveKg(totalWeight);
    wxVector<wxVariant> row;
    row.push_back(wxString::FromUTF8(position));
    row.push_back(wxString::Format("%d", totals.fixtures));
    row.push_back(wxString::Format("%d", totals.trusses));
    row.push_back(wxString::Format("%d", totals.hoists));
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        totals.fixtureWeight, weightUnit, Units::ValueFormatContext::Table)));
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        totals.trussWeight, weightUnit, Units::ValueFormatContext::Table)));
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        totals.hoistWeight, weightUnit, Units::ValueFormatContext::Table)));
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        totals.extraWeight, weightUnit, Units::ValueFormatContext::Table)));
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        totalWeight, weightUnit, Units::ValueFormatContext::Table)));
    row.push_back(wxString::FromUTF8(
        Units::FormatWeightFromKilograms(roundedFivePercentIncrease, weightUnit,
        Units::ValueFormatContext::Table)));
    unsigned int rowIndex = store->GetItemCount();
    store->AppendItem(row);
    rowPositions.push_back(position);

    const bool fixtureWeightZero = totals.hasZeroWeightFixture;
    const bool trussWeightZero = totals.hasZeroWeightTruss;
    const bool hoistWeightZero = totals.hasZeroWeightHoist;

    if (fixtureWeightZero)
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::FixtureWeight), *wxRED);
    if (trussWeightZero)
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::TrussWeight), *wxRED);
    if (hoistWeightZero)
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::HoistWeight), *wxRED);

    if (totals.hasAutoUnvalidatedExtraWeight)
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::ExtraWeight), *wxRED);

    if (fixtureWeightZero || trussWeightZero || hoistWeightZero) {
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::TotalWeight), *wxRED);
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::RoundedTotalWeight), *wxRED);
    }
    if (totals.hasAutoUnvalidatedExtraWeight) {
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::TotalWeight), *wxRED);
      store->SetCellTextColour(
          rowIndex, ColumnIndex(RiggingColumn::RoundedTotalWeight), *wxRED);
    }
  }

  AutoSizeColumns(table);

  // Force a repaint so colour changes are visible immediately after the
  // refresh is triggered (e.g. after loading/importing data or editing
  // weights in the tables).
  table->Refresh();
}

void RiggingPanel::OnItemActivated(wxDataViewEvent &event) {
  if (!table) {
    event.Skip();
    return;
  }

  if (event.GetColumn() != ColumnIndex(RiggingColumn::ExtraWeight)) {
    event.Skip();
    return;
  }

  table->EditItem(event.GetItem(),
                  table->GetColumn(ColumnIndex(RiggingColumn::ExtraWeight)));
  event.Skip();
}

void RiggingPanel::OnItemContextMenu(wxDataViewEvent &event) {
  if (!table) {
    event.Skip();
    return;
  }

  const wxDataViewItem item = event.GetItem();
  const int column = event.GetColumn();
  if (!item.IsOk() || column != ColumnIndex(RiggingColumn::ExtraWeight)) {
    event.Skip();
    return;
  }

  table->EditItem(item,
                  table->GetColumn(ColumnIndex(RiggingColumn::ExtraWeight)));
  event.Skip();
}

void RiggingPanel::OnItemEditingStarted(wxDataViewEvent &event) {
  if (event.GetColumn() == ColumnIndex(RiggingColumn::ExtraWeight) &&
      !shortcutsTemporarilyDisabled) {
    if (MainWindow::Instance()) {
      MainWindow::Instance()->EnableShortcuts(false);
      shortcutsTemporarilyDisabled = true;
    }
  }
  event.Skip();
}

void RiggingPanel::OnItemEditingDone(wxDataViewEvent &event) {
  if (event.GetColumn() == ColumnIndex(RiggingColumn::ExtraWeight) &&
      shortcutsTemporarilyDisabled) {
    if (MainWindow::Instance())
      MainWindow::Instance()->EnableShortcuts(true);
    shortcutsTemporarilyDisabled = false;
  }
  event.Skip();
}

void RiggingPanel::OnItemValueChanged(wxDataViewEvent &event) {
  if (!table)
    return;

  const int editedColumn = event.GetColumn();
  if (editedColumn != ColumnIndex(RiggingColumn::ExtraWeight))
    return;

  const int row = static_cast<int>(table->ItemToRow(event.GetItem()));
  if (row < 0 || static_cast<size_t>(row) >= rowPositions.size())
    return;

  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString rawValue = table->GetTextValue(
      static_cast<unsigned int>(row), ColumnIndex(RiggingColumn::ExtraWeight));
  const auto parsedKg =
      Units::ParseWeightToKilograms(std::string(rawValue.ToUTF8()), weightUnit);
  if (!parsedKg.has_value()) {
    RefreshData();
    return;
  }

  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  const std::string &positionName = rowPositions[static_cast<size_t>(row)];
  const float valueKg = static_cast<float>(*parsedKg);
  if (Units::NearlyEqualWeightKilograms(valueKg, 0.0, 0.0001)) {
    extraWeights.erase(positionName);
  } else {
    RiggingExtraWeightSettings::Entry &entry = extraWeights[positionName];
    entry.valueKg = valueKg;
    entry.requiresValidation = false;
  }

  cfg.SetValue(RiggingExtraWeightSettings::ConfigKey(),
               RiggingExtraWeightSettings::SerializeEntries(extraWeights));

  HoistLoadRecalculationPrompt::PromptAndApply(cfg, this, {positionName});

  RefreshData();
}

void RiggingPanel::OnMouseMove(wxMouseEvent &event) {
  UpdateHoverTooltip(NormalizeMousePositionForTable(table, event));
  event.Skip();
}

void RiggingPanel::OnMouseLeave(wxMouseEvent &event) {
  if (!activeHoverTooltip.IsEmpty()) {
    SetTableAndChildTooltips(table, wxString());
    activeHoverTooltip.clear();
  }
  event.Skip();
}

void RiggingPanel::UpdateHoverTooltip(const wxPoint &position) {
  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(position, item, column);

  wxString tooltip;
  if (item.IsOk() && column) {
    int row = table->ItemToRow(item);
    int modelColumn = column->GetModelColumn();
    if (IsRedCell(store, row, modelColumn))
      tooltip = BuildRiggingTooltipForColumn(modelColumn);
  }

  if (tooltip == activeHoverTooltip)
    return;

  SetTableAndChildTooltips(table, tooltip);
  activeHoverTooltip = tooltip;
}
