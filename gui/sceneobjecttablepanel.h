#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>

class SceneObjectTablePanel : public wxPanel
{
public:
    explicit SceneObjectTablePanel(wxWindow* parent);
    void ReloadData();

private:
    wxDataViewListCtrl* table;
    void InitializeTable();
};