#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>

class TrussTablePanel : public wxPanel
{
public:
    explicit TrussTablePanel(wxWindow* parent);
    void ReloadData(); // Refresh from ConfigManager

private:
    wxDataViewListCtrl* table;
    void InitializeTable(); // Set up columns
};
