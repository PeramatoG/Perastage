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
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <algorithm>
#include <filesystem>
#include <optional>

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
    : wxDialog(p, wxID_ANY, "Edit Truss", wxDefaultPosition, wxSize(980, 720)),
      panel(p), row(r) {
  BuildEditSession();
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *contentSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *topRowSizer = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer *bottomRowSizer = new wxBoxSizer(wxHORIZONTAL);
  wxStaticBoxSizer *mvrSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "MVR instance");
  wxStaticBoxSizer *gdtfSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "GDTF truss type");
  wxWindow *mvrParent = mvrSizer->GetStaticBox();
  wxFlexGridSizer *mvrGrid = new wxFlexGridSizer(2, 5, 5);
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

  gdtfEditorPanel = new GdtfEditorPanel(gdtfSizer->GetStaticBox());
  GdtfEditorPanelConfiguration gdtfConfiguration;
  gdtfConfiguration.layout = GdtfEditorPanelLayout::SingleColumn;
  gdtfConfiguration.metadata.title = "GDTF metadata";
  gdtfConfiguration.typeIdentity.title = "Truss type";
  gdtfConfiguration.physicalProperties.title = "Physical properties";
  gdtfConfiguration.modes.visible = false;
  gdtfEditorPanel->Configure(gdtfConfiguration);
  const auto &sessionValues =
      gdtfEditSession ? gdtfEditSession->CurrentValues()
                      : gdtf::GdtfEditableValues{};
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
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussLength,
               tableValue(TrussColumn::Length)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussLength)},
          {GdtfPhysicalPropertyField::Width, "Width",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussWidth,
               tableValue(TrussColumn::Width)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussWidth)},
          {GdtfPhysicalPropertyField::Height, "Height",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussHeight,
               tableValue(TrussColumn::Height)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::TrussHeight)},
          {GdtfPhysicalPropertyField::Weight, "Weight",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::Weight,
               tableValue(TrussColumn::Weight)),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::Weight)},
          {GdtfPhysicalPropertyField::CrossSection, "Cross section",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::TrussCrossSection,
               std::string(crossSection.ToUTF8())),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::TrussCrossSection)},
      },
      {}});
  gdtfEditorPanel->SetIdentityChangeCallback(
      [this](GdtfTypeIdentityField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field))
          SetSessionValue(*fieldId, value);
      });
  gdtfEditorPanel->SetPhysicalPropertyChangeCallback(
      [this](GdtfPhysicalPropertyField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field))
          SetSessionValue(*fieldId, value);
      });

  mvrSizer->Add(mvrGrid, 1, wxEXPAND | wxALL, 6);
  gdtfSizer->Add(gdtfEditorPanel, 1, wxEXPAND | wxALL, 6);
  gdtfSizer->Add(
      new wxStaticText(gdtfSizer->GetStaticBox(), wxID_ANY,
                       "Editing these fields creates or updates the truss "
                       "GDTF. MVR-only fields remain project-scoped."),
      0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
  wxStaticBoxSizer *previewSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "3D preview");
  previewSizer->SetMinSize(wxSize(360, 190));
  preview = new FixturePreviewPanel(previewSizer->GetStaticBox());
  preview->SetMinSize(wxSize(320, 160));
  previewSizer->Add(preview, 1, wxEXPAND | wxALL, 6);

  topRowSizer->Add(previewSizer, 1, wxEXPAND | wxALL, 10);
  bottomRowSizer->Add(mvrSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  bottomRowSizer->Add(gdtfSizer, 1, wxEXPAND | wxRIGHT | wxBOTTOM, 10);
  contentSizer->Add(topRowSizer, 0, wxEXPAND);
  contentSizer->Add(bottomRowSizer, 1, wxEXPAND);
  topSizer->Add(contentSizer, 1, wxEXPAND);

  wxStdDialogButtonSizer *buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_APPLY));
  buttons->AddButton(new wxButton(this, wxID_OK));
  buttons->AddButton(new wxButton(this, wxID_CANCEL));
  buttons->Realize();
  topSizer->Add(buttons, 0, wxALL | wxEXPAND, 10);

  Bind(wxEVT_BUTTON, &TrussEditDialog::OnApply, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &TrussEditDialog::OnOk, this, wxID_OK);
  Bind(wxEVT_BUTTON, &TrussEditDialog::OnCancel, this, wxID_CANCEL);

  SetSizerAndFit(topSizer);
  SetMinSize(GetSize());
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
      gdtfEditSession->IsFieldDirty(gdtf::GdtfFieldId::TrussCrossSection);
}

// Clears session validation tooltips from the GDTF editor presentation.
void TrussEditDialog::ClearSessionValidation() {
  if (!gdtfEditorPanel)
    return;
  for (const auto field : {GdtfPhysicalPropertyField::Length,
                           GdtfPhysicalPropertyField::Width,
                           GdtfPhysicalPropertyField::Height,
                           GdtfPhysicalPropertyField::Weight,
                           GdtfPhysicalPropertyField::CrossSection})
    gdtfEditorPanel->SetPhysicalPropertyValidation(field, {});
}

// Stores a supported panel edit in the session and mirrors dirty state.
bool TrussEditDialog::SetSessionValue(gdtf::GdtfFieldId fieldId,
                                      const std::string &value) {
  if (!gdtfEditSession)
    return false;
  const bool accepted = gdtfEditSession->SetValue(fieldId, value);
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
    wxMessageBox("Fix malformed GDTF editor values before applying.",
                 "GDTF validation", wxOK | wxICON_WARNING, this);
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
  wxMessageBox(wxString::FromUTF8(message), "GDTF validation",
               wxOK | wxICON_WARNING, this);
  return false;
}

// Applies edits without closing the dialog.
void TrussEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

// Applies edits and closes the dialog.
void TrussEditDialog::OnOk(wxCommandEvent &) {
  if (!ApplyChanges())
    return;
  EndModal(wxID_OK);
}

// Closes the dialog without applying pending edits.
void TrussEditDialog::OnCancel(wxCommandEvent &) { EndModal(wxID_CANCEL); }

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
        wxMessageBox(wxString::FromUTF8(message), "Truss GDTF",
                     wxOK | wxICON_WARNING, this);
        return false;
      }
    }
  }

  panel->UpdateSceneData(true);

  if (trussApplyResult.resultingTruss && row >= 0 &&
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
