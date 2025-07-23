#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include "colorstore.h"

class SceneObjectTablePanel : public wxPanel
{
public:
    explicit SceneObjectTablePanel(wxWindow* parent);
    void ReloadData();

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    bool dragSelecting = false;
    int startRow = -1;
    void InitializeTable();
    void OnContextMenu(wxDataViewEvent& event);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
};
