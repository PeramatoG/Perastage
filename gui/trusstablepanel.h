#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>

class TrussTablePanel : public wxPanel
{
public:
    explicit TrussTablePanel(wxWindow* parent);
    void ReloadData(); // Refresh from ConfigManager

private:
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    int sortColumn = -1;
    int sortState = 0;
    int lastClickColumn = -1;
    wxMilliClock_t lastClickTime = 0;
    void ApplySort();
    void UpdateColumnHeaders();
    void OnColumnHeaderClick(wxDataViewEvent& event);
    void InitializeTable(); // Set up columns
};
