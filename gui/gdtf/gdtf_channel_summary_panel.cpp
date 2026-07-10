/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "gdtf/gdtf_channel_summary_panel.h"

#include "gdtf/gdtf_editor_visual_metrics.h"

#include <wx/sizer.h>
#include <wx/textctrl.h>

// Creates the compact read-only channel summary panel.
GdtfChannelSummaryPanel::GdtfChannelSummaryPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  channelListCtrl = new wxTextCtrl(
      this, wxID_ANY, wxString(), wxDefaultPosition,
      wxSize(-1, gui::gdtf_layout::Dip(this, 150)),
      wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  channelListCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 120)));
  root->Add(channelListCtrl, 1, wxEXPAND);
  SetSizer(root);
}

// Sets the quick legacy-style mode channel summary text.
void GdtfChannelSummaryPanel::SetChannels(
    const std::vector<GdtfModeChannelPresentation> &channels) {
  wxString text;
  for (const auto &channel : channels) {
    text += wxString::FromUTF8(channel.channelLabel) + ": " +
            wxString::FromUTF8(channel.functionLabel) + "\n";
  }
  channelListCtrl->SetValue(text);
}

// Clears the quick channel summary text.
void GdtfChannelSummaryPanel::ClearChannels() {
  channelListCtrl->SetValue(wxString());
}
