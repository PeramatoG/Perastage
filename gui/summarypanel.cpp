#include "summarypanel.h"
#include "configmanager.h"
#include <map>

static SummaryPanel* s_instance = nullptr;

SummaryPanel::SummaryPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    table = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
    table->AppendTextColumn("Count", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    table->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
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
