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

#include "configmanager.h"

namespace {
constexpr const char *UNASSIGNED_POSITION = "Unassigned";
}

static RiggingPanel *s_instance = nullptr;

RiggingPanel::RiggingPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxDV_ROW_LINES | wxDV_VERT_RULES);
  table->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, 160, wxALIGN_LEFT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixtures", wxDATAVIEW_CELL_INERT, 80, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Trusses", wxDATAVIEW_CELL_INERT, 80, wxALIGN_RIGHT,
                          wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Fixture Weight (kg)", wxDATAVIEW_CELL_INERT, 150,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Truss Weight (kg)", wxDATAVIEW_CELL_INERT, 150,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
  table->AppendTextColumn("Total Weight (kg)", wxDATAVIEW_CELL_INERT, 150,
                          wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);

  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(table, 1, wxEXPAND | wxALL, 5);
  SetSizer(sizer);
}

RiggingPanel *RiggingPanel::Instance() { return s_instance; }

void RiggingPanel::SetInstance(RiggingPanel *panel) { s_instance = panel; }

void RiggingPanel::RefreshData() {
  if (!table)
    return;

  struct Totals {
    int fixtures = 0;
    int trusses = 0;
    float fixtureWeight = 0.0f;
    float trussWeight = 0.0f;
  };

  std::map<std::string, Totals> rows;
  const auto &scene = ConfigManager::Get().GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    std::string pos = fixture.positionName.empty() ? UNASSIGNED_POSITION
                                                   : fixture.positionName;
    auto &entry = rows[pos];
    entry.fixtures++;
    entry.fixtureWeight += fixture.weightKg;
  }

  for (const auto &[uuid, truss] : scene.trusses) {
    std::string pos = truss.positionName.empty() ? UNASSIGNED_POSITION
                                                 : truss.positionName;
    auto &entry = rows[pos];
    entry.trusses++;
    entry.trussWeight += truss.weightKg;
  }

  table->DeleteAllItems();
  for (const auto &[position, totals] : rows) {
    float totalWeight = totals.fixtureWeight + totals.trussWeight;
    wxVector<wxVariant> row;
    row.push_back(wxString::FromUTF8(position));
    row.push_back(wxString::Format("%d", totals.fixtures));
    row.push_back(wxString::Format("%d", totals.trusses));
    row.push_back(wxString::Format("%.2f", totals.fixtureWeight));
    row.push_back(wxString::Format("%.2f", totals.trussWeight));
    row.push_back(wxString::Format("%.2f", totalWeight));
    table->AppendItem(row);
  }
}
