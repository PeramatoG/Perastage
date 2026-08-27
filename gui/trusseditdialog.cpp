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
#include "trusseditdialog.h"

#include "configmanager.h"
#include "fixturepreviewpanel.h"
#include "filesystem_path_utils.h"
#include "gdtf/gdtf_editor_panel.h"
#include "gdtf/gdtf_editor_layout_preferences.h"
#include "gdtf/gdtf_editor_visual_metrics.h"
#include "gdtf/gdtf_session_panel_binding.h"
#include "gdtf_metadata_summary.h"
#include "gdtf/editor/gdtf_document.h"
#include "gdtf/editor/project_truss_gdtf_apply_adapter.h"
#include "gdtf/editor/project_truss_gdtf_context.h"
#include "gdtfdictionary.h"
#include "guiconfigservices.h"
#include "projectutils.h"
#include "preview_resource.h"
#include "table_column_indices.h"
#include "truss_gdtf_apply_services.h"
#include "trusstablepanel.h"
#include "units/units.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <optional>

#include <wx/scrolwin.h>
#include <wx/splitter.h>

namespace {
using TrussColumn = TrussTableColumns::Column;

// Converts a truss column to its stable table index.
constexpr int ColumnIndex(TrussColumn column) {
  return TableColumnIndices::ToIndex(column);
}

// Checks whether a table column stores GDTF type metadata.
bool IsGdtfTrussColumn(size_t index) {
  return index == static_cast<size_t>(ColumnIndex(TrussColumn::Manufacturer)) ||
         index == static_cast<size_t>(ColumnIndex(TrussColumn::Model)) ||
         index == static_cast<size_t>(ColumnIndex(TrussColumn::Length)) ||
         index == static_cast<size_t>(ColumnIndex(TrussColumn::Width)) ||
         index == static_cast<size_t>(ColumnIndex(TrussColumn::Height)) ||
         index == static_cast<size_t>(ColumnIndex(TrussColumn::Weight));
}

// Checks whether a path points to an existing regular file without throwing.
bool IsExistingRegularFile(const std::filesystem::path &path) {
  if (path.empty())
    return false;
  std::error_code ec;
  return std::filesystem::exists(path, ec) && !ec &&
         std::filesystem::is_regular_file(path, ec) && !ec;
}

// Resolves the active distance unit system from the shared UI preference.
Units::DistanceUnitSystem ResolveDistanceUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return Units::ParseDistanceUnitSystem(
      cfg.GetValue("ui_distance_unit_system"));
}

// Resolves the active weight unit system from the shared UI preference.
Units::WeightUnitSystem ResolveWeightUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
}

// Parses a fully consumed double value from editor session storage.
std::optional<double> ParseSessionDouble(const std::string &rawValue) {
  if (rawValue.empty())
    return std::nullopt;

  errno = 0;
  char *endPtr = nullptr;
  const double value = std::strtod(rawValue.c_str(), &endPtr);
  if (endPtr != rawValue.c_str() + rawValue.size() || errno == ERANGE)
    return std::nullopt;
  return value;
}

// Formats a truss dimension from stored millimeters into active display units.
std::string FormatTrussDimensionForEditor(
    const gdtf::GdtfEditableValues &values, gdtf::GdtfFieldId fieldId,
    const std::string &fallback, Units::DistanceUnitSystem unitSystem) {
  const auto rawValue = gdtf::GetEditableValue(values, fieldId);
  if (!rawValue.has_value())
    return fallback;
  if (rawValue->empty())
    return {};
  if (const auto parsed = ParseSessionDouble(*rawValue); parsed.has_value())
    return Units::FormatDistanceFromMillimeters(
        *parsed, unitSystem, Units::ValueFormatContext::Table);
  return *rawValue;
}

// Formats a truss weight from stored kilograms into active display units.
std::string FormatTrussWeightForEditor(const gdtf::GdtfEditableValues &values,
                                       gdtf::GdtfFieldId fieldId,
                                       const std::string &fallback,
                                       Units::WeightUnitSystem unitSystem) {
  const auto rawValue = gdtf::GetEditableValue(values, fieldId);
  if (!rawValue.has_value())
    return fallback;
  if (rawValue->empty())
    return {};
  if (const auto parsed =
          Units::ParseWeightToKilograms(
              *rawValue, Units::WeightUnitSystem::Metric);
      parsed.has_value())
    return Units::FormatWeightFromKilograms(
        *parsed, unitSystem, Units::ValueFormatContext::Table);
  return *rawValue;
}

// Parses an editor dimension value back to the session's millimeter storage.
std::optional<std::string> ParseEditorDimensionToSessionValue(
    const std::string &value, Units::DistanceUnitSystem unitSystem) {
  const auto parsed = Units::ParseDistanceToMillimeters(value, unitSystem);
  if (!parsed.has_value())
    return std::nullopt;
  return std::to_string(static_cast<float>(*parsed));
}

// Parses an editor weight value back to the session's kilogram storage.
std::optional<std::string> ParseEditorWeightToSessionValue(
    const std::string &value, Units::WeightUnitSystem unitSystem) {
  const auto parsed = Units::ParseWeightToKilograms(value, unitSystem);
  if (!parsed.has_value())
    return std::nullopt;
  return std::to_string(static_cast<float>(*parsed));
}
} // namespace

// Destroys the host-owned GDTF edit session after the dialog closes.
TrussEditDialog::~TrussEditDialog() = default;

// Builds the host-owned GDTF edit session from current truss row state.
void TrussEditDialog::BuildEditSession() {
  if (!panel || row < 0 || static_cast<size_t>(row) >= panel->rowUuids.size())
    return;
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.trusses.end())
    return;
  gdtf::ProjectTrussGdtfContextInput input;
  input.truss = it->second;
  input.resolvedGdtfPath = ResolveCurrentGdtfPath();
  if (IsExistingRegularFile(input.resolvedGdtfPath))
    input.document = gdtf::LoadGdtfDocument(input.resolvedGdtfPath);
  input.sourceKind = input.resolvedGdtfPath.empty()
                         ? gdtf::GdtfSourceKind::Unknown
                         : gdtf::GdtfSourceKind::PerastageTrussLibraryFile;
  input.writePolicy = gdtf::GdtfWritePolicy::ProjectControlledGeneration;
  gdtfEditSession = std::make_unique<gdtf::GdtfEditSession>(
      gdtf::BuildProjectTrussGdtfEditSession(input));
}

// Builds the truss editing dialog with MVR and GDTF fields.
TrussEditDialog::TrussEditDialog(TrussTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Truss", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
      panel(p), row(r) {
  BuildEditSession();
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  auto *contentPanel = new wxPanel(this, wxID_ANY);
  wxBoxSizer *contentSizer = new wxBoxSizer(wxVERTICAL);
  contentPanel->SetSizer(contentSizer);
  contextSplitter = new wxSplitterWindow(contentPanel, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3DSASH);
  auto *mvrScroll = new wxScrolledWindow(contextSplitter, wxID_ANY);
  mvrScroll->SetScrollRate(0, gui::gdtf_layout::Dip(this, 12));
  auto *mvrSizer = new wxBoxSizer(wxVERTICAL);
  mvrScroll->SetSizer(mvrSizer);
  auto *mvrTitle = new wxStaticText(mvrScroll, wxID_ANY, _("MVR instance"));
  wxFont mvrTitleFont = mvrTitle->GetFont();
  mvrTitleFont.SetWeight(wxFONTWEIGHT_BOLD);
  mvrTitle->SetFont(mvrTitleFont);
  mvrSizer->Add(mvrTitle, 0, wxEXPAND | wxBOTTOM, gui::gdtf_layout::SectionPadding(this));
  wxWindow *mvrParent = mvrScroll;
  wxFlexGridSizer *mvrGrid = new wxFlexGridSizer(2, gui::gdtf_layout::CompactFieldGap(this), gui::gdtf_layout::CompactLabelGap(this));
  mvrGrid->AddGrowableCol(1, 1);

  auto *table = panel->table;
  ctrls.resize(panel->columnLabels.size(), nullptr);
  modifiedColumns.assign(panel->columnLabels.size(), false);

  for (size_t i = 0; i < panel->columnLabels.size(); ++i) {
    if (IsGdtfTrussColumn(i))
      continue;
    wxVariant value;
    table->GetValue(value, row, static_cast<unsigned int>(i));
    wxTextCtrl *control =
        new wxTextCtrl(mvrParent, wxID_ANY, value.GetString());
    control->Bind(wxEVT_TEXT,
                  [this, i](wxCommandEvent &) { MarkColumnModified(i); });
    ctrls[i] = control;
    mvrGrid->Add(new wxStaticText(mvrParent, wxID_ANY, panel->columnLabels[i]),
                 0, wxALIGN_CENTER_VERTICAL);
    mvrGrid->Add(control, 1, wxEXPAND);
  }

  wxString crossSection;
  if (row >= 0 && static_cast<size_t>(row) < panel->rowUuids.size()) {
    auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
    if (it != scene.trusses.end())
      crossSection = wxString::FromUTF8(it->second.crossSection);
  }

  auto tableValue = [&](TrussColumn column) {
    wxVariant value;
    table->GetValue(value, row, ColumnIndex(column));
    return std::string(value.GetString().ToUTF8());
  };

  auto *rightWorkspace = new wxPanel(contextSplitter, wxID_ANY);
  auto *rightWorkspaceSizer = new wxBoxSizer(wxVERTICAL);
  rightWorkspace->SetSizer(rightWorkspaceSizer);
  gdtfEditorPanel = new GdtfEditorPanel(rightWorkspace);
  GdtfEditorPanelConfiguration gdtfConfiguration;
  gdtfConfiguration.layout = GdtfEditorPanelLayout::TwoPane;
  gdtfConfiguration.twoPaneInitialRatio = 0.55;
  gdtfConfiguration.twoPaneOrder = {
      {GdtfEditorPane::Overview, GdtfEditorSection::TypeIdentity, 0},
      {GdtfEditorPane::Overview, GdtfEditorSection::Metadata, 1},
      {GdtfEditorPane::Workspace, GdtfEditorSection::PhysicalProperties, 0}};
  gdtfConfiguration.metadata.title = "GDTF metadata";
  gdtfConfiguration.typeIdentity.title = "Truss type";
  gdtfConfiguration.physicalProperties.title = "Physical properties";
  gdtfConfiguration.channelSummary.visible = false;
  gdtfConfiguration.modes.visible = false;
  gdtfEditorPanel->Configure(gdtfConfiguration);
  wxWindow *previewHost = gdtfEditorPanel->GetWorkspaceHeaderHost();
  wxSizer *previewSizer = previewHost->GetSizer();
  preview = new FixturePreviewPanel(previewHost);
  preview->SetMinSize(wxSize(gui::gdtf_layout::MinimumWorkspacePaneWidth(this),
                             gui::gdtf_layout::Dip(this, 160)));
  previewSizer->Add(preview, 1, wxEXPAND);
  gdtfEditorPanel->Configure(gdtfConfiguration);
  const auto &sessionValues =
      gdtfEditSession ? gdtfEditSession->CurrentValues()
                      : gdtf::GdtfEditableValues{};
  const auto distanceUnit = ResolveDistanceUnitSystem();
  const auto weightUnit = ResolveWeightUnitSystem();
  const std::string distanceSuffix = Units::DistanceUnitSuffix(distanceUnit);
  const std::string weightSuffix = Units::WeightUnitSuffix(weightUnit);
  gdtfEditorPanel->SetMetadataDescriptionEditable(
      gdtfEditSession && gui::gdtf_binding::IsEditable(
                             *gdtfEditSession,
                             gdtf::GdtfFieldId::FixtureTypeDescription));
  gdtfEditorPanel->SetPresentation({
      false,
      {},
      {
          {GdtfTypeIdentityField::Manufacturer, "Manufacturer",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::Manufacturer,
               tableValue(TrussColumn::Manufacturer)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::Manufacturer)},
          {GdtfTypeIdentityField::ModelName, "Model",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::ModelName,
               tableValue(TrussColumn::Model)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::ModelName)},
      },
      {
          {GdtfPhysicalPropertyField::Length, "Length",
           FormatTrussDimensionForEditor(
               sessionValues, gdtf::GdtfFieldId::TrussLength,
               tableValue(TrussColumn::Length), distanceUnit),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussLength),
           distanceSuffix},
          {GdtfPhysicalPropertyField::Width, "Width",
           FormatTrussDimensionForEditor(
               sessionValues, gdtf::GdtfFieldId::TrussWidth,
               tableValue(TrussColumn::Width), distanceUnit),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussWidth),
           distanceSuffix},
          {GdtfPhysicalPropertyField::Height, "Height",
           FormatTrussDimensionForEditor(
               sessionValues, gdtf::GdtfFieldId::TrussHeight,
               tableValue(TrussColumn::Height), distanceUnit),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussHeight),
           distanceSuffix},
          {GdtfPhysicalPropertyField::Weight, "Weight",
           FormatTrussWeightForEditor(
               sessionValues, gdtf::GdtfFieldId::Weight,
               tableValue(TrussColumn::Weight), weightUnit),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::Weight),
           weightSuffix},
          {GdtfPhysicalPropertyField::CrossSectionType, "Cross-section type",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussCrossSectionType,
               "TrussFramework"),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::TrussCrossSectionType),
           "",
           "GDTF structure cross-section type. Official values are TrussFramework and Tube.",
           {"TrussFramework", "Tube"}},
          {GdtfPhysicalPropertyField::CrossSection, "Truss cross-section name",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussCrossSection,
               std::string(crossSection.ToUTF8())),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::TrussCrossSection) &&
               gui::gdtf_binding::ValueText(
                   sessionValues, gdtf::GdtfFieldId::TrussCrossSectionType,
                   "TrussFramework") == "TrussFramework",
           "",
           "Manufacturer or structural cross-section identifier. Used only when the cross-section type is TrussFramework, for example H40V, F34 or GenericTruss"},
      },
      {}});
  gdtfEditorPanel->SetIdentityChangeCallback(
      [this](GdtfTypeIdentityField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field))
          SetSessionValue(*fieldId, value);
      });
  gdtfEditorPanel->SetPhysicalPropertyChangeCallback(
      [this](GdtfPhysicalPropertyField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field)) {
          SetSessionValue(*fieldId, value);
          if (*fieldId == gdtf::GdtfFieldId::TrussCrossSectionType)
            gdtfEditorPanel->SetPhysicalPropertyEditable(
                GdtfPhysicalPropertyField::CrossSection,
                value == "TrussFramework");
        }
      });
  gdtfEditorPanel->SetMetadataDescriptionChangeCallback(
      [this](const std::string &value) {
        SetSessionValue(gdtf::GdtfFieldId::FixtureTypeDescription, value);
      });

  mvrSizer->Add(mvrGrid, 1, wxEXPAND | wxALL, gui::gdtf_layout::SectionPadding(this));
  mvrScroll->SetMinSize(wxSize(gui::gdtf_layout::MinimumContextPaneWidth(this), -1));
  rightWorkspaceSizer->Add(gdtfEditorPanel, 1, wxEXPAND | wxALL,
                           gui::gdtf_layout::SectionPadding(this));
  rightWorkspaceSizer->Add(
      new wxStaticText(rightWorkspace, wxID_ANY,
                       _("Editing these fields creates or updates the truss "
                       "GDTF. MVR-only fields remain project-scoped.")),
      0, wxLEFT | wxRIGHT | wxBOTTOM, gui::gdtf_layout::SectionPadding(this));
  contextSplitter->SplitVertically(mvrScroll, rightWorkspace);
  contextSplitter->SetMinimumPaneSize(gui::gdtf_layout::MinimumContextPaneWidth(this));
  contentSizer->Add(contextSplitter, 1, wxEXPAND);
  topSizer->Add(contentPanel, 1, wxEXPAND | wxALL, gui::gdtf_layout::OuterMargin(this));

  wxStdDialogButtonSizer *buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_APPLY));
  buttons->AddButton(new wxButton(this, wxID_OK));
  buttons->AddButton(new wxButton(this, wxID_CANCEL));
  buttons->Realize();
  topSizer->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, gui::gdtf_layout::ButtonRowMargin(this));

  Bind(wxEVT_BUTTON, &TrussEditDialog::OnApply, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &TrussEditDialog::OnOk, this, wxID_OK);
  Bind(wxEVT_BUTTON, &TrussEditDialog::OnCancel, this, wxID_CANCEL);
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event) {
    SaveLayoutPreferences();
    event.Skip();
  });

  SetSizer(topSizer);
  RestoreLayoutPreferences();
  SetMinSize(gui::gdtf_layout::ClampDialogSize(this, wxSize(900, 560), wxSize(900, 560), wxSize(1, 1)));
  UpdateMetadataSummary();
  UpdatePreview();
}

// Marks a table-backed field as user-modified.
void TrussEditDialog::MarkColumnModified(size_t index) {
  if (index < modifiedColumns.size())
    modifiedColumns[index] = true;
}

// Mirrors authoritative session dirty fields into legacy table-column flags.
void TrussEditDialog::SyncSessionDirtyToLegacyFlags() {
  if (!gdtfEditSession)
    return;
  auto setFlag = [this](TrussColumn column, gdtf::GdtfFieldId fieldId) {
    const size_t index = static_cast<size_t>(ColumnIndex(column));
    if (index < modifiedColumns.size())
      modifiedColumns[index] = gdtfEditSession->IsFieldDirty(fieldId);
  };
  setFlag(TrussColumn::Manufacturer, gdtf::GdtfFieldId::Manufacturer);
  setFlag(TrussColumn::Model, gdtf::GdtfFieldId::ModelName);
  setFlag(TrussColumn::Length, gdtf::GdtfFieldId::TrussLength);
  setFlag(TrussColumn::Width, gdtf::GdtfFieldId::TrussWidth);
  setFlag(TrussColumn::Height, gdtf::GdtfFieldId::TrussHeight);
  setFlag(TrussColumn::Weight, gdtf::GdtfFieldId::Weight);
  crossSectionModified =
      gdtfEditSession->IsFieldDirty(gdtf::GdtfFieldId::TrussCrossSection) ||
      gdtfEditSession->IsFieldDirty(gdtf::GdtfFieldId::TrussCrossSectionType) ||
      gdtfEditSession->IsFieldDirty(gdtf::GdtfFieldId::FixtureTypeDescription);
}

// Clears session validation tooltips from the GDTF editor presentation.
void TrussEditDialog::ClearSessionValidation() {
  if (!gdtfEditorPanel)
    return;
  for (const auto field : {GdtfPhysicalPropertyField::Length,
                           GdtfPhysicalPropertyField::Width,
                           GdtfPhysicalPropertyField::Height,
                           GdtfPhysicalPropertyField::Weight,
                           GdtfPhysicalPropertyField::CrossSectionType,
                           GdtfPhysicalPropertyField::CrossSection})
    gdtfEditorPanel->SetPhysicalPropertyValidation(field, {});
}

// Stores a supported panel edit in the session and mirrors dirty state.
bool TrussEditDialog::SetSessionValue(gdtf::GdtfFieldId fieldId,
                                      const std::string &value) {
  if (!gdtfEditSession)
    return false;
  std::string sessionValue = value;
  if (fieldId == gdtf::GdtfFieldId::TrussLength ||
      fieldId == gdtf::GdtfFieldId::TrussWidth ||
      fieldId == gdtf::GdtfFieldId::TrussHeight) {
    const auto parsed =
        ParseEditorDimensionToSessionValue(value, ResolveDistanceUnitSystem());
    if (!parsed.has_value())
      sessionValue = value;
    else
      sessionValue = *parsed;
  } else if (fieldId == gdtf::GdtfFieldId::Weight) {
    const auto parsed =
        ParseEditorWeightToSessionValue(value, ResolveWeightUnitSystem());
    if (!parsed.has_value())
      sessionValue = value;
    else
      sessionValue = *parsed;
  }
  const bool accepted = gdtfEditSession->SetValue(fieldId, sessionValue);
  if (!accepted) {
    rejectedSessionInputs[fieldId] = "Enter a valid numeric value.";
    auto physicalField = GdtfPhysicalPropertyField::Weight;
    if (fieldId == gdtf::GdtfFieldId::TrussLength)
      physicalField = GdtfPhysicalPropertyField::Length;
    else if (fieldId == gdtf::GdtfFieldId::TrussWidth)
      physicalField = GdtfPhysicalPropertyField::Width;
    else if (fieldId == gdtf::GdtfFieldId::TrussHeight)
      physicalField = GdtfPhysicalPropertyField::Height;
    gdtfEditorPanel->SetPhysicalPropertyValidation(
        physicalField, "Enter a valid numeric value.");
    SyncSessionDirtyToLegacyFlags();
    return false;
  }
  rejectedSessionInputs.erase(fieldId);
  ClearSessionValidation();
  for (const auto &entry : rejectedSessionInputs) {
    auto physicalField = GdtfPhysicalPropertyField::Weight;
    if (entry.first == gdtf::GdtfFieldId::TrussLength)
      physicalField = GdtfPhysicalPropertyField::Length;
    else if (entry.first == gdtf::GdtfFieldId::TrussWidth)
      physicalField = GdtfPhysicalPropertyField::Width;
    else if (entry.first == gdtf::GdtfFieldId::TrussHeight)
      physicalField = GdtfPhysicalPropertyField::Height;
    else if (entry.first == gdtf::GdtfFieldId::TrussCrossSectionType)
      physicalField = GdtfPhysicalPropertyField::CrossSectionType;
    else if (entry.first == gdtf::GdtfFieldId::TrussCrossSection)
      physicalField = GdtfPhysicalPropertyField::CrossSection;
    gdtfEditorPanel->SetPhysicalPropertyValidation(physicalField, entry.second);
  }
  SyncSessionDirtyToLegacyFlags();
  return true;
}

// Validates session state before any legacy apply mutation starts.
bool TrussEditDialog::ValidateSessionBeforeApply() {
  if (!gdtfEditSession)
    return true;
  ClearSessionValidation();
  if (!rejectedSessionInputs.empty()) {
    wxMessageBox(_("Fix malformed GDTF editor values before applying."),
                 _("GDTF validation"), wxOK | wxICON_WARNING, this);
    return false;
  }
  const auto diagnostics = gdtfEditSession->Validate();
  if (diagnostics.empty())
    return true;
  std::string message = "Fix GDTF editor validation errors before applying.";
  for (const auto &diagnostic : diagnostics) {
    message += "\n- " + diagnostic.message;
    if (diagnostic.fieldId == gdtf::GdtfFieldId::TrussLength)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Length, diagnostic.message);
    else if (diagnostic.fieldId == gdtf::GdtfFieldId::TrussWidth)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Width, diagnostic.message);
    else if (diagnostic.fieldId == gdtf::GdtfFieldId::TrussHeight)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Height, diagnostic.message);
    else if (diagnostic.fieldId == gdtf::GdtfFieldId::Weight)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Weight, diagnostic.message);
  }
  wxMessageBox(wxString::FromUTF8(message), _("GDTF validation"),
               wxOK | wxICON_WARNING, this);
  return false;
}

// Saves the Truss Edit visual layout preferences on dialog close paths.
void TrussEditDialog::SaveLayoutPreferences() {
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  gui::gdtf_layout::TrussLayoutPreferences preferences;
  preferences.dialogSize = GetSize();
  if (contextSplitter)
    preferences.contextRatio = gui::gdtf_layout::SashToRatio(
        contextSplitter->GetSashPosition(), contextSplitter->GetClientSize().GetWidth(), 0.25);
  if (gdtfEditorPanel)
    preferences.gdtfRatio = gdtfEditorPanel->GetTwoPaneSplitterRatio();
  gui::gdtf_layout::SaveTrussLayoutPreferences(config, preferences);
}

// Restores Truss Edit size and splitter preferences with display clamping.
void TrussEditDialog::RestoreLayoutPreferences() {
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto preferences = gui::gdtf_layout::LoadTrussLayoutPreferences(config, this);
  SetSize(preferences.dialogSize);
  Layout();
  if (contextSplitter)
    contextSplitter->SetSashPosition(gui::gdtf_layout::RatioToSash(
        contextSplitter->GetClientSize().GetWidth(),
        gui::gdtf_layout::MinimumContextPaneWidth(this),
        gui::gdtf_layout::MinimumWorkspacePaneWidth(this), preferences.contextRatio));
  if (gdtfEditorPanel)
    gdtfEditorPanel->SetTwoPaneSplitterRatio(preferences.gdtfRatio);
}

// Applies edits without closing the dialog.
void TrussEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

// Applies edits and closes the dialog.
void TrussEditDialog::OnOk(wxCommandEvent &) {
  if (!ApplyChanges())
    return;
  SaveLayoutPreferences();
  EndModal(wxID_OK);
}

// Closes the dialog without applying pending edits.
void TrussEditDialog::OnCancel(wxCommandEvent &) {
  SaveLayoutPreferences();
  EndModal(wxID_CANCEL);
}

// Resolves the current truss GDTF path for metadata and preview display.
std::string TrussEditDialog::ResolveCurrentGdtfPath() const {
  if (!panel || row < 0 || static_cast<size_t>(row) >= panel->rowUuids.size())
    return {};

  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.trusses.end())
    return {};
  std::filesystem::path resolved = PathUtils::PathFromUtf8(it->second.gdtfSpec);
  if (resolved.is_relative() && !scene.basePath.empty())
    resolved = PathUtils::PathFromUtf8(scene.basePath) / resolved;
  return IsExistingRegularFile(resolved) ? PathUtils::PathToUtf8(resolved) : std::string();
}

// Updates the read-only GDTF metadata section from the current GDTF.
void TrussEditDialog::UpdateMetadataSummary() {
  GdtfMetadataSummary metadata;
  if (LoadGdtfMetadataSummary(ResolveCurrentGdtfPath(), metadata)) {
    if (gdtfEditorPanel)
      gdtfEditorPanel->SetMetadata(metadata);
  } else if (gdtfEditorPanel) {
    gdtfEditorPanel->SetMetadataUnavailable();
  }
  Layout();
}

// Updates the embedded 3D preview from the best available renderable resource.
void TrussEditDialog::UpdatePreview() {
  if (!preview || !panel || row < 0 ||
      static_cast<size_t>(row) >= panel->rowUuids.size())
    return;

  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.trusses.end()) {
    preview->LoadResource({});
    return;
  }

  preview->LoadResource(
      gui::ResolveTrussPreviewResourcePath(it->second, scene.basePath));
}

// Applies edited control values to the table and scene data model.
bool TrussEditDialog::ApplyChanges() {
  if (!panel || !panel->table)
    return true;
  SyncSessionDirtyToLegacyFlags();
  if (!ValidateSessionBeforeApply())
    return false;

  const bool hasTableChanges =
      std::any_of(modifiedColumns.begin(), modifiedColumns.end(),
                  [](bool modified) { return modified; });
  if (!hasTableChanges && !crossSectionModified)
    return true;

  bool gdtfColumnChanged = crossSectionModified;
  gdtf::ProjectTrussGdtfApplyResult trussApplyResult;
  for (size_t i = 0; i < modifiedColumns.size(); ++i)
    gdtfColumnChanged =
        gdtfColumnChanged || (modifiedColumns[i] && IsGdtfTrussColumn(i));

  if (gdtfColumnChanged && gdtfEditSession) {
    auto request = gdtfEditSession->BuildApplyRequest();
    if (!request.changedDocumentFields.empty() || !request.changedContextFields.empty()) {
      const auto &scene =
          GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
      gdtf::ProjectTrussGdtfApplyAdapter adapter(gui::MakeTrussGdtfApplyServices());
      gdtf::ProjectTrussGdtfApplyInput applyInput;
      applyInput.request = request;
      applyInput.trusses = &scene.trusses;
      applyInput.projectResourceBasePath = PathUtils::PathFromUtf8(scene.basePath);
      applyInput.outputRoot =
          PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses"));
      trussApplyResult = adapter.Apply(applyInput);
      if (!trussApplyResult.common.success) {
        const std::string message = trussApplyResult.common.diagnostics.empty()
                                        ? "Could not apply truss GDTF changes."
                                        : trussApplyResult.common.diagnostics.front();
        wxMessageBox(wxString::FromUTF8(message), _("Truss GDTF"),
                     wxOK | wxICON_WARNING, this);
        return false;
      }
    }
  }

  for (size_t i = 0; i < ctrls.size(); ++i) {
    if (!modifiedColumns[i])
      continue;
    if (i == static_cast<size_t>(ColumnIndex(TrussColumn::Manufacturer)) &&
        gdtfEditorPanel) {
      auto value = gdtfEditorPanel->GetIdentityValue(
          GdtfTypeIdentityField::Manufacturer);
      panel->table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          static_cast<unsigned int>(i));
    } else if (i == static_cast<size_t>(ColumnIndex(TrussColumn::Model)) &&
               gdtfEditorPanel) {
      auto value = gdtfEditorPanel->GetIdentityValue(GdtfTypeIdentityField::ModelName);
      panel->table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          static_cast<unsigned int>(i));
    } else if (gdtfEditorPanel && IsGdtfTrussColumn(i)) {
      const auto field = i == static_cast<size_t>(ColumnIndex(TrussColumn::Length))
                             ? GdtfPhysicalPropertyField::Length
                         : i == static_cast<size_t>(ColumnIndex(TrussColumn::Width))
                             ? GdtfPhysicalPropertyField::Width
                         : i == static_cast<size_t>(ColumnIndex(TrussColumn::Height))
                             ? GdtfPhysicalPropertyField::Height
                             : GdtfPhysicalPropertyField::Weight;
      auto value = gdtfEditorPanel->GetPhysicalPropertyValue(field);
      panel->table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          static_cast<unsigned int>(i));
    } else if (auto *control = wxDynamicCast(ctrls[i], wxTextCtrl)) {
      panel->table->SetValue(wxVariant(control->GetValue()), row,
                             static_cast<unsigned int>(i));
    }
  }


  panel->UpdateSceneData(true);

  if (trussApplyResult.resultingTruss && !hasTableChanges)
    GetDefaultGuiConfigServices().LegacyConfigManager().PushUndoState(
        "edit truss");

  if (!trussApplyResult.resultingTrusses.empty()) {
    auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    for (const auto &[uuid, truss] : trussApplyResult.resultingTrusses)
      scene.trusses[uuid] = truss;
  } else if (trussApplyResult.resultingTruss && row >= 0 &&
             static_cast<size_t>(row) < panel->rowUuids.size()) {
    auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    scene.trusses[panel->rowUuids[static_cast<size_t>(row)]] =
        *trussApplyResult.resultingTruss;
  }

  UpdateMetadataSummary();
  UpdatePreview();
  panel->ReloadData();
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  } else if (Viewer2DPanel::Instance()) {
    Viewer2DPanel::Instance()->UpdateScene();
  }

  std::fill(modifiedColumns.begin(), modifiedColumns.end(), false);
  crossSectionModified = false;
  if (gdtfEditSession)
    BuildEditSession();
  applied = true;
  return true;
}
