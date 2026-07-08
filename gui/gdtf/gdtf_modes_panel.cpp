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
#include "gdtf_modes_panel.h"

#include <utility>

#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Formats a raw GDTF channel function label for display.
std::string FormatGdtfModeFunctionLabel(const std::string &functionText) {
  wxString function = wxString::FromUTF8(functionText);
  while (true) {
    const int sectionStart = function.Find('[');
    if (sectionStart == wxNOT_FOUND)
      break;
    const wxString remainder = function.Mid(static_cast<size_t>(sectionStart));
    const int sectionEndRelative = remainder.Find(']');
    if (sectionEndRelative == wxNOT_FOUND) {
      function = function.Left(static_cast<size_t>(sectionStart));
      break;
    }
    const size_t sectionEnd =
        static_cast<size_t>(sectionStart + sectionEndRelative);
    function = function.Left(static_cast<size_t>(sectionStart)) +
               function.Mid(sectionEnd + 1);
  }
  function.Trim(true).Trim(false);
  if (function.empty())
    function = "-";
  return std::string(function.ToUTF8());
}

// Creates the reusable modes and channels layout.
GdtfModesPanel::GdtfModesPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 5, 5);
  grid->AddGrowableCol(1, 1);
  modeChoice = new wxChoice(this, wxID_ANY);
  channelCountCtrl = new wxTextCtrl(this, wxID_ANY, wxString(),
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY);
  grid->Add(new wxStaticText(this, wxID_ANY, "Mode"), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(modeChoice, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Channel count"), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(channelCountCtrl, 1, wxEXPAND);
  root->Add(grid, 0, wxEXPAND | wxBOTTOM, 6);
  root->Add(new wxStaticText(this, wxID_ANY, "Mode channels"), 0,
            wxBOTTOM, 3);
  channelListCtrl = new wxTextCtrl(this, wxID_ANY, wxString(),
                                   wxDefaultPosition, wxSize(-1, 150),
                                   wxTE_MULTILINE | wxTE_READONLY);
  root->Add(channelListCtrl, 1, wxEXPAND);
  modeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
    NotifyModeChanged();
  });
  SetSizer(root);
}

// Applies a complete modes presentation without notifying the host.
void GdtfModesPanel::SetPresentation(const GdtfModesPresentation &presentation) {
  SetModes(presentation.modes);
  SetSelectedMode(presentation.selectedMode);
  SetChannelCount(presentation.channelCount);
  SetChannels(presentation.channels);
}

// Replaces the ordered mode list without notifying the host.
void GdtfModesPanel::SetModes(const std::vector<std::string> &modes) {
  updating = true;
  modeChoice->Clear();
  for (const auto &mode : modes)
    modeChoice->Append(wxString::FromUTF8(mode));
  updating = false;
}

// Selects a mode without notifying the host.
void GdtfModesPanel::SetSelectedMode(const std::string &mode) {
  updating = true;
  const int selection = modeChoice->FindString(wxString::FromUTF8(mode));
  if (selection != wxNOT_FOUND)
    modeChoice->SetSelection(selection);
  else
    modeChoice->SetSelection(wxNOT_FOUND);
  updating = false;
}

// Returns the selected mode exactly as shown by the choice control.
std::string GdtfModesPanel::GetSelectedMode() const {
  return std::string(modeChoice->GetStringSelection().ToUTF8());
}

// Registers the host mode-selection callback.
void GdtfModesPanel::SetModeSelectionCallback(ModeSelectionCallback callback) {
  selectionCallback = std::move(callback);
}

// Sets the derived channel count text.
void GdtfModesPanel::SetChannelCount(const std::string &channelCount) {
  updating = true;
  channelCountCtrl->SetValue(wxString::FromUTF8(channelCount));
  updating = false;
}

// Sets the ordered channel display rows.
void GdtfModesPanel::SetChannels(
    const std::vector<GdtfModeChannelPresentation> &channels) {
  wxString text;
  for (const auto &channel : channels) {
    text += wxString::FromUTF8(channel.channelLabel) + ": " +
            wxString::FromUTF8(channel.functionLabel) + "\n";
  }
  updating = true;
  channelListCtrl->SetValue(text);
  updating = false;
}

// Clears the derived channel presentation.
void GdtfModesPanel::ClearModeDetails() {
  updating = true;
  channelCountCtrl->SetValue(wxString());
  channelListCtrl->SetValue(wxString());
  updating = false;
}

// Enables or disables mode selection.
void GdtfModesPanel::SetModeSelectionEnabled(bool enabled) {
  modeChoice->Enable(enabled);
}

// Notifies the host about a genuine user mode selection.
void GdtfModesPanel::NotifyModeChanged() {
  if (updating || !selectionCallback)
    return;
  selectionCallback(GetSelectedMode());
}
