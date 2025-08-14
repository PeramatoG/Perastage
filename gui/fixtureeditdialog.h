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

class FixtureTablePanel;
class FixturePreviewPanel;

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
    void UpdateChannels();
    void ApplyChanges();

    FixtureTablePanel* panel;
    int row;
    std::vector<wxControl*> ctrls;
    wxChoice* modeChoice = nullptr;
    wxTextCtrl* chCountCtrl = nullptr;
    wxTextCtrl* modelCtrl = nullptr;
    wxTextCtrl* channelList = nullptr;
    FixturePreviewPanel* preview = nullptr;
    bool applied = false;
    wxString originalType;
};

