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
    void InitializeTable(); // Set up columns
    void OnContextMenu(wxDataViewEvent& event);
};
