#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include <string>
#include "colorstore.h"

class FixtureTablePanel : public wxPanel
{
public:
    FixtureTablePanel(wxWindow* parent);
    ~FixtureTablePanel();
    void ReloadData(); // Refresh content from ConfigManager
    void HighlightFixture(const std::string& uuid);
    void ClearSelection();
    std::vector<std::string> GetSelectedUuids() const;
    void SelectByUuid(const std::vector<std::string>& uuids);
    bool IsActivePage() const;
    void DeleteSelected();
    wxDataViewListCtrl* GetTableCtrl() const { return table; }

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
    std::vector<int> selectionOrder;

    void InitializeTable(); // Set up columns
    void OnContextMenu(wxDataViewEvent& event);
    void OnItemActivated(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void ResyncRows(const std::vector<std::string>& oldOrder,
                    const std::vector<std::string>& selectedUuids);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnSelectionChanged(wxDataViewEvent& evt);
    void PropagateTypeValues(const wxDataViewItemArray& selections, int col);
    void UpdateSceneData();
    void ApplyModeForGdtf(const wxString& path);
    void HighlightDuplicateFixtureIds();
    void HighlightPatchConflicts();
};
