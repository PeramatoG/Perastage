#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>

class FixtureTablePanel : public wxPanel
{
public:
    FixtureTablePanel(wxWindow* parent);
    void ReloadData(); // Refresh content from ConfigManager

private:
    wxDataViewListCtrl* table;
    void InitializeTable(); // Set up columns
};
