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
#include "gdtf/gdtf_metadata_panel.h"

#include <algorithm>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
constexpr const char *kUnavailableValue = "-";
constexpr int kDescriptionHeight = 90;
constexpr int kMinimumWrapWidth = 300;
constexpr int kLabelColumnWidth = 110;
constexpr int kSizerPadding = 24;

// Returns metadata field labels in display order.
std::array<wxString, 8> MetadataFieldLabels() {
  return {"Manufacturer", "Description", "Creation date", "UserID",
          "ModifiedBy",   "Revision",    "Last modified", "Version"};
}
} // namespace

// Builds the reusable read-only GDTF metadata presentation panel.
GdtfMetadataPanel::GdtfMetadataPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
  grid->AddGrowableCol(1, 1);

  const std::array<wxString, 8> labels = MetadataFieldLabels();
  for (size_t i = 0; i < labels.size(); ++i) {
    grid->Add(new wxStaticText(this, wxID_ANY, labels[i]), 0,
              wxALIGN_CENTER_VERTICAL);
    if (i == 1) {
      descriptionCtrl = new wxTextCtrl(this, wxID_ANY, kUnavailableValue,
                                       wxDefaultPosition,
                                       wxSize(-1, kDescriptionHeight),
                                       wxTE_MULTILINE | wxTE_READONLY);
      descriptionCtrl->SetMinSize(wxSize(kMinimumWrapWidth, kDescriptionHeight));
      descriptionCtrl->ShowPosition(0);
      valueLabels[i] = nullptr;
      grid->Add(descriptionCtrl, 1, wxEXPAND);
      continue;
    }

    valueLabels[i] = new wxStaticText(this, wxID_ANY, kUnavailableValue);
    valueLabels[i]->Wrap(kMinimumWrapWidth);
    grid->Add(valueLabels[i], 1, wxEXPAND);
  }

  SetSizer(grid);
  SetMinSize(wxSize(360, 190));
  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    RewrapValueLabels();
    event.Skip();
  });
}

// Displays a successfully loaded metadata summary.
void GdtfMetadataPanel::SetMetadata(const GdtfMetadataSummary &summary) {
  SetValues({ValueOrFallback(summary.manufacturer),
             ValueOrFallback(summary.description),
             ValueOrFallback(summary.creationDate),
             ValueOrFallback(summary.userId),
             ValueOrFallback(summary.modifiedBy),
             ValueOrFallback(summary.revision),
             ValueOrFallback(summary.lastModified),
             ValueOrFallback(summary.version)});
}

// Displays unavailable state for every metadata field.
void GdtfMetadataPanel::SetUnavailable() {
  const wxString unavailable(kUnavailableValue);
  SetValues({unavailable, unavailable, unavailable, unavailable, unavailable,
             unavailable, unavailable, unavailable});
}

// Converts an individual metadata value to display text with fallback handling.
wxString GdtfMetadataPanel::ValueOrFallback(const std::string &value) const {
  if (value.empty())
    return wxString(kUnavailableValue);
  return wxString::FromUTF8(value);
}

// Replaces all displayed metadata values and refreshes layout.
void GdtfMetadataPanel::SetValues(const std::array<wxString, 8> &values) {
  for (size_t i = 0; i < valueLabels.size() && i < values.size(); ++i) {
    if (i == 1 && descriptionCtrl) {
      descriptionCtrl->SetValue(values[i]);
      descriptionCtrl->ShowPosition(0);
      continue;
    }
    if (valueLabels[i])
      valueLabels[i]->SetLabel(values[i]);
  }
  RewrapValueLabels();
  Layout();
}

// Rewraps static metadata values using the current panel width.
void GdtfMetadataPanel::RewrapValueLabels() {
  const int width = WrapWidth();
  for (wxStaticText *label : valueLabels) {
    if (label)
      label->Wrap(width);
  }
}

// Computes a stable wrap width for long metadata values.
int GdtfMetadataPanel::WrapWidth() const {
  const int clientWidth = GetClientSize().GetWidth();
  if (clientWidth <= 0)
    return kMinimumWrapWidth;
  return std::max(kMinimumWrapWidth,
                  clientWidth - kLabelColumnWidth - kSizerPadding);
}
