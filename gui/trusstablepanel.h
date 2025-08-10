#pragma once

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/time.h>
#include <vector>
#include <string>
#include "colorstore.h"

class TrussTablePanel : public wxPanel
{
public:
    explicit TrussTablePanel(wxWindow* parent);
    ~TrussTablePanel();
    void ReloadData(); // Refresh from ConfigManager
    void HighlightTruss(const std::string& uuid);
    void ClearSelection();
    std::vector<std::string> GetSelectedUuids() const;
    void SelectByUuid(const std::vector<std::string>& uuids);
    bool IsActivePage() const;
    void DeleteSelected();
    wxDataViewListCtrl* GetTableCtrl() const { return table; }

    static TrussTablePanel* Instance();
    static void SetInstance(TrussTablePanel* panel);

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<std::string> rowUuids;
    std::vector<wxString> modelPaths;  // Displayed model file paths (.gtruss if any)
    std::vector<wxString> symbolPaths; // Resolved geometry file paths
    bool dragSelecting = false;
    int startRow = -1;
    void InitializeTable(); // Set up columns
    void OnSelectionChanged(wxDataViewEvent& evt);
    void UpdateSceneData();
    void OnContextMenu(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void ResyncRows(const std::vector<std::string>& oldOrder,
                    const std::vector<std::string>& selectedUuids);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
};
