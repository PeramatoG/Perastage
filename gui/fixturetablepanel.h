#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include "colorstore.h"

class FixtureTablePanel : public wxPanel
{
public:
    FixtureTablePanel(wxWindow* parent);
    ~FixtureTablePanel();
    void ReloadData(); // Refresh content from ConfigManager
    void HighlightFixture(const std::string& uuid);

    static FixtureTablePanel* Instance();
    static void SetInstance(FixtureTablePanel* panel);

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<wxString> gdtfPaths; // Stores full GDTF paths per row
    std::vector<std::string> rowUuids;

    bool dragSelecting = false;
    int startRow = -1;

    void InitializeTable(); // Set up columns
    void OnContextMenu(wxDataViewEvent& event);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void UpdateSceneData();
};
