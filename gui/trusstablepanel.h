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
#include <memory>
#include <string>
#include <unordered_map>
#include "colorstore.h"
#include "positionvalueupdate.h"

namespace gui { class DataViewDeferredSelectionGuard; }

class IGuiConfigServices;
class TrussEditDialog;

class TrussTablePanel : public wxPanel
{
public:
    explicit TrussTablePanel(wxWindow* parent, IGuiConfigServices* services = nullptr);
    ~TrussTablePanel();
    void ReloadData(); // Refresh from ConfigManager
    void HighlightTruss(const std::string& uuid);
    void HighlightTruss(const std::string& uuid,
                        const std::vector<std::string>& relatedUuids);
    void ClearSelection();
    std::vector<std::string> GetSelectedUuids() const;
    void SelectByUuid(const std::vector<std::string>& uuids,
                      bool notifySelectionChanged = true);
    bool IsActivePage() const;
    void DeleteSelected(bool pushUndoState = true);
    void UpdatePositionValues(const std::vector<std::string>& uuids);
    void ApplyPositionValueUpdates(const std::vector<PositionValueUpdate>& updates);
    wxDataViewListCtrl* GetTableCtrl() const { return table; }

    static TrussTablePanel* Instance();
    static void SetInstance(TrussTablePanel* panel);

    void UpdateSceneData(bool logChanges = true);

private:
    ColorfulDataViewListStore* store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<std::string> rowUuids;
    std::vector<std::string> highlightedRelatedUuids;
    std::unordered_map<wxUIntPtr, std::string> rowUuidByKey;
    std::unordered_map<wxUIntPtr, wxString> modelPathByKey;
    std::unordered_map<wxUIntPtr, wxString> symbolPathByKey;
    wxUIntPtr nextRowKey = 1;
    std::string highlightedUuid;
    std::vector<wxString> modelPaths;  // Displayed model file paths (.gtruss if any)
    std::vector<wxString> symbolPaths; // Resolved geometry file paths
    bool dragSelecting = false;
    int startRow = -1;
    IGuiConfigServices *guiConfigServices = nullptr;
    std::unique_ptr<gui::DataViewDeferredSelectionGuard> deferredSelectionGuard;
    void InitializeTable(); // Set up columns
    void OnSelectionChanged(wxDataViewEvent& evt);
    void SyncSelectionFromTable();
    void OnContextMenu(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void RebuildRowCachesFromRowKeys();
    std::string UuidForItem(const wxDataViewItem& item) const;
    void SetModelPathsForRow(unsigned int row, const wxString& modelPath,
                             const wxString& symbolPath);
    void ResyncRows(const std::vector<std::string>& oldOrder,
                    const std::vector<std::string>& selectedUuids);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void OnItemActivated(wxDataViewEvent& event);
    void UpdateSelectionHighlight();

    friend class TrussEditDialog;
};
