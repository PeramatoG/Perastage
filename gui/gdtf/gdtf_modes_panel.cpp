/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "gdtf_modes_panel.h"
#include "gdtf/gdtf_editor_visual_metrics.h"
#include "gdtf/gdtf_mode_data_view_model.h"

#include <algorithm>
#include <utility>

#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
// Clamps the browser splitter ratio to a usable normalized range.
double ClampBrowserRatio(double ratio) { return std::clamp(ratio, 0.25, 0.85); }
}

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
  root->Add(new wxStaticText(this, wxID_ANY, "Mode and channel browser"), 0,
            wxBOTTOM, 3);

  browserSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
                                         wxSize(-1, gui::gdtf_layout::Dip(this, 300)),
                                         wxSP_LIVE_UPDATE | wxSP_3DSASH);
  browserCtrl = new wxDataViewCtrl(browserSplitter, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES);
  browserCtrl->AppendTextColumn("Item", GdtfModeDataViewModel::Item, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 180), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("Channel name", GdtfModeDataViewModel::Address, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 120), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("DMX range", GdtfModeDataViewModel::DmxRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 90), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("Physical range", GdtfModeDataViewModel::PhysicalRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 130), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("Unit", GdtfModeDataViewModel::Unit, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 80), wxALIGN_LEFT, 0);
  browserModel = new GdtfModeDataViewModel();
  browserCtrl->AssociateModel(browserModel);
  browserModel->DecRef();
  detailsCtrl = new wxTextCtrl(browserSplitter, wxID_ANY, wxString(), wxDefaultPosition,
                               wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  browserCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 180)));
  detailsCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 90)));
  browserSplitter->SplitHorizontally(browserCtrl, detailsCtrl,
      gui::gdtf_layout::Dip(this, 204));
  root->Add(browserSplitter, 1, wxEXPAND);

  modeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { NotifyModeChanged(); });
  browserCtrl->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &event) {
    const auto *node = browserModel ? browserModel->GetNode(event.GetItem()) : nullptr;
    if (node)
      selectedByMode[GetSelectedMode()] = node->id;
    UpdateDetails(node);
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

// Ignores legacy summary rows because they are displayed by GdtfChannelSummaryPanel.
void GdtfModesPanel::SetChannels(const std::vector<GdtfModeChannelPresentation> &) {}

// Sets the ordered read-only hierarchical browser nodes.
void GdtfModesPanel::SetBrowserNodes(const std::vector<GdtfModeBrowserNodePresentation> &nodes) {
  RememberExpandedItems();
  updating = true;
  browserModel->SetNodes(nodes);
  RestoreExpandedItems();
  wxDataViewItem selected;
  auto selectedIt = selectedByMode.find(GetSelectedMode());
  if (selectedIt != selectedByMode.end())
    selected = browserModel->GetItemById(selectedIt->second);
  if (!selected.IsOk()) {
    auto roots = browserModel->GetTopLevelItems();
    if (!roots.empty())
      selected = roots.front();
  }
  if (selected.IsOk())
    browserCtrl->Select(selected);
  UpdateDetails(browserModel->GetNode(selected));
  updating = false;
}

// Sets the normalized browser/details splitter ratio.
void GdtfModesPanel::SetBrowserSplitterRatio(double ratio) {
  browserSplitterRatio = ClampBrowserRatio(ratio);
  if (!browserSplitter)
    return;
  const int height = browserSplitter->GetClientSize().GetHeight();
  if (height > 0)
    browserSplitter->SetSashPosition(static_cast<int>(height * browserSplitterRatio));
}

// Returns the current normalized browser/details splitter ratio.
double GdtfModesPanel::GetBrowserSplitterRatio() const {
  if (!browserSplitter)
    return browserSplitterRatio;
  const int height = browserSplitter->GetClientSize().GetHeight();
  if (height <= 0)
    return browserSplitterRatio;
  return ClampBrowserRatio(static_cast<double>(browserSplitter->GetSashPosition()) / height);
}

// Clears the derived channel presentation.
void GdtfModesPanel::ClearModeDetails() {
  updating = true;
  channelCountCtrl->SetValue(wxString());
  browserModel->SetNodes({});
  detailsCtrl->SetValue(wxString());
  updating = false;
}

// Enables or disables mode selection.
void GdtfModesPanel::SetModeSelectionEnabled(bool enabled) { modeChoice->Enable(enabled); }

// Notifies the host about a genuine user mode selection.
void GdtfModesPanel::NotifyModeChanged() {
  if (updating || !selectionCallback)
    return;
  RememberExpandedItems();
  selectionCallback(GetSelectedMode());
}

// Updates the read-only key/value details inspector for the selected node.
void GdtfModesPanel::UpdateDetails(const GdtfModeBrowserNodePresentation *node) {
  wxString text;
  if (node) {
    for (const auto &row : node->details)
      text += wxString::FromUTF8(row.key) + ": " + wxString::FromUTF8(row.value) + "\n";
  }
  detailsCtrl->SetValue(text);
}

// Remembers expanded browser item identities for the active mode.
void GdtfModesPanel::RememberExpandedItems() {
  if (!browserModel || !browserCtrl)
    return;
  auto &expanded = expandedByMode[GetSelectedMode()];
  expanded.clear();
  wxDataViewItemArray children;
  browserModel->GetChildren(wxDataViewItem(), children);
  for (auto item : children) {
    if (browserCtrl->IsExpanded(item)) {
      if (const auto *node = browserModel->GetNode(item))
        expanded.insert(node->id);
    }
  }
}

// Restores mode-specific expansion or expands DMX Channel roots initially.
void GdtfModesPanel::RestoreExpandedItems() {
  if (!browserModel || !browserCtrl)
    return;
  const auto mode = GetSelectedMode();
  const auto it = expandedByMode.find(mode);
  for (auto item : browserModel->GetTopLevelItems()) {
    const auto *node = browserModel->GetNode(item);
    if (it == expandedByMode.end() || (node && it->second.count(node->id)))
      browserCtrl->Expand(item);
  }
}
