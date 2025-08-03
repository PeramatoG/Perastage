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
    bool IsActivePage() const;
    void DeleteSelected();

    static TrussTablePanel* Instance();
    static void SetInstance(TrussTablePanel* panel);

private:
    ColorfulDataViewListStore store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<std::string> rowUuids;
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
