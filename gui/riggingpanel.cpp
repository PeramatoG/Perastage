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

#include <map>
#include <string>

#include "colorstore.h"
#include "columnutils.h"
#include "configmanager.h"

namespace {
constexpr const char *UNASSIGNED_POSITION = "Unassigned";
}

static RiggingPanel *s_instance = nullptr;

RiggingPanel::RiggingPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  store = new ColorfulDataViewListStore();
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxDV_ROW_LINES | wxDV_VERT_RULES);
  table->AssociateModel(store);
  store->DecRef();
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
  const auto &scene = ConfigManager::Get().GetScene();
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
    wxVector<wxVariant> row;
    row.push_back(wxString::FromUTF8(position));
    row.push_back(wxString::Format("%d", totals.fixtures));
    row.push_back(wxString::Format("%d", totals.trusses));
    row.push_back(wxString::Format("%d", totals.hoists));
    row.push_back(wxString::Format("%.2f", totals.fixtureWeight));
    row.push_back(wxString::Format("%.2f", totals.trussWeight));
    row.push_back(wxString::Format("%.2f", totals.hoistWeight));
    row.push_back(wxString::Format("%.2f", totalWeight));
    unsigned int rowIndex = table->GetItemCount();
    table->AppendItem(row);

    const bool fixtureCountZero = totals.fixtures == 0;
    const bool trussCountZero = totals.trusses == 0;
    const bool hoistCountZero = totals.hoists == 0;
    const bool fixtureWeightZero =
        totals.fixtureWeight <= 0.0f || totals.hasZeroWeightFixture;
    const bool trussWeightZero =
        totals.trussWeight <= 0.0f || totals.hasZeroWeightTruss;
    const bool hoistWeightZero =
        totals.hoistWeight <= 0.0f || totals.hasZeroWeightHoist;

    if (fixtureCountZero)
      store->SetCellTextColour(rowIndex, 1, *wxRED);
    if (trussCountZero)
      store->SetCellTextColour(rowIndex, 2, *wxRED);
    if (hoistCountZero)
      store->SetCellTextColour(rowIndex, 3, *wxRED);
    if (fixtureWeightZero)
      store->SetCellTextColour(rowIndex, 4, *wxRED);
    if (trussWeightZero)
      store->SetCellTextColour(rowIndex, 5, *wxRED);
    if (hoistWeightZero)
      store->SetCellTextColour(rowIndex, 6, *wxRED);

    if (fixtureCountZero || trussCountZero || hoistCountZero ||
        fixtureWeightZero || trussWeightZero || hoistWeightZero)
      store->SetCellTextColour(rowIndex, 7, *wxRED);
  }

  AutoSizeColumns(table);

  // Force a repaint so colour changes are visible immediately after the
  // refresh is triggered (e.g. after loading/importing data or editing
  // weights in the tables).
  table->Refresh();
}
