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
#include <unordered_map>
#include <unordered_set>
#include "colorstore.h"
#include "positionvalueupdate.h"

class FixtureEditDialog; // forward declaration
class IGuiConfigServices;

class FixtureTablePanel : public wxPanel
{
public:
    // SceneDataUpdateType groups edits by likely subsystem impact:
    // - kVisualLabelOnly: fixture naming/labels (viewer refresh, no scene rebuild).
    // - kPatchOnly: DMX addressing/patching (patch conflict checks, scene + rigging refresh).
    // - kAppearanceOnly: visual look (fixture color/swatch and related rendering refresh).
    // - kCategoryOnly: fixture classification/category-dependent UI summaries.
    // - kTransformOnly: orientation/transform values (viewer scene transforms).
    // - kWeightOrPosition: physical load/position values (hoist/rigging recalculation).
    // - kMetadataOnly: fixture descriptive metadata (type, mode, layer, model, hang position).
    // - kGeneral: mixed or broad edits spanning multiple categories.
    enum class SceneDataUpdateType {
        kGeneral,
        kVisualLabelOnly,
        kPatchOnly,
        kAppearanceOnly,
        kCategoryOnly,
        kTransformOnly,
        kWeightOrPosition,
        kMetadataOnly,
        kFixtureIdOnly
    };

    explicit FixtureTablePanel(wxWindow* parent, IGuiConfigServices* services = nullptr);
    ~FixtureTablePanel();
    void ReloadData(); // Refresh content from ConfigManager
    void HighlightFixture(const std::string& uuid);
    void HighlightFixture(const std::string& uuid,
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

    static FixtureTablePanel* Instance();
    static void SetInstance(FixtureTablePanel* panel);
    static SceneDataUpdateType UpdateTypeForColumn(int column);
    static SceneDataUpdateType CombineUpdateTypes(SceneDataUpdateType lhs,
                                                  SceneDataUpdateType rhs);
    static bool RequiresFullViewerSceneUpdate(SceneDataUpdateType updateType);

    void UpdateSceneData(
        bool logChanges = true,
        SceneDataUpdateType updateType = SceneDataUpdateType::kGeneral,
        const std::vector<unsigned int> *targetRows = nullptr);

private:
    friend class FixtureEditDialog; // allow dialog to access internals

    ColorfulDataViewListStore* store;
    wxDataViewListCtrl* table;
    std::vector<wxString> columnLabels;
    std::vector<wxString> gdtfPaths; // Stores full GDTF paths per row
    std::vector<std::string> rowUuids;
    std::vector<std::string> highlightedRelatedUuids;
    std::unordered_map<wxUIntPtr, std::string> rowUuidByKey;
    std::unordered_map<wxUIntPtr, wxString> gdtfPathByKey;
    wxUIntPtr nextRowKey = 1;
    std::string highlightedUuid;

    bool dragSelecting = false;
    int startRow = -1;
    std::vector<std::string> selectionOrderUuids;
    std::unordered_set<std::string> manualCategoryUuidsPending;
    IGuiConfigServices *guiConfigServices = nullptr;

    void InitializeTable(); // Set up columns
    void OnContextMenu(wxDataViewEvent& event);
    void OnCellEditRequested(wxDataViewEvent& event);
    void OnItemActivated(wxDataViewEvent& event);
    void OnColumnSorted(wxDataViewEvent& event);
    void RebuildRowCachesFromRowKeys();
    std::string UuidForItem(const wxDataViewItem& item) const;
    void SetGdtfPathForRow(unsigned int row, const wxString& path);
    void ResyncRows(const std::vector<std::string>& oldOrder,
                    const std::vector<std::string>& selectedUuids,
                    const std::vector<wxString>* oldPaths = nullptr);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftDClick(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseLeave(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void OnSelectionChanged(wxDataViewEvent& evt);
    void UpdateHoverTooltip(const wxPoint& position);
    void UpdateSelectionHighlight();
    void PropagateTypeValues(const wxDataViewItemArray& selections, int col);
    void ApplyModeForGdtf(const wxString& path, const wxString& preferredMode = wxString());
    void HighlightDuplicateFixtureIds();
    void HighlightPatchConflicts();
    void HighlightAutoFallbackCategories();
    void RunValidationHighlights(SceneDataUpdateType updateType);

    wxString activeHoverTooltip;
};
