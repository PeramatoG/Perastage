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

#include "gdtf/gdtf_editor_visual_metrics.h"

#include <algorithm>

#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/string.h>


// Creates the reusable composite GDTF editor panel and its child panels.
GdtfEditorPanel::GdtfEditorPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {}

// Applies section titles, visibility, and the selected column layout.
void GdtfEditorPanel::Configure(
    const GdtfEditorPanelConfiguration &newConfiguration) {
  configuration = newConfiguration;
  if (!rootSizer)
    BuildSections();
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

// Forwards validation feedback for a visible identity field.
void GdtfEditorPanel::SetIdentityFieldValidation(GdtfTypeIdentityField field,
                                                 const std::string &message) {
  typeIdentityPanel->SetFieldValidation(field, message);
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

// Provides a flat titled section without a nested static-box border.
class GdtfEditorFlatSection : public wxPanel {
public:
  GdtfEditorFlatSection(wxWindow *parent, const wxString &title)
      : wxPanel(parent, wxID_ANY) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    titleLabel = new wxStaticText(this, wxID_ANY, title);
    wxFont titleFont = titleLabel->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleLabel->SetFont(titleFont);
    root->Add(titleLabel, 0, wxEXPAND | wxBOTTOM,
              gui::gdtf_layout::SectionPadding(this) / 2);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM,
              gui::gdtf_layout::SectionPadding(this));
    content = new wxPanel(this, wxID_ANY);
    auto *contentSizer = new wxBoxSizer(wxVERTICAL);
    content->SetSizer(contentSizer);
    root->Add(content, 1, wxEXPAND);
    SetSizer(root);
  }

  // Returns the content host used as the parent for one reusable child panel.
  wxPanel *Content() const { return content; }

  // Updates the visible section title.
  void SetTitle(const wxString &title) { titleLabel->SetLabel(title); }

private:
  wxStaticText *titleLabel = nullptr;
  wxPanel *content = nullptr;
};

// Creates each child panel exactly once and places it inside a flat section.
void GdtfEditorPanel::BuildSections() {
  rootSizer = new wxBoxSizer(wxVERTICAL);
  SetSizer(rootSizer);

  twoPaneSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize,
                                         wxSP_LIVE_UPDATE | wxSP_3DSASH);
  overviewPane = new wxPanel(twoPaneSplitter, wxID_ANY);
  workspacePane = new wxPanel(twoPaneSplitter, wxID_ANY);
  overviewSizer = new wxBoxSizer(wxVERTICAL);
  workspaceSizer = new wxBoxSizer(wxVERTICAL);
  overviewPane->SetSizer(overviewSizer);
  workspacePane->SetSizer(workspaceSizer);
  twoPaneSplitter->SetMinimumPaneSize(
      gui::gdtf_layout::MinimumOverviewPaneWidth(this));
  twoPaneSplitter->SplitVertically(overviewPane, workspacePane,
                                   gui::gdtf_layout::InitialContextPaneWidth(this));
  rootSizer->Add(twoPaneSplitter, 1, wxEXPAND);

  metadataSection = new GdtfEditorFlatSection(
      InitialParentForSection(GdtfEditorSection::Metadata), "GDTF metadata");
  typeIdentitySection = new GdtfEditorFlatSection(
      InitialParentForSection(GdtfEditorSection::TypeIdentity), "Type identity");
  physicalPropertiesSection = new GdtfEditorFlatSection(
      InitialParentForSection(GdtfEditorSection::PhysicalProperties),
      "Physical properties");
  modesSection = new GdtfEditorFlatSection(
      InitialParentForSection(GdtfEditorSection::Modes), "Modes and channels");

  metadataPanel = new GdtfMetadataPanel(metadataSection->Content());
  typeIdentityPanel = new GdtfTypeIdentityPanel(typeIdentitySection->Content());
  physicalPropertiesPanel =
      new GdtfPhysicalPropertiesPanel(physicalPropertiesSection->Content());
  modesPanel = new GdtfModesPanel(modesSection->Content());

  metadataSection->Content()->GetSizer()->Add(metadataPanel, 1, wxEXPAND);
  typeIdentitySection->Content()->GetSizer()->Add(typeIdentityPanel, 0,
                                                  wxEXPAND);
  physicalPropertiesSection->Content()->GetSizer()->Add(physicalPropertiesPanel,
                                                        0, wxEXPAND);
  modesSection->Content()->GetSizer()->Add(modesPanel, 1, wxEXPAND);
}

// Applies one section label and window visibility from typed configuration.
void GdtfEditorPanel::ApplySectionConfiguration(
    GdtfEditorFlatSection *section,
    const GdtfEditorSectionConfiguration &sectionConfiguration) {
  section->SetTitle(wxString::FromUTF8(sectionConfiguration.title));
  section->Show(sectionConfiguration.visible);
}

// Detaches reusable section windows before rebuilding the layout arrangement.
void GdtfEditorPanel::DetachReusableSections() {
  overviewSizer->Detach(metadataSection);
  overviewSizer->Detach(typeIdentitySection);
  overviewSizer->Detach(physicalPropertiesSection);
  overviewSizer->Detach(modesSection);
  workspaceSizer->Detach(metadataSection);
  workspaceSizer->Detach(typeIdentitySection);
  workspaceSizer->Detach(physicalPropertiesSection);
  workspaceSizer->Detach(modesSection);
}

// Rebuilds only the top-level sizer arrangement while preserving child panels.
void GdtfEditorPanel::RebuildLayout() {
  ApplySectionConfiguration(metadataSection, configuration.metadata);
  ApplySectionConfiguration(typeIdentitySection, configuration.typeIdentity);
  ApplySectionConfiguration(physicalPropertiesSection,
                            configuration.physicalProperties);
  ApplySectionConfiguration(modesSection, configuration.modes);

  DetachReusableSections();

  const bool useTwoPane = configuration.layout == GdtfEditorPanelLayout::TwoPane;
  if (useTwoPane) {
    if (!twoPaneSplitter->IsSplit())
      twoPaneSplitter->SplitVertically(overviewPane, workspacePane);
    AddTwoPaneSections();
  } else {
    if (twoPaneSplitter->IsSplit())
      twoPaneSplitter->Unsplit(workspacePane);
    AddSingleColumnSections();
  }

  Layout();
}

// Adds a visible section to a target sizer with deterministic grow behavior.
void GdtfEditorPanel::AddSection(wxBoxSizer *target, GdtfEditorSection section,
                                 int growProportion) {
  const auto &sectionConfiguration = SectionConfiguration(section);
  if (!sectionConfiguration.visible)
    return;
  target->Add(SectionWindow(section), sectionConfiguration.expanded ? growProportion : 0,
              wxEXPAND | wxBOTTOM, gui::gdtf_layout::SectionGap(this));
}

// Adds visible sections in the configured single-column order.
void GdtfEditorPanel::AddSingleColumnSections() {
  for (const auto &placement : configuration.singleColumnOrder)
    AddSection(overviewSizer, placement.section, SectionGrow(placement));
}

// Adds visible sections in the configured overview/workspace pane order.
void GdtfEditorPanel::AddTwoPaneSections() {
  for (const auto &placement : configuration.twoPaneOrder) {
    wxBoxSizer *target = placement.pane == GdtfEditorPane::Overview
                             ? overviewSizer
                             : workspaceSizer;
    AddSection(target, placement.section, SectionGrow(placement));
  }
  SetTwoPaneSplitterRatio(configuration.twoPaneInitialRatio);
}


// Returns the initial pane parent for a section from typed placement.
wxWindow *GdtfEditorPanel::InitialParentForSection(
    GdtfEditorSection section) const {
  if (configuration.layout != GdtfEditorPanelLayout::TwoPane)
    return overviewPane;
  for (const auto &placement : configuration.twoPaneOrder) {
    if (placement.section == section)
      return placement.pane == GdtfEditorPane::Workspace ? workspacePane
                                                         : overviewPane;
  }
  return overviewPane;
}

// Returns the flat section window for a typed section identifier.
GdtfEditorFlatSection *GdtfEditorPanel::SectionWindow(
    GdtfEditorSection section) const {
  switch (section) {
  case GdtfEditorSection::TypeIdentity:
    return typeIdentitySection;
  case GdtfEditorSection::Metadata:
    return metadataSection;
  case GdtfEditorSection::PhysicalProperties:
    return physicalPropertiesSection;
  case GdtfEditorSection::Modes:
    return modesSection;
  }
  return metadataSection;
}

// Returns the configuration for a typed section identifier.
const GdtfEditorSectionConfiguration &
GdtfEditorPanel::SectionConfiguration(GdtfEditorSection section) const {
  switch (section) {
  case GdtfEditorSection::TypeIdentity:
    return configuration.typeIdentity;
  case GdtfEditorSection::Metadata:
    return configuration.metadata;
  case GdtfEditorSection::PhysicalProperties:
    return configuration.physicalProperties;
  case GdtfEditorSection::Modes:
    return configuration.modes;
  }
  return configuration.metadata;
}

// Returns the grow proportion requested by one placement.
int GdtfEditorPanel::SectionGrow(
    const GdtfEditorSectionPlacement &placement) const {
  return std::max(0, placement.growProportion);
}

// Restores the internal two-pane splitter ratio after panes exist.
void GdtfEditorPanel::SetTwoPaneSplitterRatio(double ratio) {
  if (!twoPaneSplitter || !twoPaneSplitter->IsSplit())
    return;
  const int total = twoPaneSplitter->GetClientSize().GetWidth();
  const int minFirst = gui::gdtf_layout::MinimumOverviewPaneWidth(this);
  const int minSecond = gui::gdtf_layout::MinimumWorkspacePaneWidth(this);
  twoPaneSplitter->SetSashPosition(
      gui::gdtf_layout::RatioToSash(total, minFirst, minSecond, ratio));
}

// Returns the current internal two-pane splitter ratio.
double GdtfEditorPanel::GetTwoPaneSplitterRatio() const {
  if (!twoPaneSplitter)
    return configuration.twoPaneInitialRatio;
  return gui::gdtf_layout::SashToRatio(
      twoPaneSplitter->GetSashPosition(), twoPaneSplitter->GetClientSize().GetWidth(),
      configuration.twoPaneInitialRatio);
}
