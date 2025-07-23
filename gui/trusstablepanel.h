#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include "colorstore.h"

class TrussTablePanel : public wxPanel
{
public:
    explicit TrussTablePanel(wxWindow* parent);
    void ReloadData(); // Refresh from ConfigManager

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    bool dragSelecting = false;
    int startRow = -1;
    void InitializeTable(); // Set up columns
    void OnContextMenu(wxDataViewEvent& event);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
};
