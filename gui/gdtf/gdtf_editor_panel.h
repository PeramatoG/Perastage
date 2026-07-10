/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include "gdtf_metadata_panel.h"
#include "gdtf_modes_panel.h"
#include "gdtf_physical_properties_panel.h"
#include "gdtf_type_identity_panel.h"

#include <optional>
#include <string>
#include <vector>

#include <wx/panel.h>

class GdtfEditorFlatSection;
class wxBoxSizer;
class wxSplitterWindow;

// Selects the top-level arrangement used by the reusable GDTF editor panel.
enum class GdtfEditorPanelLayout { SingleColumn, TwoPane };

// Identifies a reusable presentation-only GDTF editor section.
enum class GdtfEditorSection {
  TypeIdentity,
  Metadata,
  PhysicalProperties,
  Modes
};

// Identifies a pane in the typed two-pane GDTF editor layout.
enum class GdtfEditorPane { Overview, Workspace };

struct GdtfEditorSectionConfiguration {
  bool visible = true;
  bool expanded = true;
  std::string title;
};

struct GdtfEditorSectionPlacement {
  GdtfEditorPane pane = GdtfEditorPane::Overview;
  GdtfEditorSection section = GdtfEditorSection::Metadata;
  int growProportion = 0;
};

struct GdtfEditorPanelConfiguration {
  GdtfEditorPanelLayout layout = GdtfEditorPanelLayout::SingleColumn;
  GdtfEditorSectionConfiguration metadata{true, true, "GDTF metadata"};
  GdtfEditorSectionConfiguration typeIdentity{true, true, "Type identity"};
  GdtfEditorSectionConfiguration physicalProperties{true, true,
                                                    "Physical properties"};
  GdtfEditorSectionConfiguration modes{true, true, "Modes and channels"};
  std::vector<GdtfEditorSectionPlacement> singleColumnOrder{
      {GdtfEditorPane::Overview, GdtfEditorSection::TypeIdentity, 0},
      {GdtfEditorPane::Overview, GdtfEditorSection::Metadata, 1},
      {GdtfEditorPane::Overview, GdtfEditorSection::PhysicalProperties, 0},
      {GdtfEditorPane::Overview, GdtfEditorSection::Modes, 1}};
  std::vector<GdtfEditorSectionPlacement> twoPaneOrder{
      {GdtfEditorPane::Overview, GdtfEditorSection::TypeIdentity, 0},
      {GdtfEditorPane::Overview, GdtfEditorSection::Metadata, 1},
      {GdtfEditorPane::Overview, GdtfEditorSection::PhysicalProperties, 0},
      {GdtfEditorPane::Workspace, GdtfEditorSection::Modes, 1}};
  double twoPaneInitialRatio = 0.45;
};

struct GdtfEditorPanelPresentation {
  bool metadataAvailable = false;
  GdtfMetadataSummary metadata;
  std::vector<GdtfTypeIdentityPresentation> identityFields;
  std::vector<GdtfPhysicalPropertyPresentation> physicalFields;
  GdtfModesPresentation modes;
};

class GdtfEditorPanel : public wxPanel {
public:
  explicit GdtfEditorPanel(wxWindow *parent);

  void Configure(const GdtfEditorPanelConfiguration &configuration);
  void SetPresentation(const GdtfEditorPanelPresentation &presentation);
  void SetUnavailable();
  void SetTwoPaneSplitterRatio(double ratio);
  double GetTwoPaneSplitterRatio() const;

  void SetIdentityChangeCallback(GdtfTypeIdentityPanel::ChangeCallback callback);
  void SetIdentityActionCallback(GdtfTypeIdentityPanel::ActionCallback callback);
  void SetPhysicalPropertyChangeCallback(
      GdtfPhysicalPropertiesPanel::ChangeCallback callback);
  void SetModeSelectionCallback(GdtfModesPanel::ModeSelectionCallback callback);

  std::optional<std::string> GetIdentityValue(GdtfTypeIdentityField field) const;
  std::optional<std::string> GetPhysicalPropertyValue(
      GdtfPhysicalPropertyField field) const;
  std::string GetSelectedMode() const;

  void SetIdentityValue(GdtfTypeIdentityField field, const std::string &value);
  void SetIdentityFieldEditable(GdtfTypeIdentityField field, bool editable);
  void SetIdentityFieldValidation(GdtfTypeIdentityField field,
                                  const std::string &message);
  void SetPhysicalPropertyValue(GdtfPhysicalPropertyField field,
                                const std::string &value);
  void SetPhysicalPropertyEditable(GdtfPhysicalPropertyField field, bool editable);
  void SetPhysicalPropertyValidation(GdtfPhysicalPropertyField field,
                                     const std::string &message);
  void SetModesPresentation(const GdtfModesPresentation &presentation);
  void SetModes(const std::vector<std::string> &modes);
  void SetSelectedMode(const std::string &mode);
  void SetChannelCount(const std::string &channelCount);
  void SetChannels(const std::vector<GdtfModeChannelPresentation> &channels);
  void ClearModeDetails();
  void SetModeSelectionEnabled(bool enabled);
  void SetMetadata(const GdtfMetadataSummary &summary);
  void SetMetadataUnavailable();

private:
  void BuildSections();
  void ApplySectionConfiguration(
      GdtfEditorFlatSection *section,
      const GdtfEditorSectionConfiguration &configuration);
  void DetachReusableSections();
  void RebuildLayout();
  void AddSection(wxBoxSizer *target, GdtfEditorSection section,
                  int growProportion);
  void AddSingleColumnSections();
  void AddTwoPaneSections();
  wxWindow *InitialParentForSection(GdtfEditorSection section) const;
  GdtfEditorFlatSection *SectionWindow(GdtfEditorSection section) const;
  const GdtfEditorSectionConfiguration &SectionConfiguration(
      GdtfEditorSection section) const;
  int SectionGrow(const GdtfEditorSectionPlacement &placement) const;

  GdtfMetadataPanel *metadataPanel = nullptr;
  GdtfTypeIdentityPanel *typeIdentityPanel = nullptr;
  GdtfPhysicalPropertiesPanel *physicalPropertiesPanel = nullptr;
  GdtfModesPanel *modesPanel = nullptr;

  wxBoxSizer *rootSizer = nullptr;
  wxSplitterWindow *twoPaneSplitter = nullptr;
  wxPanel *overviewPane = nullptr;
  wxPanel *workspacePane = nullptr;
  wxBoxSizer *overviewSizer = nullptr;
  wxBoxSizer *workspaceSizer = nullptr;

  GdtfEditorFlatSection *metadataSection = nullptr;
  GdtfEditorFlatSection *typeIdentitySection = nullptr;
  GdtfEditorFlatSection *physicalPropertiesSection = nullptr;
  GdtfEditorFlatSection *modesSection = nullptr;

  GdtfEditorPanelConfiguration configuration;
};
