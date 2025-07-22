#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>

class FixtureTablePanel : public wxPanel
{
public:
    FixtureTablePanel(wxWindow* parent);
    void ReloadData(); // Refresh content from ConfigManager

private:
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    void InitializeTable(); // Set up columns

    void OnColumnHeaderClick(wxDataViewEvent& event);
    void SortByAddress(bool ascending);
    bool addrSortAscending = true;

    wxDECLARE_EVENT_TABLE();
};
