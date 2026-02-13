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
#include "summarypanel.h"
#include "columnutils.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include <map>

static SummaryPanel* s_instance = nullptr;

SummaryPanel::SummaryPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
    table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    ColumnUtils::EnforceMinColumnWidth(table);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(table, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

SummaryPanel* SummaryPanel::Instance()
{
    return s_instance;
}

void SummaryPanel::SetInstance(SummaryPanel* panel)
{
    s_instance = panel;
}

void SummaryPanel::ShowSummary(const std::vector<std::pair<std::string,int>>& items)
{
    if (!table) return;
    table->DeleteAllItems();
    for (const auto& [name, count] : items) {
        wxVector<wxVariant> row;
        // Append the count as a string so it renders correctly in the text column
        row.push_back(wxString::Format("%d", count));
        row.push_back(wxString::FromUTF8(name));
        table->AppendItem(row);
    }
}

void SummaryPanel::ShowFixtureSummary()
{
    if (!table) return;

    table->DeleteAllItems();

    std::map<std::string, int> fixtureCounts;
    const auto& fixtures = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().fixtures;
    for (const auto& [uuid, fix] : fixtures)
        fixtureCounts[fix.typeName]++;
    std::vector<std::pair<std::string, int>> fixtureItems(fixtureCounts.begin(), fixtureCounts.end());

    ShowSummary(fixtureItems);
}

void SummaryPanel::ShowTrussSummary()
{
    std::map<std::string,int> counts;
    const auto& trusses = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().trusses;
    for (const auto& [uuid, truss] : trusses)
        counts[truss.model]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowHoistSummary()
{
    if (!table) return;

    std::map<std::string, int> hoistCounts;
    const auto& supports = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().supports;
    for (const auto& [uuid, support] : supports) {
        std::string type = support.function.empty() ? "Hoist" : support.function;
        hoistCounts[type]++;
    }
    std::vector<std::pair<std::string, int>> hoistItems(hoistCounts.begin(), hoistCounts.end());

    ShowSummary(hoistItems);
}

void SummaryPanel::ShowSceneObjectSummary()
{
    std::map<std::string,int> counts;
    const auto& objs = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().sceneObjects;
    for (const auto& [uuid, obj] : objs)
        counts[obj.name]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}
