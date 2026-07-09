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
#include "gdtf/gdtf_editor_panel.h"

#include <utility>

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/string.h>

namespace {
constexpr int kSectionGap = 8;
constexpr int kSectionPadding = 6;
}

// Creates the reusable composite GDTF editor panel and its child panels.
GdtfEditorPanel::GdtfEditorPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  BuildSections();
  RebuildLayout();
}

// Applies section titles, visibility, and the selected column layout.
void GdtfEditorPanel::Configure(
    const GdtfEditorPanelConfiguration &newConfiguration) {
  configuration = newConfiguration;
  RebuildLayout();
}

// Applies a complete aggregate presentation without notifying callbacks.
void GdtfEditorPanel::SetPresentation(
    const GdtfEditorPanelPresentation &presentation) {
  if (presentation.metadataAvailable)
    metadataPanel->SetMetadata(presentation.metadata);
  else
    metadataPanel->SetUnavailable();
  typeIdentityPanel->ConfigureFields(presentation.identityFields);
  physicalPropertiesPanel->ConfigureFields(presentation.physicalFields);
  modesPanel->SetPresentation(presentation.modes);
}

// Clears all child presentations to their deterministic unavailable state.
void GdtfEditorPanel::SetUnavailable() {
  metadataPanel->SetUnavailable();
  typeIdentityPanel->ConfigureFields({});
  physicalPropertiesPanel->ConfigureFields({});
  modesPanel->SetPresentation({});
}

// Registers the forwarded type identity change callback.
void GdtfEditorPanel::SetIdentityChangeCallback(
    GdtfTypeIdentityPanel::ChangeCallback callback) {
  typeIdentityPanel->SetChangeCallback(std::move(callback));
}

// Registers the forwarded type identity action callback.
void GdtfEditorPanel::SetIdentityActionCallback(
    GdtfTypeIdentityPanel::ActionCallback callback) {
  typeIdentityPanel->SetActionCallback(std::move(callback));
}

// Registers the forwarded physical property change callback.
void GdtfEditorPanel::SetPhysicalPropertyChangeCallback(
    GdtfPhysicalPropertiesPanel::ChangeCallback callback) {
  physicalPropertiesPanel->SetChangeCallback(std::move(callback));
}

// Registers the forwarded mode selection callback.
void GdtfEditorPanel::SetModeSelectionCallback(
    GdtfModesPanel::ModeSelectionCallback callback) {
  modesPanel->SetModeSelectionCallback(std::move(callback));
}

// Returns the current value for a visible identity field.
std::optional<std::string>
GdtfEditorPanel::GetIdentityValue(GdtfTypeIdentityField field) const {
  return typeIdentityPanel->GetFieldValue(field);
}

// Returns the current value for a visible physical property field.
std::optional<std::string> GdtfEditorPanel::GetPhysicalPropertyValue(
    GdtfPhysicalPropertyField field) const {
  return physicalPropertiesPanel->GetFieldValue(field);
}

// Returns the currently selected GDTF mode.
std::string GdtfEditorPanel::GetSelectedMode() const {
  return modesPanel->GetSelectedMode();
}

// Sets one identity field value without notifying callbacks.
void GdtfEditorPanel::SetIdentityValue(GdtfTypeIdentityField field,
                                       const std::string &value) {
  typeIdentityPanel->SetFieldValue(field, value);
}

// Sets identity field editability for a visible field.
void GdtfEditorPanel::SetIdentityFieldEditable(GdtfTypeIdentityField field,
                                               bool editable) {
  typeIdentityPanel->SetFieldEditable(field, editable);
}

// Sets one physical property value without notifying callbacks.
void GdtfEditorPanel::SetPhysicalPropertyValue(GdtfPhysicalPropertyField field,
                                               const std::string &value) {
  physicalPropertiesPanel->SetFieldValue(field, value);
}

// Sets physical property editability for a visible field.
void GdtfEditorPanel::SetPhysicalPropertyEditable(GdtfPhysicalPropertyField field,
                                                  bool editable) {
  physicalPropertiesPanel->SetFieldEditable(field, editable);
}

// Forwards validation feedback for a visible physical property field.
void GdtfEditorPanel::SetPhysicalPropertyValidation(
    GdtfPhysicalPropertyField field, const std::string &message) {
  physicalPropertiesPanel->SetFieldValidation(field, message);
}

// Applies modes and channels presentation without notifying callbacks.
void GdtfEditorPanel::SetModesPresentation(
    const GdtfModesPresentation &presentation) {
  modesPanel->SetPresentation(presentation);
}

// Replaces the mode list without notifying callbacks.
void GdtfEditorPanel::SetModes(const std::vector<std::string> &modes) {
  modesPanel->SetModes(modes);
}

// Selects a mode without notifying callbacks.
void GdtfEditorPanel::SetSelectedMode(const std::string &mode) {
  modesPanel->SetSelectedMode(mode);
}

// Sets the derived channel count without notifying callbacks.
void GdtfEditorPanel::SetChannelCount(const std::string &channelCount) {
  modesPanel->SetChannelCount(channelCount);
}

// Sets the visible channel rows without notifying callbacks.
void GdtfEditorPanel::SetChannels(
    const std::vector<GdtfModeChannelPresentation> &channels) {
  modesPanel->SetChannels(channels);
}

// Clears derived mode details without notifying callbacks.
void GdtfEditorPanel::ClearModeDetails() {
  modesPanel->ClearModeDetails();
}

// Enables or disables user mode selection.
void GdtfEditorPanel::SetModeSelectionEnabled(bool enabled) {
  modesPanel->SetModeSelectionEnabled(enabled);
}

// Applies metadata presentation to the metadata section.
void GdtfEditorPanel::SetMetadata(const GdtfMetadataSummary &summary) {
  metadataPanel->SetMetadata(summary);
}

// Applies the metadata unavailable fallback presentation.
void GdtfEditorPanel::SetMetadataUnavailable() {
  metadataPanel->SetUnavailable();
}

// Creates each child panel exactly once and places it inside a section box.
void GdtfEditorPanel::BuildSections() {
  rootSizer = new wxBoxSizer(wxVERTICAL);
  twoColumnSizer = new wxBoxSizer(wxHORIZONTAL);
  leftColumnSizer = new wxBoxSizer(wxVERTICAL);
  rightColumnSizer = new wxBoxSizer(wxVERTICAL);
  SetSizer(rootSizer);

  metadataSection = new wxStaticBoxSizer(wxVERTICAL, this, wxString());
  typeIdentitySection = new wxStaticBoxSizer(wxVERTICAL, this, wxString());
  physicalPropertiesSection = new wxStaticBoxSizer(wxVERTICAL, this, wxString());
  modesSection = new wxStaticBoxSizer(wxVERTICAL, this, wxString());

  metadataPanel = new GdtfMetadataPanel(metadataSection->GetStaticBox());
  typeIdentityPanel =
      new GdtfTypeIdentityPanel(typeIdentitySection->GetStaticBox());
  physicalPropertiesPanel = new GdtfPhysicalPropertiesPanel(
      physicalPropertiesSection->GetStaticBox());
  modesPanel = new GdtfModesPanel(modesSection->GetStaticBox());

  metadataSection->Add(metadataPanel, 0, wxEXPAND | wxALL, kSectionPadding);
  typeIdentitySection->Add(typeIdentityPanel, 0, wxEXPAND | wxALL, kSectionPadding);
  physicalPropertiesSection->Add(physicalPropertiesPanel, 0,
                                 wxEXPAND | wxALL, kSectionPadding);
  modesSection->Add(modesPanel, 1, wxEXPAND | wxALL, kSectionPadding);
}

// Applies one section label and window visibility from typed configuration.
void GdtfEditorPanel::ApplySectionConfiguration(
    wxStaticBoxSizer *section,
    const GdtfEditorSectionConfiguration &sectionConfiguration) {
  section->GetStaticBox()->SetLabel(wxString::FromUTF8(sectionConfiguration.title));
  section->ShowItems(sectionConfiguration.visible);
  section->GetStaticBox()->Show(sectionConfiguration.visible);
}

// Rebuilds only the top-level sizer arrangement while preserving child panels.
void GdtfEditorPanel::RebuildLayout() {
  ApplySectionConfiguration(metadataSection, configuration.metadata);
  ApplySectionConfiguration(typeIdentitySection, configuration.typeIdentity);
  ApplySectionConfiguration(physicalPropertiesSection,
                            configuration.physicalProperties);
  ApplySectionConfiguration(modesSection, configuration.modes);

  rootSizer->Clear(false);
  twoColumnSizer->Clear(false);
  leftColumnSizer->Clear(false);
  rightColumnSizer->Clear(false);

  if (configuration.layout == GdtfEditorPanelLayout::TwoColumn)
    AddTwoColumnSections(rootSizer);
  else
    AddSingleColumnSections(rootSizer);

  Layout();
}

// Adds a visible section to a target sizer with deterministic grow behavior.
void GdtfEditorPanel::AddSection(wxBoxSizer *target, wxStaticBoxSizer *section,
                                 const GdtfEditorSectionConfiguration &sectionConfiguration,
                                 int growProportion) {
  if (!sectionConfiguration.visible)
    return;
  target->Add(section, sectionConfiguration.expanded ? growProportion : 0,
              wxEXPAND | wxBOTTOM, kSectionGap);
}

// Adds visible sections in the documented single-column order.
void GdtfEditorPanel::AddSingleColumnSections(wxBoxSizer *root) {
  AddSection(root, metadataSection, configuration.metadata, 0);
  AddSection(root, typeIdentitySection, configuration.typeIdentity, 0);
  AddSection(root, physicalPropertiesSection, configuration.physicalProperties, 0);
  AddSection(root, modesSection, configuration.modes, 1);
}

// Adds visible sections in the documented balanced two-column arrangement.
void GdtfEditorPanel::AddTwoColumnSections(wxBoxSizer *root) {
  AddSection(leftColumnSizer, metadataSection, configuration.metadata, 0);
  AddSection(leftColumnSizer, typeIdentitySection, configuration.typeIdentity, 0);
  AddSection(rightColumnSizer, physicalPropertiesSection,
             configuration.physicalProperties, 0);
  AddSection(rightColumnSizer, modesSection, configuration.modes, 1);
  twoColumnSizer->Add(leftColumnSizer, 1, wxEXPAND | wxRIGHT, kSectionGap);
  twoColumnSizer->Add(rightColumnSizer, 1, wxEXPAND);
  root->Add(twoColumnSizer, 1, wxEXPAND);
}
