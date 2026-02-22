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
#include <string>

#include "colorstore.h"
#include "columnutils.h"
#include "configmanager.h"
#include "guiconfigservices.h"

namespace {
constexpr const char *UNASSIGNED_POSITION = "Unassigned";

float CeilToNearestHalfTen(float value) {
  return std::ceil(value / 5.0f) * 5.0f;
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

wxString BuildRiggingTooltipForColumn(int modelColumn) {
  switch (modelColumn) {
  case 4:
    return "Fixture weight includes zero or missing values in this position.";
  case 5:
    return "Truss weight includes zero or missing values in this position.";
  case 6:
    return "Hoist weight includes zero or missing values in this position.";
  case 7:
  case 8:
    return "Total weight includes items with zero or missing weight.";
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
  table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixtures", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Trusses", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Hoists", wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixture Weight (kg)", wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Truss Weight (kg)", wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Hoists Weight (kg)", wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Total Weight (kg)", wxDATAVIEW_CELL_INERT,
                          wxCOL_WIDTH_AUTOSIZE, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Total Weight +5% (kg)", wxDATAVIEW_CELL_INERT,
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
    bool hasZeroWeightFixture = false;
    bool hasZeroWeightTruss = false;
    bool hasZeroWeightHoist = false;
  };

  std::map<std::string, Totals> rows;
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
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

  // Ensure both the view and the custom store start from a clean state so
  // text colours get recalculated on every refresh.
  store->DeleteAllItems();
  table->DeleteAllItems();
  for (const auto &[position, totals] : rows) {
    float totalWeight =
        totals.fixtureWeight + totals.trussWeight + totals.hoistWeight;
    float roundedFivePercentIncrease = CeilToNearestHalfTen(totalWeight * 1.05f);
    wxVector<wxVariant> row;
    row.push_back(wxString::FromUTF8(position));
    row.push_back(wxString::Format("%d", totals.fixtures));
    row.push_back(wxString::Format("%d", totals.trusses));
    row.push_back(wxString::Format("%d", totals.hoists));
    row.push_back(wxString::Format("%.2f", totals.fixtureWeight));
    row.push_back(wxString::Format("%.2f", totals.trussWeight));
    row.push_back(wxString::Format("%.2f", totals.hoistWeight));
    row.push_back(wxString::Format("%.2f", totalWeight));
    row.push_back(wxString::Format("%.2f", roundedFivePercentIncrease));
    unsigned int rowIndex = table->GetItemCount();
    table->AppendItem(row);

    const bool fixtureWeightZero = totals.hasZeroWeightFixture;
    const bool trussWeightZero = totals.hasZeroWeightTruss;
    const bool hoistWeightZero = totals.hasZeroWeightHoist;

    if (fixtureWeightZero)
      store->SetCellTextColour(rowIndex, 4, *wxRED);
    if (trussWeightZero)
      store->SetCellTextColour(rowIndex, 5, *wxRED);
    if (hoistWeightZero)
      store->SetCellTextColour(rowIndex, 6, *wxRED);

    if (fixtureWeightZero || trussWeightZero || hoistWeightZero) {
      store->SetCellTextColour(rowIndex, 7, *wxRED);
      store->SetCellTextColour(rowIndex, 8, *wxRED);
    }
  }

  AutoSizeColumns(table);

  // Force a repaint so colour changes are visible immediately after the
  // refresh is triggered (e.g. after loading/importing data or editing
  // weights in the tables).
  table->Refresh();
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
