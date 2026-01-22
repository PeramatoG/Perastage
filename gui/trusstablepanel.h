/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
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
    void UpdatePositionValues(const std::vector<std::string>& uuids);
    wxDataViewListCtrl* GetTableCtrl() const { return table; }

    static TrussTablePanel* Instance();
    static void SetInstance(TrussTablePanel* panel);

    void UpdateSceneData();

private:
    ColorfulDataViewListStore* store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<std::string> rowUuids;
    std::vector<wxString> modelPaths;  // Displayed model file paths (.gtruss if any)
    std::vector<wxString> symbolPaths; // Resolved geometry file paths
    bool dragSelecting = false;
    int startRow = -1;
    void InitializeTable(); // Set up columns
    void OnSelectionChanged(wxDataViewEvent& evt);
    void OnContextMenu(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void ResyncRows(const std::vector<std::string>& oldOrder,
                    const std::vector<std::string>& selectedUuids);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void UpdateSelectionHighlight();
};
