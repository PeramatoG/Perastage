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

#include "gdtf_metadata_panel.h"
#include "gdtf_modes_panel.h"
#include "gdtf_physical_properties_panel.h"
#include "gdtf_type_identity_panel.h"

#include <optional>
#include <string>
#include <vector>

#include <wx/panel.h>

class wxBoxSizer;
class wxStaticBoxSizer;

// Selects the top-level arrangement used by the reusable GDTF editor panel.
enum class GdtfEditorPanelLayout { SingleColumn, TwoColumn };

struct GdtfEditorSectionConfiguration {
  bool visible = true;
  bool expanded = true;
  std::string title;
};

struct GdtfEditorPanelConfiguration {
  GdtfEditorPanelLayout layout = GdtfEditorPanelLayout::SingleColumn;
  GdtfEditorSectionConfiguration metadata{true, true, "GDTF metadata"};
  GdtfEditorSectionConfiguration typeIdentity{true, true, "Type identity"};
  GdtfEditorSectionConfiguration physicalProperties{true, true,
                                                    "Physical properties"};
  GdtfEditorSectionConfiguration modes{true, true, "Modes and channels"};
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
  void ApplySectionConfiguration(wxStaticBoxSizer *section,
                                 const GdtfEditorSectionConfiguration &configuration);
  void RebuildLayout();
  void AddSection(wxBoxSizer *target, wxStaticBoxSizer *section,
                  const GdtfEditorSectionConfiguration &configuration,
                  int growProportion);
  void AddSingleColumnSections(wxBoxSizer *root);
  void AddTwoColumnSections(wxBoxSizer *root);

  GdtfMetadataPanel *metadataPanel = nullptr;
  GdtfTypeIdentityPanel *typeIdentityPanel = nullptr;
  GdtfPhysicalPropertiesPanel *physicalPropertiesPanel = nullptr;
  GdtfModesPanel *modesPanel = nullptr;

  wxBoxSizer *rootSizer = nullptr;
  wxBoxSizer *twoColumnSizer = nullptr;
  wxBoxSizer *leftColumnSizer = nullptr;
  wxBoxSizer *rightColumnSizer = nullptr;

  wxStaticBoxSizer *metadataSection = nullptr;
  wxStaticBoxSizer *typeIdentitySection = nullptr;
  wxStaticBoxSizer *physicalPropertiesSection = nullptr;
  wxStaticBoxSizer *modesSection = nullptr;

  GdtfEditorPanelConfiguration configuration;
};
