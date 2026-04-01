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
#include "symbols/PerastageSvgSymbol.h"

class FixtureTablePanel;
class FixturePreviewPanel;
class wxStaticBitmap;
class wxPanel;

class FixtureEditDialog : public wxDialog {
public:
    FixtureEditDialog(FixtureTablePanel* panel, int row);
    bool WasApplied() const { return applied; }

private:
    void OnApply(wxCommandEvent& evt);
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnBrowse(wxCommandEvent& evt);
    void OnModeChanged(wxCommandEvent& evt);
    void OnSymbolPreviewPaint(wxPaintEvent& evt);
    void UpdateChannels();
    void UpdateVisualizers();
    void ApplyChanges();

    FixtureTablePanel* panel;
    int row;
    std::vector<wxControl*> ctrls;
    wxChoice* modeChoice = nullptr;
    wxTextCtrl* chCountCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxTextCtrl* channelList = nullptr;
    FixturePreviewPanel* preview = nullptr;
    wxStaticBitmap* fixtureImagePreview = nullptr;
    std::array<wxPanel*, 3> symbolPanels{};
    std::array<bool, 3> symbolAvailability{};
    std::array<PerastageSvgSymbolData, 3> symbolData{};
    bool applied = false;
    wxString originalType;
    float originalPowerW = 0.0f;
    float originalWeightKg = 0.0f;
};
