#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>

class SceneObjectTablePanel : public wxPanel
{
public:
    explicit SceneObjectTablePanel(wxWindow* parent);
    void ReloadData();

private:
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    void InitializeTable();
    void OnContextMenu(wxDataViewEvent& event);
};
