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
#include "gdtf/gdtf_editor_panel.h"
#include "gdtf_metadata_summary.h"
#include "gdtfdictionary.h"
#include "guiconfigservices.h"
#include "projectutils.h"
#include "preview_resource.h"
#include "table_column_indices.h"
#include "truss_gdtf_builder.h"
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

// Resolves a scene resource path to an absolute path for GDTF generation.
std::string ResolveTrussResourcePath(const std::string &basePath,
                                     const std::string &path) {
  namespace fs = std::filesystem;
  if (path.empty())
    return {};
  fs::path resolved(path);
  if (resolved.is_relative() && !basePath.empty())
    resolved = fs::path(basePath) / resolved;
  return resolved.string();
}
} // namespace

// Builds the truss editing dialog with MVR and GDTF fields.
TrussEditDialog::TrussEditDialog(TrussTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Truss", wxDefaultPosition, wxSize(980, 720)),
      panel(p), row(r) {
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
  gdtfEditorPanel->SetPresentation({
      false,
      {},
      {
          {GdtfTypeIdentityField::Manufacturer, "Manufacturer",
           tableValue(TrussColumn::Manufacturer)},
          {GdtfTypeIdentityField::ModelName, "Model",
           tableValue(TrussColumn::Model)},
      },
      {
          {GdtfPhysicalPropertyField::Length, "Length",
           tableValue(TrussColumn::Length)},
          {GdtfPhysicalPropertyField::Width, "Width",
           tableValue(TrussColumn::Width)},
          {GdtfPhysicalPropertyField::Height, "Height",
           tableValue(TrussColumn::Height)},
          {GdtfPhysicalPropertyField::Weight, "Weight",
           tableValue(TrussColumn::Weight)},
          {GdtfPhysicalPropertyField::CrossSection, "Cross section",
           std::string(crossSection.ToUTF8())},
      },
      {}});
  gdtfEditorPanel->SetIdentityChangeCallback(
      [this](GdtfTypeIdentityField field, const std::string &) {
        if (field == GdtfTypeIdentityField::Manufacturer)
          MarkColumnModified(ColumnIndex(TrussColumn::Manufacturer));
        else if (field == GdtfTypeIdentityField::ModelName)
          MarkColumnModified(ColumnIndex(TrussColumn::Model));
      });
  gdtfEditorPanel->SetPhysicalPropertyChangeCallback(
      [this](GdtfPhysicalPropertyField field, const std::string &) {
        if (field == GdtfPhysicalPropertyField::Length)
          MarkColumnModified(ColumnIndex(TrussColumn::Length));
        else if (field == GdtfPhysicalPropertyField::Width)
          MarkColumnModified(ColumnIndex(TrussColumn::Width));
        else if (field == GdtfPhysicalPropertyField::Height)
          MarkColumnModified(ColumnIndex(TrussColumn::Height));
        else if (field == GdtfPhysicalPropertyField::Weight)
          MarkColumnModified(ColumnIndex(TrussColumn::Weight));
        else if (field == GdtfPhysicalPropertyField::CrossSection)
          crossSectionModified = true;
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

// Applies edits without closing the dialog.
void TrussEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

// Applies edits and closes the dialog.
void TrussEditDialog::OnOk(wxCommandEvent &) {
  ApplyChanges();
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
  return ResolveTrussResourcePath(scene.basePath, it->second.gdtfSpec);
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

// Creates or refreshes the Perastage-authored truss GDTF.
bool TrussEditDialog::EnsureGdtfForEditedTruss() {
  if (!panel || row < 0 || static_cast<size_t>(row) >= panel->rowUuids.size())
    return false;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.trusses.end())
    return false;

  Truss exportTruss = it->second;
  exportTruss.symbolFile =
      ResolveTrussResourcePath(scene.basePath, exportTruss.symbolFile);
  exportTruss.modelFile =
      ResolveTrussResourcePath(scene.basePath, exportTruss.modelFile);

  const std::string canonicalFileName =
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
          exportTruss.manufacturer,
          exportTruss.model.empty() ? exportTruss.name : exportTruss.model,
          exportTruss.name);
  std::filesystem::path outPath =
      std::filesystem::path(ProjectUtils::GetWritableLibraryPath("trusses")) /
      canonicalFileName;

  std::string error;
  if (!BuildTrussGdtfFromInstance(exportTruss, outPath, &error)) {
    wxMessageBox(error.empty() ? "Failed to create truss GDTF." : error,
                 "Truss GDTF", wxOK | wxICON_WARNING, this);
    return false;
  }

  it->second.gdtfSpec = outPath.string();
  it->second.modelFile = outPath.string();
  it->second.perastageAuxGdtfArchivePath = canonicalFileName;
  it->second.gdtfMode =
      it->second.gdtfMode.empty() ? "Default" : it->second.gdtfMode;
  if (static_cast<size_t>(row) >= panel->modelPaths.size())
    panel->modelPaths.resize(static_cast<size_t>(row) + 1);
  panel->modelPaths[static_cast<size_t>(row)] =
      wxString::FromUTF8(outPath.string());
  panel->table->SetValue(
      wxVariant(wxString::FromUTF8(outPath.filename().string())), row,
      ColumnIndex(TrussColumn::ModelFile));
  return true;
}

// Applies edited control values to the table and scene data model.
void TrussEditDialog::ApplyChanges() {
  if (!panel || !panel->table)
    return;

  const bool hasTableChanges =
      std::any_of(modifiedColumns.begin(), modifiedColumns.end(),
                  [](bool modified) { return modified; });
  if (!hasTableChanges && !crossSectionModified)
    return;

  bool gdtfColumnChanged = crossSectionModified;
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

  panel->UpdateSceneData(true);

  if (crossSectionModified && row >= 0 &&
      static_cast<size_t>(row) < panel->rowUuids.size()) {
    auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    auto it = scene.trusses.find(panel->rowUuids[static_cast<size_t>(row)]);
    if (it != scene.trusses.end()) {
      if (!hasTableChanges)
        GetDefaultGuiConfigServices().LegacyConfigManager().PushUndoState(
            "edit truss GDTF metadata");
      auto value = gdtfEditorPanel
                       ? gdtfEditorPanel->GetPhysicalPropertyValue(
                             GdtfPhysicalPropertyField::CrossSection)
                       : std::optional<std::string>();
      it->second.crossSection = value.value_or(std::string());
    }
  }

  if (gdtfColumnChanged)
    EnsureGdtfForEditedTruss();

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
  applied = true;
}
