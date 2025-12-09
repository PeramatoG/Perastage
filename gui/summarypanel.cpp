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
#include <map>

static SummaryPanel* s_instance = nullptr;

SummaryPanel::SummaryPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
    ResetColumns({"Count", "Type"});
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

void SummaryPanel::ResetColumns(const std::vector<wxString>& headers)
{
    if (!table)
        return;

    while (table->GetColumnCount() > 0)
        table->DeleteColumn(0);

    for (size_t i = 0; i < headers.size(); ++i) {
        int width = (i == 0) ? 80 : 150;
        table->AppendTextColumn(headers[i], wxDATAVIEW_CELL_INERT, width, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
    }
    ColumnUtils::EnforceMinColumnWidth(table);
}

void SummaryPanel::ShowSummary(const std::vector<std::pair<std::string,int>>& items)
{
    if (!table) return;
    ResetColumns({"Count", "Type"});
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
    std::map<std::string,int> counts;
    const auto& fixtures = ConfigManager::Get().GetScene().fixtures;
    for (const auto& [uuid, fix] : fixtures)
        counts[fix.typeName]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowTrussSummary()
{
    std::map<std::string,int> counts;
    const auto& trusses = ConfigManager::Get().GetScene().trusses;
    for (const auto& [uuid, truss] : trusses)
        counts[truss.model]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowSceneObjectSummary()
{
    std::map<std::string,int> counts;
    const auto& objs = ConfigManager::Get().GetScene().sceneObjects;
    for (const auto& [uuid, obj] : objs)
        counts[obj.name]++;
    std::vector<std::pair<std::string,int>> items(counts.begin(), counts.end());
    ShowSummary(items);
}

void SummaryPanel::ShowSupportSummary()
{
    if (!table) return;

    struct Totals {
        int count = 0;
        float weight = 0.0f;
        float capacity = 0.0f;
    };

    std::map<std::string, Totals> data;
    const auto& supports = ConfigManager::Get().GetScene().supports;
    for (const auto& [uuid, support] : supports) {
        std::string key = support.function;
        if (key.empty())
            key = support.gdtfSpec.empty() ? "Unknown" : support.gdtfSpec;

        auto& entry = data[key];
        entry.count++;
        entry.weight += support.weightKg;
        entry.capacity += support.capacityKg;
    }

    ResetColumns({"Count", "Type", "Weight (kg)", "Capacity (kg)"});
    table->DeleteAllItems();

    int totalCount = 0;
    float totalWeight = 0.0f;
    float totalCapacity = 0.0f;

    for (const auto& [name, totals] : data) {
        wxVector<wxVariant> row;
        row.push_back(wxString::Format("%d", totals.count));
        row.push_back(wxString::FromUTF8(name));
        row.push_back(wxString::Format("%.2f", totals.weight));
        row.push_back(wxString::Format("%.2f", totals.capacity));
        table->AppendItem(row);

        totalCount += totals.count;
        totalWeight += totals.weight;
        totalCapacity += totals.capacity;
    }

    // Append a total row for quick reference
    wxVector<wxVariant> totalRow;
    totalRow.push_back(wxString::Format("%d", totalCount));
    totalRow.push_back("Total");
    totalRow.push_back(wxString::Format("%.2f", totalWeight));
    totalRow.push_back(wxString::Format("%.2f", totalCapacity));
    table->AppendItem(totalRow);
}
