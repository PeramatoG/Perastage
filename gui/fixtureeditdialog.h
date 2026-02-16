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
#include <wx/spinctrl.h>
#include <vector>
#include <string>

class FixtureTablePanel;
class FixturePreviewPanel;
class wxSlider;
class wxSpinCtrlDouble;
class wxStaticText;

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
    void OnMotionControlChanged(wxCommandEvent& evt);
    void OnMotionSliderChanged(wxCommandEvent& evt);
    void OnMotionLimitChanged(wxSpinDoubleEvent& evt);
    void OnResetMotion(wxCommandEvent& evt);
    void UpdateChannels();
    void SyncMotionControlsFromFixture();
    void SyncMotionControlsToUi();
    void ApplyMotionToFixture();
    void RefreshPreviewMotion();
    void ApplyChanges();

    FixtureTablePanel* panel;
    int row;
    std::vector<wxControl*> ctrls;
    wxChoice* modeChoice = nullptr;
    wxTextCtrl* chCountCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxTextCtrl* channelList = nullptr;
    wxSlider* panSlider = nullptr;
    wxSlider* tiltSlider = nullptr;
    wxSlider* dimmerSlider = nullptr;
    wxTextCtrl* panCtrl = nullptr;
    wxTextCtrl* tiltCtrl = nullptr;
    wxTextCtrl* dimmerCtrl = nullptr;
    wxSpinCtrlDouble* panLimitCtrl = nullptr;
    wxSpinCtrlDouble* tiltLimitCtrl = nullptr;
    wxStaticText* emitterDebugLabel = nullptr;
    double panLimitDeg = 270.0;
    double tiltLimitDeg = 135.0;
    bool syncingMotionUi = false;
    std::string editedFixtureUuid;
    FixturePreviewPanel* preview = nullptr;
    bool applied = false;
    wxString originalType;
};
