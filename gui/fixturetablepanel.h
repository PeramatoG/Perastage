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
    int sortColumn = -1;      // Currently sorted column
    int sortState = 0;        // 0 none, 1 asc, -1 desc
    int lastClickColumn = -1; // For detecting double click
    wxMilliClock_t lastClickTime = 0;
    void ApplySort();
    void UpdateColumnHeaders();
    void OnColumnHeaderClick(wxDataViewEvent& event);
    void InitializeTable(); // Set up columns
};
