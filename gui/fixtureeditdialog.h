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
#include <vector>
#include <string>
#include <array>
#include <filesystem>
#include <memory>
#include <map>
#include "gdtf/editor/gdtf_field_registry.h"
#include "gdtf/gdtf_mode_channel_browser.h"
#include "gdtf/gdtf_wheel_catalog.h"
#include "gdtf/gdtf_resource_bitmap_cache.h"
#include "gdtf/gdtf_wheel_inspector_panel.h"
#include "symbols/PerastageSvgSymbol.h"

class FixtureTablePanel;
class FixturePreviewPanel;
class wxStaticBitmap;
class wxPanel;
class wxNotebook;
class wxSplitterWindow;
class GdtfChannelSummaryPanel;
class GdtfEditorPanel;
namespace gdtf { class GdtfEditSession; }

class FixtureEditDialog : public wxDialog {
public:
    FixtureEditDialog(FixtureTablePanel* panel, int row);
    ~FixtureEditDialog() override;
    bool WasApplied() const { return applied; }

private:
    void MarkColumnModified(size_t index);
    void OnApply(wxCommandEvent& evt);
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnBrowse(wxCommandEvent& evt);
    void OnModeChanged(wxCommandEvent& evt);
    void OnSymbolPreviewPaint(wxPaintEvent& evt);
    void UpdateChannels(bool markChannelCountDirty = false);
    void UpdateVisualizers();
    void UpdateMetadataSummary();
    void ReloadModeChannelDocument();
    bool ApplyChanges();
    void BuildEditSession();
    std::filesystem::path GetActiveResolvedGdtfPath() const;
    void SyncSessionDirtyToLegacyFlags();
    bool SetSessionValue(gdtf::GdtfFieldId fieldId, const std::string& value);
    bool ValidateSessionBeforeApply();
    void ClearSessionValidation();
    void SaveLayoutPreferences();
    void RestoreLayoutPreferences();
    GdtfWheelInspectorPresentation BuildWheelInspectorVisualPresentation(
        const GdtfWheelInspectorPresentation &presentation);

    FixtureTablePanel* panel;
    int row;
    std::vector<wxControl*> ctrls;
    GdtfChannelSummaryPanel* fixtureChannelSummaryPanel = nullptr;
    GdtfEditorPanel* gdtfEditorPanel = nullptr;
    GdtfWheelInspectorPanel* gdtfWheelInspectorPanel = nullptr;
    std::unique_ptr<gdtf::GdtfEditSession> gdtfEditSession;
    FixturePreviewPanel* preview = nullptr;
    wxStaticBitmap* fixtureImagePreview = nullptr;
    wxStaticBitmap* officialSymbolPreview = nullptr;
    wxSplitterWindow* contextSplitter = nullptr;
    wxSplitterWindow* visualSplitter = nullptr;
    wxNotebook* visualNotebook = nullptr;
    std::array<wxPanel*, 3> symbolPanels{};
    std::array<bool, 3> symbolAvailability{};
    std::array<PerastageSvgSymbolData, 3> symbolData{};
    bool applied = false;
    std::map<gdtf::GdtfFieldId, std::string> rejectedSessionInputs;
    std::filesystem::path pendingSelectedGdtfPath;
    std::filesystem::path cachedModeChannelSource;
    gdtf::GdtfModeChannelDocument cachedModeChannelDocument;
    gdtf::GdtfWheelCatalog cachedWheelCatalog;
    GdtfResourceBitmapCache wheelBitmapCache;
    wxString originalType;
    float originalPowerW = 0.0f;
    float originalWeightKg = 0.0f;
    std::vector<bool> modifiedColumns;
};
