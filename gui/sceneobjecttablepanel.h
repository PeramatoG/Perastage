#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include <string>
#include "colorstore.h"

class SceneObjectTablePanel : public wxPanel
{
public:
    explicit SceneObjectTablePanel(wxWindow* parent);
    ~SceneObjectTablePanel();
    void ReloadData();
    void HighlightObject(const std::string& uuid);
    bool IsActivePage() const;
    void DeleteSelected();

    static SceneObjectTablePanel* Instance();
    static void SetInstance(SceneObjectTablePanel* panel);

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<std::string> rowUuids;
    bool dragSelecting = false;
    int startRow = -1;
    void InitializeTable();
    void OnSelectionChanged(wxDataViewEvent& evt);
    void UpdateSceneData();
    void OnContextMenu(wxDataViewEvent& event);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
};
