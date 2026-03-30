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

#include <cmath>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>

#include "colorstore.h"
#include "columnutils.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "hoist_weight_distribution.h"
#include "hoisttablepanel.h"
#include "rigging_extra_weight_settings.h"
#include "units/unit_label_utils.h"
#include "units/units.h"

namespace {
constexpr const char *UNASSIGNED_POSITION = "Unassigned";

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
  switch (modelColumn) {
  case 4:
    return "Fixture weight includes zero or missing values in this position.";
  case 5:
    return "Truss weight includes zero or missing values in this position.";
  case 6:
    return "Hoist weight includes zero or missing values in this position.";
  case 7:
    return "Extra weight was auto-calculated and must be validated by the user.";
  case 8:
  case 9:
    return "Total weight includes values that need attention (missing weights or pending validation).";
  default:
    return wxString();
  }
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
  wxWindow *sourceWindow =
      dynamic_cast<wxWindow *>(event.GetEventObject());
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
}

static RiggingPanel *s_instance = nullptr;

RiggingPanel::RiggingPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  store = new ColorfulDataViewListStore();
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
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
  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString weightSuffix =
      wxString::FromUTF8(Units::WeightUnitSuffix(weightUnit));
  table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixtures", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Trusses", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Hoists", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixture Weight (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Truss Weight (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Hoists Weight (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Extra Weight (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_EDITABLE,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Total Weight (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Rounded Total Weight +5% (" + weightSuffix + ")",
                          wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);

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
    table->GetColumn(4)->SetTitle("Fixture Weight (" + weightSuffix + ")");
    table->GetColumn(5)->SetTitle("Truss Weight (" + weightSuffix + ")");
    table->GetColumn(6)->SetTitle("Hoists Weight (" + weightSuffix + ")");
    table->GetColumn(7)->SetTitle("Extra Weight (" + weightSuffix + ")");
    table->GetColumn(8)->SetTitle("Total Weight (" + weightSuffix + ")");
    table->GetColumn(9)->SetTitle("Rounded Total Weight +5% (" + weightSuffix +
                                  ")");
  }
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto &scene = cfg.GetScene();
  const auto extraWeights =
      RiggingExtraWeightSettings::ParseEntries(
          cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  for (const auto &[uuid, fixture] : scene.fixtures) {
    std::string pos = fixture.positionName.empty() ? UNASSIGNED_POSITION
                                                   : fixture.positionName;
    auto &entry = rows[pos];
    entry.fixtures++;
    entry.fixtureWeight += fixture.weightKg;
    if (fixture.weightKg <= 0.0f)
      entry.hasZeroWeightFixture = true;
  }

  for (const auto &[uuid, truss] : scene.trusses) {
    std::string pos = truss.positionName.empty() ? UNASSIGNED_POSITION
                                                 : truss.positionName;
    auto &entry = rows[pos];
    entry.trusses++;
    entry.trussWeight += truss.weightKg;
    if (truss.weightKg <= 0.0f)
      entry.hasZeroWeightTruss = true;
  }

  for (const auto &[uuid, support] : scene.supports) {
    std::string pos = support.positionName.empty() ? UNASSIGNED_POSITION
                                                   : support.positionName;
    auto &entry = rows[pos];
    entry.hoists++;
    entry.hoistWeight += support.weightKg;
    if (support.weightKg <= 0.0f)
      entry.hasZeroWeightHoist = true;
  }

  for (auto &[position, totals] : rows) {
    if (const auto it = extraWeights.find(position); it != extraWeights.end()) {
      totals.extraWeight = it->second.valueKg;
      totals.hasAutoUnvalidatedExtraWeight = it->second.requiresValidation;
    }
  }

  // Ensure both the view and the custom store start from a clean state so
  // text colours get recalculated on every refresh.
  store->DeleteAllItems();
  table->DeleteAllItems();
  rowPositions.clear();
  rowPositions.reserve(rows.size());
  for (const auto &[position, totals] : rows) {
    float totalWeight =
        totals.fixtureWeight + totals.trussWeight + totals.hoistWeight +
        totals.extraWeight;
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
    row.push_back(wxString::FromUTF8(Units::FormatWeightFromKilograms(
        roundedFivePercentIncrease, weightUnit,
        Units::ValueFormatContext::Table)));
    unsigned int rowIndex = table->GetItemCount();
    table->AppendItem(row);
    rowPositions.push_back(position);

    const bool fixtureWeightZero = totals.hasZeroWeightFixture;
    const bool trussWeightZero = totals.hasZeroWeightTruss;
    const bool hoistWeightZero = totals.hasZeroWeightHoist;

    if (fixtureWeightZero)
      store->SetCellTextColour(rowIndex, 4, *wxRED);
    if (trussWeightZero)
      store->SetCellTextColour(rowIndex, 5, *wxRED);
    if (hoistWeightZero)
      store->SetCellTextColour(rowIndex, 6, *wxRED);

    if (totals.hasAutoUnvalidatedExtraWeight)
      store->SetCellTextColour(rowIndex, 7, *wxRED);

    if (fixtureWeightZero || trussWeightZero || hoistWeightZero) {
      store->SetCellTextColour(rowIndex, 8, *wxRED);
      store->SetCellTextColour(rowIndex, 9, *wxRED);
    }
    if (totals.hasAutoUnvalidatedExtraWeight) {
      store->SetCellTextColour(rowIndex, 8, *wxRED);
      store->SetCellTextColour(rowIndex, 9, *wxRED);
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

  if (event.GetColumn() != 7) {
    event.Skip();
    return;
  }

  table->EditItem(event.GetItem(), table->GetColumn(7));
  event.Skip();
}

void RiggingPanel::OnItemValueChanged(wxDataViewEvent &event) {
  if (!table)
    return;

  const int editedColumn = event.GetColumn();
  if (editedColumn != 7)
    return;

  const int row = static_cast<int>(table->ItemToRow(event.GetItem()));
  if (row < 0 || static_cast<size_t>(row) >= rowPositions.size())
    return;

  const auto weightUnit = ResolveWeightUnitSystem();
  const wxString rawValue =
      table->GetTextValue(static_cast<unsigned int>(row), 7);
  const auto parsedKg = Units::ParseWeightToKilograms(
      std::string(rawValue.ToUTF8()), weightUnit);
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

  auto &scene = cfg.GetScene();
  std::vector<std::string> supportsInPosition;
  supportsInPosition.reserve(scene.supports.size());
  for (const auto &[supportUuid, support] : scene.supports) {
    const std::string supportPosition = support.positionName.empty()
                                            ? UNASSIGNED_POSITION
                                            : support.positionName;
    if (supportPosition == positionName)
      supportsInPosition.push_back(supportUuid);
  }

  if (!supportsInPosition.empty()) {
    const auto roundedTotalsByPosition =
        HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(
            scene, RiggingExtraWeightSettings::BuildKilogramsByPosition(
                       extraWeights));
    HoistWeightDistribution::ApplyForImportedSupports(
        scene, supportsInPosition, roundedTotalsByPosition);
    if (HoistTablePanel::Instance())
      HoistTablePanel::Instance()->ReloadData();
  }

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
