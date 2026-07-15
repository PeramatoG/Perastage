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
#include <utility>

#include "gdtf/gdtf_editor_visual_metrics.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
constexpr const char *kUnavailableValue = "-";
constexpr int kDescriptionHeight = 130;
constexpr int kMinimumValueWrapWidth = 120;
constexpr int kInitialValueWrapWidth = 300;
constexpr int kLabelColumnWidth = 105;
constexpr int kSizerPadding = 20;

// Returns metadata field labels in display order.
std::array<wxString, 8> MetadataFieldLabels() {
  return {"Manufacturer", "Description", "Creation date", "UserID",
          "ModifiedBy",   "Revision",    "Last modified", "Version"};
}
} // namespace

// Builds the reusable read-only GDTF metadata presentation panel.
GdtfMetadataPanel::GdtfMetadataPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  currentValues.fill(wxString(kUnavailableValue));

  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 4, 8);
  grid->AddGrowableCol(1, 1);

  const std::array<wxString, 8> labels = MetadataFieldLabels();
  for (size_t i = 0; i < labels.size(); ++i) {
    grid->Add(new wxStaticText(this, wxID_ANY, labels[i]), 0,
              wxALIGN_CENTER_VERTICAL);
    if (i == 1) {
      descriptionCtrl = new wxTextCtrl(this, wxID_ANY, kUnavailableValue,
                                       wxDefaultPosition,
                                       wxSize(-1, gui::gdtf_layout::Dip(this, kDescriptionHeight)),
                                       wxTE_MULTILINE);
      descriptionCtrl->SetMinSize(
          wxSize(-1, gui::gdtf_layout::Dip(this, kDescriptionHeight)));
      descriptionCtrl->ShowPosition(0);
      descriptionCtrl->SetToolTip("GDTF FixtureType description.");
      descriptionCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent &) {
        NotifyDescriptionChanged();
      });
      valueLabels[i] = nullptr;
      grid->Add(descriptionCtrl, 1, wxEXPAND);
      continue;
    }

    valueLabels[i] = new wxStaticText(this, wxID_ANY, kUnavailableValue);
    valueLabels[i]->Wrap(kInitialValueWrapWidth);
    grid->Add(valueLabels[i], 1, wxEXPAND);
  }

  SetSizer(grid);
  SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 240)));
  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    RewrapValueLabels();
    event.Skip();
  });
}

// Displays a successfully loaded metadata summary.
void GdtfMetadataPanel::SetMetadata(const GdtfMetadataSummary &summary) {
  SetValues({ValueOrFallback(summary.manufacturer),
             wxString::FromUTF8(summary.description),
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
  currentValues = values;
  if (descriptionCtrl) {
    updating = true;
    descriptionCtrl->SetValue(currentValues[1]);
    descriptionCtrl->ShowPosition(0);
    updating = false;
  }
  RewrapValueLabels(true);
  Layout();
}

// Rewraps static metadata values from their stored unwrapped text.
void GdtfMetadataPanel::RewrapValueLabels(bool force) {
  const int width = WrapWidth();
  if (!force && width == lastAppliedWrapWidth)
    return;

  lastAppliedWrapWidth = width;
  for (size_t i = 0; i < valueLabels.size(); ++i) {
    wxStaticText *label = valueLabels[i];
    if (!label)
      continue;
    label->SetLabel(currentValues[i]);
    label->Wrap(width);
  }
}

// Computes a stable wrap width for the current value column.
int GdtfMetadataPanel::WrapWidth() const {
  const int clientWidth = GetClientSize().GetWidth();
  if (clientWidth <= 0)
    return kInitialValueWrapWidth;

  const int valueWidth = clientWidth - gui::gdtf_layout::Dip(const_cast<GdtfMetadataPanel *>(this), kLabelColumnWidth + kSizerPadding);
  return std::max(kMinimumValueWrapWidth, valueWidth);
}

// Enables or disables editing of the FixtureType description value.
void GdtfMetadataPanel::SetDescriptionEditable(bool editable) {
  if (descriptionCtrl)
    descriptionCtrl->SetEditable(editable);
}

// Registers a callback for user edits to the FixtureType description.
void GdtfMetadataPanel::SetDescriptionChangeCallback(
    DescriptionChangeCallback callback) {
  descriptionChangeCallback = std::move(callback);
}

// Notifies the host when the user changes the FixtureType description.
void GdtfMetadataPanel::NotifyDescriptionChanged() {
  if (updating || !descriptionChangeCallback || !descriptionCtrl)
    return;
  descriptionChangeCallback(std::string(descriptionCtrl->GetValue().ToUTF8()));
}
