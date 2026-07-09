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
#include "fixtureeditdialog.h"
#include "configmanager.h"
#include "filesystem_path_utils.h"
#include "fixturepreviewpanel.h"
#include "fixturetable/fixture_table_columns.h"
#include "fixturetablepanel.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "gdtf/gdtf_editor_panel.h"
#include "gdtf/gdtf_session_panel_binding.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_metadata_summary.h"
#include "gdtf/editor/gdtf_document.h"
#include "gdtf/editor/project_fixture_gdtf_context.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "hoist_load_recalculation_prompt.h"
#include "projectutils.h"
#include "symbolcache.h"
#include "symbols/PerastageSvgSymbol.h"
#include "units/units.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <set>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>
#include <wx/clrpicker.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

// Checks whether a path resolves inside a candidate parent directory.
bool IsPathInsideDirectory(const std::filesystem::path &path,
                           const std::filesystem::path &directory) {
  if (path.empty() || directory.empty())
    return false;

  std::error_code ec;
  const std::filesystem::path canonicalPath =
      std::filesystem::weakly_canonical(path, ec);
  if (ec)
    return false;
  ec.clear();
  const std::filesystem::path canonicalDirectory =
      std::filesystem::weakly_canonical(directory, ec);
  if (ec)
    return false;

  auto pathIt = canonicalPath.begin();
  auto dirIt = canonicalDirectory.begin();
  for (; dirIt != canonicalDirectory.end(); ++dirIt, ++pathIt) {
    if (pathIt == canonicalPath.end() || *pathIt != *dirIt)
      return false;
  }
  return true;
}

// Checks whether a GDTF path belongs to the writable user fixture library.
bool IsUserFixtureLibraryPath(const std::string &path) {
  const std::filesystem::path candidate = PathUtils::PathFromUtf8(path);
  return IsPathInsideDirectory(
      candidate, PathUtils::PathFromUtf8(
                     ProjectUtils::GetWritableLibraryPath("fixtures")));
}

// Checks whether a path points to an existing regular file without throwing.
bool IsExistingRegularFile(const std::filesystem::path &path) {
  if (path.empty())
    return false;
  std::error_code ec;
  return std::filesystem::exists(path, ec) && !ec &&
         std::filesystem::is_regular_file(path, ec) && !ec;
}

// Converts a wx path string to a filesystem path for operational I/O.
std::filesystem::path PathFromWxString(const wxString &path) {
  return PathUtils::PathFromUtf8(std::string(path.ToUTF8()));
}

// Checks whether two fixture records use the same GDTF type-level physical
// values.
bool MatchesPhysicalPropertyType(const Fixture &fixture,
                                 const std::string &gdtfSpec,
                                 const std::string &typeName) {
  if (!gdtfSpec.empty() && fixture.gdtfSpec == gdtfSpec)
    return true;
  return !typeName.empty() && fixture.typeName == typeName;
}

// Mirrors a GDTF physical-property edit to every row and fixture of the same
// type.
std::unordered_set<std::string> ApplySharedPhysicalPropertyEdit(
    wxDataViewListCtrl *table, const std::vector<std::string> &rowUuids,
    const std::string &sourceUuid, float weightKg, float powerW) {
  std::unordered_set<std::string> changedWeightPositions;
  if (!table || sourceUuid.empty())
    return changedWeightPositions;

  auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto sourceIt = scene.fixtures.find(sourceUuid);
  if (sourceIt == scene.fixtures.end())
    return changedWeightPositions;

  const std::string gdtfSpec = sourceIt->second.gdtfSpec;
  const std::string typeName = sourceIt->second.typeName;
  const auto weightUnitSystem = Units::ParseWeightUnitSystem(
      GetDefaultGuiConfigServices().LegacyConfigManager().GetValue(
          "ui_weight_unit_system"));
  const wxVariant powerValue(wxString::Format("%.1f", powerW));
  const wxVariant weightValue(
      wxString::FromUTF8(Units::FormatWeightFromKilograms(
      weightKg, weightUnitSystem, Units::ValueFormatContext::Table)));

  const size_t count =
      std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
  for (size_t rowIndex = 0; rowIndex < count; ++rowIndex) {
    const auto fixtureIt = scene.fixtures.find(rowUuids[rowIndex]);
    if (fixtureIt == scene.fixtures.end() ||
        !MatchesPhysicalPropertyType(fixtureIt->second, gdtfSpec, typeName))
      continue;

    if (!Units::NearlyEqualWeightKilograms(fixtureIt->second.weightKg, weightKg,
                                           0.001))
      changedWeightPositions.insert(fixtureIt->second.positionName.empty()
                                        ? "Unassigned"
                                        : fixtureIt->second.positionName);
    fixtureIt->second.weightKg = weightKg;
    fixtureIt->second.powerConsumptionW = powerW;
    fixtureIt->second.physicalPropertiesSource =
        FixturePhysicalPropertiesSource::Gdtf;
    fixtureIt->second.physicalPropertiesDirty = false;
    table->SetValue(powerValue, rowIndex, 16);
    table->SetValue(weightValue, rowIndex, 17);
  }
  return changedWeightPositions;
}

// Parses a floating-point value while preserving the previous value on failure.
bool ParseFloatOrDefault(const wxString &text, float &out) {
  double parsed = 0.0;
  if (!text.ToDouble(&parsed))
    return false;
  out = static_cast<float>(parsed);
  return true;
}

// Updates a fixture table row with a rendered color swatch cell.
void SetFixtureColorCell(wxDataViewListCtrl *table, int row,
                         const std::string &hexColor) {
  if (!table || row == wxNOT_FOUND || hexColor.empty())
    return;
  wxBitmap colorSwatch(16, 16);
  {
    wxMemoryDC dc(colorSwatch);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(wxString::FromUTF8(hexColor))));
    dc.DrawRectangle(0, 0, 16, 16);
    dc.SelectObject(wxNullBitmap);
  }
  wxVariant colorValue;
  colorValue << wxDataViewIconText(wxString::FromUTF8(hexColor), colorSwatch);
  table->SetValue(colorValue, row, 19);
}

// Loads the thumbnail bitmap from a GDTF archive when available.
bool LoadGdtfThumbnail(const std::string &gdtfPath, wxBitmap &outBitmap) {
  if (gdtfPath.empty())
    return false;

  wxFileInputStream input(wxString::FromUTF8(gdtfPath));
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::unordered_map<std::string, std::string> entries;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    wxString name = entry->GetName();
    std::string content;
    char buffer[4096];
    while (true) {
      zipInput.Read(buffer, sizeof(buffer));
      size_t count = zipInput.LastRead();
      if (count == 0)
        break;
      content.append(buffer, buffer + count);
    }
    entries.emplace(std::string(name.ToUTF8()), std::move(content));
  }

  auto descriptionIt = entries.find("description.xml");
  if (descriptionIt == entries.end())
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionIt->second.c_str(), descriptionIt->second.size()) !=
      tinyxml2::XML_SUCCESS) {
    return false;
  }

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  else
    fixtureType = doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  const char *thumbnailAttr = fixtureType->Attribute("Thumbnail");
  std::string thumbnailBase = thumbnailAttr ? thumbnailAttr : "";

  std::vector<std::string> candidates;
  if (!thumbnailBase.empty()) {
    candidates.push_back(thumbnailBase);
    candidates.push_back(thumbnailBase + ".png");
    candidates.push_back(thumbnailBase + ".jpg");
    candidates.push_back(thumbnailBase + ".jpeg");
    candidates.push_back("thumbnails/" + thumbnailBase + ".png");
    candidates.push_back("thumbnails/" + thumbnailBase + ".jpg");
    candidates.push_back("thumbnails/" + thumbnailBase + ".jpeg");
  }
  candidates.push_back("thumbnail.png");
  candidates.push_back("thumbnail.jpg");
  candidates.push_back("thumbnail.jpeg");

  for (const auto &candidate : candidates) {
    auto it = entries.find(candidate);
    if (it == entries.end())
      continue;
    wxMemoryInputStream stream(it->second.data(), it->second.size());
    wxImage image;
    if (!image.LoadFile(stream, wxBITMAP_TYPE_ANY))
      continue;
    constexpr int kPreviewSize = 220;
    const int srcW = std::max(1, image.GetWidth());
    const int srcH = std::max(1, image.GetHeight());
    const double scale = std::min(static_cast<double>(kPreviewSize) / srcW,
                                  static_cast<double>(kPreviewSize) / srcH);
    const int dstW = std::max(1, static_cast<int>(std::round(srcW * scale)));
    const int dstH = std::max(1, static_cast<int>(std::round(srcH * scale)));
    if (dstW != srcW || dstH != srcH)
      image.Rescale(dstW, dstH, wxIMAGE_QUALITY_HIGH);

    wxImage canvas(kPreviewSize, kPreviewSize);
    canvas.SetRGB(wxRect(0, 0, kPreviewSize, kPreviewSize), 255, 255, 255);
    canvas.Paste(image, (kPreviewSize - image.GetWidth()) / 2,
                 (kPreviewSize - image.GetHeight()) / 2);
    outBitmap = wxBitmap(canvas);
    return outBitmap.IsOk();
  }
  return false;
}


} // namespace

// Destroys the host-owned GDTF edit session after the dialog closes.
FixtureEditDialog::~FixtureEditDialog() = default;

// Builds the host-owned GDTF edit session from current fixture row state.
void FixtureEditDialog::BuildEditSession() {
  if (!panel || row < 0 || static_cast<size_t>(row) >= panel->rowUuids.size())
    return;
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.fixtures.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.fixtures.end())
    return;
  const Fixture &fixture = it->second;

  std::filesystem::path resolvedPath;
  if (static_cast<size_t>(row) < panel->gdtfPaths.size()) {
    const auto rowPath = PathFromWxString(panel->gdtfPaths[static_cast<size_t>(row)]);
    if (IsExistingRegularFile(rowPath))
      resolvedPath = rowPath;
  }

  gui::fixtures::FixtureGdtfResolution resolution;
  std::string resolutionError;
  if (resolvedPath.empty() &&
      gui::fixtures::ResolveFixtureGdtfDeterministic(
          fixture, scene, resolution, resolutionError,
          "FixtureEditDialog::BuildEditSession")) {
    resolvedPath = PathUtils::PathFromUtf8(resolution.selectedPath);
  }
  if (resolvedPath.empty() && !resolutionError.empty())
    wxLogWarning("%s", resolutionError.c_str());

  gdtf::ProjectFixtureGdtfContextInput input;
  input.fixture = fixture;
  input.resolvedGdtfPath = resolvedPath;
  input.editorSourceFileReference =
      !resolvedPath.empty() ? resolvedPath.string() : fixture.gdtfSpec;
  if (IsExistingRegularFile(input.resolvedGdtfPath))
    input.document = gdtf::LoadGdtfDocument(input.resolvedGdtfPath);
  input.sourceKind = IsUserFixtureLibraryPath(input.resolvedGdtfPath.string())
                         ? gdtf::GdtfSourceKind::PerastageFixtureLibraryFile
                         : gdtf::GdtfSourceKind::Unknown;
  input.writePolicy = gdtf::GdtfWritePolicy::CreateDerivativeBeforeMutation;
  gdtfEditSession = std::make_unique<gdtf::GdtfEditSession>(
      gdtf::BuildProjectFixtureGdtfEditSession(input));
}

// Returns the active resolved GDTF path used for all Fixture Edit file I/O.
std::filesystem::path FixtureEditDialog::GetActiveResolvedGdtfPath() const {
  if (gdtfEditSession) {
    const auto &path = gdtfEditSession->Context().sourcePath;
    if (IsExistingRegularFile(path))
      return path;
  }
  if (IsExistingRegularFile(pendingSelectedGdtfPath))
    return pendingSelectedGdtfPath;
  if (panel && row >= 0 && static_cast<size_t>(row) < panel->gdtfPaths.size()) {
    const auto rowPath = PathFromWxString(panel->gdtfPaths[static_cast<size_t>(row)]);
    if (IsExistingRegularFile(rowPath))
      return rowPath;
  }
  if (!panel || row < 0 || static_cast<size_t>(row) >= panel->rowUuids.size())
    return {};
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto it = scene.fixtures.find(panel->rowUuids[static_cast<size_t>(row)]);
  if (it == scene.fixtures.end())
    return {};
  gui::fixtures::FixtureGdtfResolution resolution;
  std::string resolutionError;
  if (gui::fixtures::ResolveFixtureGdtfDeterministic(
          it->second, scene, resolution, resolutionError,
          "FixtureEditDialog::GetActiveResolvedGdtfPath"))
    return PathUtils::PathFromUtf8(resolution.selectedPath);
  if (!resolutionError.empty())
    wxLogWarning("%s", resolutionError.c_str());
  return {};
}

FixtureEditDialog::FixtureEditDialog(FixtureTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Fixture", wxDefaultPosition,
               wxSize(900, 740)),
      panel(p), row(r) {
  BuildEditSession();
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *hSizer = new wxBoxSizer(wxHORIZONTAL);
  wxStaticBoxSizer *fixtureSpecificSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Fixture-specific");
  wxStaticBoxSizer *gdtfGeneralSizer = new wxStaticBoxSizer(
      wxVERTICAL, this, "GDTF (shared for this fixture type)");
  wxFlexGridSizer *fixtureGrid = new wxFlexGridSizer(2, 5, 5);
  fixtureGrid->AddGrowableCol(1, 1);
  wxWindow *fixtureSpecificParent = fixtureSpecificSizer->GetStaticBox();

  auto *table = panel->table; // friend access
  ctrls.resize(panel->columnLabels.size(), nullptr);
  modifiedColumns.assign(panel->columnLabels.size(), false);

  wxVariant initType;
  table->GetValue(initType, row, 2);
  originalType = initType.GetString();

  const std::set<size_t> gdtfColumns = {};
  auto addLabeledControl = [&](size_t index, wxWindow *controlWindow,
                               wxSizer *nestedSizer, bool isGdtfField) {
    (void)isGdtfField;
    fixtureGrid->Add(new wxStaticText(fixtureSpecificParent, wxID_ANY,
                                       panel->columnLabels[index]),
                     0, wxALIGN_CENTER_VERTICAL);
    if (nestedSizer)
      fixtureGrid->Add(nestedSizer, 1, wxEXPAND);
    else
      fixtureGrid->Add(controlWindow, 1, wxEXPAND);
  };

  for (size_t i = 0; i < panel->columnLabels.size(); ++i) {
    wxVariant v;
    table->GetValue(v, row, i);
    wxWindow *controlWindow = nullptr;
    wxSizer *nestedSizer = nullptr;
    if (i == 2 || i == 7 || i == 8 || i == 9 || i == 16 || i == 17) {
      continue;
    } else if (i == 18) {
      auto *category = new wxChoice(fixtureSpecificParent, wxID_ANY);
      const wxArrayString values = {
          "Beam",         "Blinder", "Conventional", "FX",    "Hoist",
          "Hybrid",       "Laser",   "LED",          "Smoke", "Spot",
          "Strobe",       "Unknown", "Video",        "Wash"};
      for (const auto &entry : values)
        category->Append(entry);
      int selection = category->FindString(v.GetString());
      if (selection != wxNOT_FOUND)
        category->SetSelection(selection);
      category->Bind(wxEVT_CHOICE,
                     [this, i](wxCommandEvent &) { MarkColumnModified(i); });
      ctrls[i] = category;
      controlWindow = category;
    } else if (i == static_cast<size_t>(FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::VisualColor)) ||
               i == static_cast<size_t>(FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::MvrColor))) {
      wxString colorString;
      if (v.GetType() == "wxDataViewIconText") {
        wxDataViewIconText icon;
        icon << v;
        colorString = icon.GetText();
      } else {
        colorString = v.GetString();
      }
      wxColour initial(colorString);
      if (colorString.IsEmpty() || !initial.IsOk())
        initial = *wxWHITE;
      auto *picker =
          new wxColourPickerCtrl(fixtureSpecificParent, wxID_ANY, initial);
      picker->Bind(wxEVT_COLOURPICKER_CHANGED,
                   [this, i](wxColourPickerEvent &) { MarkColumnModified(i); });
      ctrls[i] = picker;
      controlWindow = picker;
    } else {
      wxTextCtrl *tc =
          new wxTextCtrl(fixtureSpecificParent, wxID_ANY, v.GetString());
      tc->Bind(wxEVT_TEXT,
               [this, i](wxCommandEvent &) { MarkColumnModified(i); });
      ctrls[i] = tc;
      controlWindow = tc;
    }
    addLabeledControl(i, controlWindow, nestedSizer, gdtfColumns.count(i) > 0);
  }


  const std::filesystem::path initialGdtfPath = GetActiveResolvedGdtfPath();

  wxVariant typeValue;
  table->GetValue(typeValue, row, 2);
  wxVariant powerValue;
  wxVariant weightValue;
  table->GetValue(powerValue, row, FixtureTableColumns::ToIndex(
                                FixtureTableColumns::Column::Power));
  table->GetValue(weightValue, row, FixtureTableColumns::ToIndex(
                                 FixtureTableColumns::Column::Weight));
  ParseFloatOrDefault(powerValue.GetString(), originalPowerW);
  ParseFloatOrDefault(weightValue.GetString(), originalWeightKg);
  wxVariant modeValue;
  table->GetValue(modeValue, row, FixtureTableColumns::ToIndex(
                               FixtureTableColumns::Column::Mode));

  gdtfEditorPanel = new GdtfEditorPanel(gdtfGeneralSizer->GetStaticBox());
  GdtfEditorPanelConfiguration gdtfConfiguration;
  gdtfConfiguration.layout = GdtfEditorPanelLayout::SingleColumn;
  gdtfConfiguration.metadata.title = "GDTF metadata";
  gdtfConfiguration.typeIdentity.title = "Fixture type";
  gdtfConfiguration.physicalProperties.title = "Physical properties";
  gdtfConfiguration.modes.title = "Modes and channels";
  gdtfEditorPanel->Configure(gdtfConfiguration);
  const auto &sessionValues =
      gdtfEditSession ? gdtfEditSession->CurrentValues()
                      : gdtf::GdtfEditableValues{};
  gdtfEditorPanel->SetPresentation({
      false,
      {},
      {
          {GdtfTypeIdentityField::FixtureTypeName, "Type",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::FixtureTypeName,
               std::string(typeValue.GetString().ToUTF8())),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::FixtureTypeName)},
          {GdtfTypeIdentityField::SourceFileReference, "Model file",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::SourceFileReference,
               initialGdtfPath.string()),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::SourceFileReference),
           true, "..."},
      },
      {
          {GdtfPhysicalPropertyField::PowerConsumption, "Power",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::PowerConsumption,
               std::string(powerValue.GetString().ToUTF8())),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(
                   *gdtfEditSession, gdtf::GdtfFieldId::PowerConsumption),
           "W"},
          {GdtfPhysicalPropertyField::Weight, "Weight",
           gui::gdtf_binding::ValueText(
               sessionValues, gdtf::GdtfFieldId::Weight,
               std::string(weightValue.GetString().ToUTF8())),
           true,
           gdtfEditSession &&
               gui::gdtf_binding::IsEditable(*gdtfEditSession,
                                             gdtf::GdtfFieldId::Weight),
           "kg"},
      },
      {initialGdtfPath.empty() ? std::vector<std::string>()
                               : GetGdtfModes(initialGdtfPath.string()),
       gui::gdtf_binding::ValueText(
           sessionValues, gdtf::GdtfFieldId::ModeName,
           std::string(modeValue.GetString().ToUTF8())),
       std::string(),
       {}}});
  gdtfEditorPanel->SetIdentityChangeCallback(
      [this](GdtfTypeIdentityField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field))
          SetSessionValue(*fieldId, value);
      });
  gdtfEditorPanel->SetIdentityActionCallback(
      [this](GdtfTypeIdentityField field) {
        if (field == GdtfTypeIdentityField::SourceFileReference) {
          wxCommandEvent event;
          OnBrowse(event);
        }
      });
  gdtfEditorPanel->SetPhysicalPropertyChangeCallback(
      [this](GdtfPhysicalPropertyField field, const std::string &value) {
        if (auto fieldId = gui::gdtf_binding::ToFieldId(field))
          SetSessionValue(*fieldId, value);
      });
  gdtfEditorPanel->SetModeSelectionCallback([this](const std::string &value) {
    SetSessionValue(gdtf::GdtfFieldId::ModeName, value);
    UpdateChannels(true);
  });

  fixtureSpecificSizer->Add(fixtureGrid, 1, wxEXPAND | wxALL, 6);

  gdtfGeneralSizer->Add(gdtfEditorPanel, 1, wxEXPAND | wxALL, 6);
  gdtfGeneralSizer->Add(new wxStaticText(gdtfGeneralSizer->GetStaticBox(), wxID_ANY,
                                         "Type, source, and mode controls select project fixture context. Power and weight edits update the GDTF file and append a revision entry."),
                        0, wxLEFT | wxRIGHT | wxBOTTOM, 6);

  wxBoxSizer *formSizer = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer *leftColumnSizer = new wxBoxSizer(wxVERTICAL);
  leftColumnSizer->SetMinSize(wxSize(320, -1));
  leftColumnSizer->Add(fixtureSpecificSizer, 1, wxEXPAND);
  gdtfGeneralSizer->SetMinSize(wxSize(320, -1));
  formSizer->Add(leftColumnSizer, 1, wxRIGHT | wxEXPAND, 8);
  formSizer->Add(gdtfGeneralSizer, 1, wxLEFT | wxEXPAND, 8);
  formSizer->SetMinSize(wxSize(680, -1));
  hSizer->Add(formSizer, 3, wxALL | wxEXPAND, 10);

  wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
  preview = new FixturePreviewPanel(this);
  preview->SetMinSize(wxSize(240, 220));
  rightSizer->Add(preview, 1, wxEXPAND | wxBOTTOM, 5);

  wxStaticBoxSizer *symbolSizer =
      new wxStaticBoxSizer(wxHORIZONTAL, this, "Symbols");
  wxWindow *symbolParent = symbolSizer->GetStaticBox();
  const std::array<wxString, 3> symbolLabels = {"Top", "Front", "Side"};
  for (size_t i = 0; i < symbolPanels.size(); ++i) {
    wxBoxSizer *symbolColumn = new wxBoxSizer(wxVERTICAL);
    symbolColumn->Add(new wxStaticText(symbolParent, wxID_ANY, symbolLabels[i]),
                      0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 3);
    symbolPanels[i] = new wxPanel(symbolParent, wxID_ANY, wxDefaultPosition,
                                  wxSize(90, 70), wxBORDER_SIMPLE);
    symbolPanels[i]->SetBackgroundStyle(wxBG_STYLE_PAINT);
    symbolPanels[i]->Bind(wxEVT_PAINT, &FixtureEditDialog::OnSymbolPreviewPaint,
                          this);
    symbolColumn->Add(symbolPanels[i], 1, wxEXPAND);
    symbolSizer->Add(symbolColumn, 1, wxEXPAND | wxRIGHT, i < 2 ? 6 : 0);
  }
  rightSizer->Add(symbolSizer, 0, wxEXPAND | wxBOTTOM, 5);

  wxStaticBoxSizer *imageSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Fixture image");
  fixtureImagePreview = new wxStaticBitmap(imageSizer->GetStaticBox(), wxID_ANY,
                                           wxBitmap(220, 220));
  imageSizer->Add(fixtureImagePreview, 0, wxALIGN_CENTER | wxALL, 4);
  rightSizer->Add(imageSizer, 0, wxEXPAND | wxBOTTOM, 5);
  rightSizer->SetMinSize(wxSize(280, -1));

  hSizer->Add(rightSizer, 0, wxTOP | wxBOTTOM | wxRIGHT | wxEXPAND, 10);

  topSizer->Add(hSizer, 1, wxEXPAND);

  wxStdDialogButtonSizer *btns = new wxStdDialogButtonSizer();
  btns->AddButton(new wxButton(this, wxID_APPLY));
  btns->AddButton(new wxButton(this, wxID_OK));
  btns->AddButton(new wxButton(this, wxID_CANCEL));
  btns->Realize();
  topSizer->Add(btns, 0, wxALL | wxEXPAND, 10);

  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnApply, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnOk, this, wxID_OK);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnCancel, this, wxID_CANCEL);

  SetSizerAndFit(topSizer);
  SetMinSize(GetSize());
  UpdateChannels(false);
  UpdateVisualizers();
  UpdateMetadataSummary();
}

void FixtureEditDialog::MarkColumnModified(size_t index) {
  if (index < modifiedColumns.size())
    modifiedColumns[index] = true;
}

// Mirrors authoritative session dirty fields into legacy table-column flags.
void FixtureEditDialog::SyncSessionDirtyToLegacyFlags() {
  if (!gdtfEditSession)
    return;
  auto setFlag = [this](FixtureTableColumns::Column column,
                        gdtf::GdtfFieldId fieldId) {
    const size_t index =
        static_cast<size_t>(FixtureTableColumns::ToIndex(column));
    if (index < modifiedColumns.size())
      modifiedColumns[index] = gdtfEditSession->IsFieldDirty(fieldId);
  };
  setFlag(FixtureTableColumns::Column::Type, gdtf::GdtfFieldId::FixtureTypeName);
  setFlag(FixtureTableColumns::Column::Mode, gdtf::GdtfFieldId::ModeName);
  setFlag(FixtureTableColumns::Column::ModelFile,
          gdtf::GdtfFieldId::SourceFileReference);
  setFlag(FixtureTableColumns::Column::Power,
          gdtf::GdtfFieldId::PowerConsumption);
  setFlag(FixtureTableColumns::Column::Weight, gdtf::GdtfFieldId::Weight);
}

// Clears session validation tooltips from the GDTF editor presentation.
void FixtureEditDialog::ClearSessionValidation() {
  if (!gdtfEditorPanel)
    return;
  gdtfEditorPanel->SetIdentityFieldValidation(
      GdtfTypeIdentityField::FixtureTypeName, {});
  gdtfEditorPanel->SetPhysicalPropertyValidation(
      GdtfPhysicalPropertyField::PowerConsumption, {});
  gdtfEditorPanel->SetPhysicalPropertyValidation(
      GdtfPhysicalPropertyField::Weight, {});
}

// Stores a supported panel edit in the session and mirrors dirty state.
bool FixtureEditDialog::SetSessionValue(gdtf::GdtfFieldId fieldId,
                                        const std::string &value) {
  if (!gdtfEditSession)
    return false;
  ClearSessionValidation();
  const bool accepted = gdtfEditSession->SetValue(fieldId, value);
  if (!accepted) {
    hasRejectedSessionInput = true;
    if (fieldId == gdtf::GdtfFieldId::PowerConsumption)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::PowerConsumption,
          "Enter a valid numeric power value.");
    else if (fieldId == gdtf::GdtfFieldId::Weight)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Weight, "Enter a valid numeric weight.");
    SyncSessionDirtyToLegacyFlags();
    return false;
  }
  hasRejectedSessionInput = false;
  SyncSessionDirtyToLegacyFlags();
  return true;
}

// Validates session state before any legacy apply mutation starts.
bool FixtureEditDialog::ValidateSessionBeforeApply() {
  if (!gdtfEditSession)
    return true;
  ClearSessionValidation();
  if (hasRejectedSessionInput) {
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
    if (diagnostic.fieldId == gdtf::GdtfFieldId::FixtureTypeName)
      gdtfEditorPanel->SetIdentityFieldValidation(
          GdtfTypeIdentityField::FixtureTypeName, diagnostic.message);
    else if (diagnostic.fieldId == gdtf::GdtfFieldId::PowerConsumption)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::PowerConsumption, diagnostic.message);
    else if (diagnostic.fieldId == gdtf::GdtfFieldId::Weight)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Weight, diagnostic.message);
  }
  wxMessageBox(wxString::FromUTF8(message), "GDTF validation",
               wxOK | wxICON_WARNING, this);
  return false;
}

void FixtureEditDialog::OnBrowse(wxCommandEvent &) {
  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fdlg.ShowModal() != wxID_OK)
    return;
  wxString path = fdlg.GetPath();
  pendingSelectedGdtfPath = PathFromWxString(path);
  if (gdtfEditSession && panel && row >= 0 &&
      static_cast<size_t>(row) < panel->rowUuids.size()) {
    const auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    auto it = scene.fixtures.find(panel->rowUuids[static_cast<size_t>(row)]);
    if (it != scene.fixtures.end()) {
      gdtf::ProjectFixtureGdtfContextInput input;
      input.fixture = it->second;
      input.resolvedGdtfPath = pendingSelectedGdtfPath;
      input.editorSourceFileReference = input.resolvedGdtfPath.string();
      if (IsExistingRegularFile(input.resolvedGdtfPath))
        input.document = gdtf::LoadGdtfDocument(input.resolvedGdtfPath);
      input.sourceKind =
          IsUserFixtureLibraryPath(input.resolvedGdtfPath.string())
              ? gdtf::GdtfSourceKind::PerastageFixtureLibraryFile
              : gdtf::GdtfSourceKind::Unknown;
      input.writePolicy = gdtf::GdtfWritePolicy::CreateDerivativeBeforeMutation;
      gdtfEditSession->RebindContextPreservingValues(
          gdtf::BuildProjectFixtureGdtfEditorContext(input));
    }
  }
  gdtfEditorPanel->SetIdentityValue(GdtfTypeIdentityField::SourceFileReference, std::string(path.ToUTF8()));
  SetSessionValue(gdtf::GdtfFieldId::SourceFileReference,
                  std::string(path.ToUTF8()));
  if (preview)
    preview->LoadFixture(std::string(path.ToUTF8()));
  // update type/power/weight fields
  if (ctrls.size() > 2) {
    wxString typeName =
        wxString::FromUTF8(GetGdtfFixtureName(std::string(path.ToUTF8())));
    if (typeName.empty())
      typeName = fdlg.GetFilename();
    gdtfEditorPanel->SetIdentityValue(GdtfTypeIdentityField::FixtureTypeName, std::string(typeName.ToUTF8()));
    SetSessionValue(gdtf::GdtfFieldId::FixtureTypeName,
                    std::string(typeName.ToUTF8()));
    float w = 0.f, p = 0.f;
    GetGdtfProperties(std::string(path.ToUTF8()), w, p);
    if (ctrls.size() > 16)
      gdtfEditorPanel->SetPhysicalPropertyValue(
          GdtfPhysicalPropertyField::PowerConsumption,
          std::string(wxString::Format("%.1f", p).ToUTF8()));
    if (ctrls.size() > 16)
      SetSessionValue(gdtf::GdtfFieldId::PowerConsumption,
                      std::string(wxString::Format("%.1f", p).ToUTF8()));
    if (ctrls.size() > 17)
      gdtfEditorPanel->SetPhysicalPropertyValue(
          GdtfPhysicalPropertyField::Weight,
          std::string(wxString::Format("%.2f", w).ToUTF8()));
    if (ctrls.size() > 17)
      SetSessionValue(gdtf::GdtfFieldId::Weight,
                      std::string(wxString::Format("%.2f", w).ToUTF8()));
  }
  // repopulate modes
  if (gdtfEditorPanel) {
    auto modes = GetGdtfModes(std::string(path.ToUTF8()));
    gdtfEditorPanel->SetModes(modes);
    if (!modes.empty()) {
      gdtfEditorPanel->SetSelectedMode(modes.front());
      SetSessionValue(gdtf::GdtfFieldId::ModeName, modes.front());
    }
  }
  UpdateChannels(true);
  UpdateVisualizers();
  UpdateMetadataSummary();
}

void FixtureEditDialog::OnModeChanged(wxCommandEvent &) {
  UpdateChannels(true);
}

void FixtureEditDialog::OnSymbolPreviewPaint(wxPaintEvent &evt) {
  wxPanel *panelWindow = wxDynamicCast(evt.GetEventObject(), wxPanel);
  if (!panelWindow)
    return;

  int panelIndex = -1;
  for (size_t i = 0; i < symbolPanels.size(); ++i) {
    if (symbolPanels[i] == panelWindow) {
      panelIndex = static_cast<int>(i);
      break;
    }
  }
  if (panelIndex < 0)
    return;

  wxAutoBufferedPaintDC dc(panelWindow);
  dc.SetBackground(*wxWHITE_BRUSH);
  dc.Clear();

  if (!symbolAvailability[panelIndex]) {
    dc.SetTextForeground(*wxLIGHT_GREY);
    dc.DrawLabel("N/A", panelWindow->GetClientRect(), wxALIGN_CENTER);
    return;
  }

  wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
  if (!gc)
    return;

  const PerastageSvgSymbolData &svg = symbolData[panelIndex];
  wxRect rect = panelWindow->GetClientRect();
  const double scale =
      std::min((rect.GetWidth() - 8.0) / std::max(1.0, svg.viewBoxWidth),
      (rect.GetHeight() - 8.0) / std::max(1.0, svg.viewBoxHeight));
  const double originX =
      rect.GetX() + (rect.GetWidth() - svg.viewBoxWidth * scale) * 0.5;
  const double originY =
      rect.GetY() + (rect.GetHeight() - svg.viewBoxHeight * scale) * 0.5;

  gc->SetPen(wxPen(wxColour(210, 210, 210), 1));
  gc->SetBrush(*wxWHITE_BRUSH);
  gc->DrawRectangle(rect.GetX(), rect.GetY(), rect.GetWidth(),
                    rect.GetHeight());
  gc->SetPen(*wxTRANSPARENT_PEN);
  gc->SetBrush(wxBrush(wxColour(224, 224, 224)));
  for (const auto &poly : svg.fills) {
    if (poly.points.size() < 3)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(originX + poly.points[0].x * scale,
                     originY + poly.points[0].y * scale);
    for (size_t i = 1; i < poly.points.size(); ++i)
      path.AddLineToPoint(originX + poly.points[i].x * scale,
                          originY + poly.points[i].y * scale);
    path.CloseSubpath();
    gc->FillPath(path);
    gc->SetBrush(*wxWHITE_BRUSH);
    for (const auto &hole : poly.holes) {
      if (hole.size() < 3)
        continue;
      wxGraphicsPath holePath = gc->CreatePath();
      holePath.MoveToPoint(originX + hole[0].x * scale,
                           originY + hole[0].y * scale);
      for (size_t i = 1; i < hole.size(); ++i)
        holePath.AddLineToPoint(originX + hole[i].x * scale,
                                originY + hole[i].y * scale);
      holePath.CloseSubpath();
      gc->FillPath(holePath);
    }
    gc->SetBrush(wxBrush(wxColour(224, 224, 224)));
  }
  gc->SetPen(wxPen(wxColour(0, 0, 0), 1));
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(originX + line.points[0].x * scale,
                     originY + line.points[0].y * scale);
    for (size_t i = 1; i < line.points.size(); ++i)
      path.AddLineToPoint(originX + line.points[i].x * scale,
                          originY + line.points[i].y * scale);
    gc->StrokePath(path);
  }
  delete gc;
}

void FixtureEditDialog::UpdateVisualizers() {
  const std::string path = GetActiveResolvedGdtfPath().string();
  const std::array<SymbolViewKind, 3> views = {
      SymbolViewKind::Bottom, SymbolViewKind::Front, SymbolViewKind::Left};
  for (size_t i = 0; i < views.size(); ++i) {
    PerastageSvgSymbolData loaded;
    symbolAvailability[i] =
        LoadPerastageSvgSymbolFromGdtf(path, views[i], loaded);
    if (symbolAvailability[i])
      symbolData[i] = std::move(loaded);
    if (symbolPanels[i])
      symbolPanels[i]->Refresh();
  }

  if (fixtureImagePreview) {
    wxBitmap image;
    if (LoadGdtfThumbnail(path, image)) {
      fixtureImagePreview->SetBitmap(image);
      fixtureImagePreview->SetToolTip("");
    } else {
      wxBitmap fallback(220, 220);
      wxMemoryDC dc(fallback);
      dc.SetBackground(*wxLIGHT_GREY_BRUSH);
      dc.Clear();
      dc.SetTextForeground(*wxBLACK);
      dc.DrawLabel("No image", wxRect(0, 0, 220, 220), wxALIGN_CENTER);
      dc.SelectObject(wxNullBitmap);
      fixtureImagePreview->SetBitmap(fallback);
      fixtureImagePreview->SetToolTip("No thumbnail image found in this GDTF.");
    }
    Layout();
  }
}

void FixtureEditDialog::UpdateMetadataSummary() {
  const std::string path = GetActiveResolvedGdtfPath().string();
  GdtfMetadataSummary metadata;
  if (LoadGdtfMetadataSummary(path, metadata)) {
    if (gdtfEditorPanel)
      gdtfEditorPanel->SetMetadata(metadata);
  } else if (gdtfEditorPanel) {
    gdtfEditorPanel->SetMetadataUnavailable();
  }
  Layout();
}

void FixtureEditDialog::UpdateChannels(bool markChannelCountDirty) {
  const std::filesystem::path gdtfPath = GetActiveResolvedGdtfPath();
  wxString mode = gdtfEditorPanel ? wxString::FromUTF8(gdtfEditorPanel->GetSelectedMode()) : wxString();
  if (preview)
    preview->LoadFixture(gdtfPath.string());
  if (gdtfPath.empty() || mode.empty()) {
    if (gdtfEditorPanel)
      gdtfEditorPanel->ClearModeDetails();
    return;
  }
  auto channels =
      GetGdtfModeChannels(gdtfPath.string(), std::string(mode.ToUTF8()));
  std::vector<GdtfModeChannelPresentation> channelRows;
  for (const auto &ch : channels) {
    channelRows.push_back({
        ch.isVirtual ? std::string("V") :
                       std::string(wxString::Format("%d", ch.channel).ToUTF8()),
        FormatGdtfModeFunctionLabel(ch.function),
    });
  }
  if (gdtfEditorPanel)
    gdtfEditorPanel->SetChannels(channelRows);
  int chCount =
      GetGdtfModeChannelCount(gdtfPath.string(), std::string(mode.ToUTF8()));
  if (gdtfEditorPanel)
    gdtfEditorPanel->SetChannelCount(chCount >= 0 ? std::string(wxString::Format("%d", chCount).ToUTF8()) : std::string());
  if (markChannelCountDirty)
    MarkColumnModified(FixtureTableColumns::ToIndex(
        FixtureTableColumns::Column::ChannelCount));
}

void FixtureEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

void FixtureEditDialog::OnOk(wxCommandEvent &) {
  if (!ApplyChanges())
    return;
  EndModal(wxID_OK);
}

void FixtureEditDialog::OnCancel(wxCommandEvent &) { EndModal(wxID_CANCEL); }

bool FixtureEditDialog::ApplyChanges() {
  if (!panel)
    return true;
  SyncSessionDirtyToLegacyFlags();
  if (!ValidateSessionBeforeApply())
    return false;
  auto *table = panel->table;
  std::filesystem::path gdtfPath = GetActiveResolvedGdtfPath();

  std::vector<std::string> oldOrder = panel->rowUuids;
  std::vector<std::string> selectedUuids;
  if ((size_t)row < panel->rowUuids.size())
    selectedUuids.push_back(panel->rowUuids[row]);

  const bool hasUserChanges =
      std::any_of(modifiedColumns.begin(), modifiedColumns.end(),
                                          [](bool modified) { return modified; });
  if (!hasUserChanges)
    return true;

  for (size_t i = 0; i < ctrls.size(); ++i) {
    if (i >= modifiedColumns.size() || !modifiedColumns[i])
      continue;
    if (i == 7 && gdtfEditorPanel) {
      table->SetValue(wxVariant(wxString::FromUTF8(gdtfEditorPanel->GetSelectedMode())), row, i);
    } else if (i == 8 && gdtfEditorPanel) {
      int chCount = gdtfPath.empty()
                        ? -1
                        : GetGdtfModeChannelCount(
                              gdtfPath.string(),
                              gdtfEditorPanel->GetSelectedMode());
      table->SetValue(wxVariant(chCount >= 0 ? wxString::Format("%d", chCount)
                                             : wxString()),
                      row, i);
    } else if (i == 9 && gdtfEditorPanel) {
      wxFileName fn(wxString::FromUTF8(gdtfPath.string()));
      table->SetValue(wxVariant(fn.GetFullName()), row, i);
      if ((size_t)row >= panel->gdtfPaths.size())
        panel->gdtfPaths.resize(row + 1);
      panel->gdtfPaths[row] = wxString::FromUTF8(gdtfPath.string());
    } else if (i == 2 && gdtfEditorPanel) {
      auto value = gdtfEditorPanel->GetIdentityValue(
          GdtfTypeIdentityField::FixtureTypeName);
      table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          i);
    } else if (i == 16 && gdtfEditorPanel) {
      auto value = gdtfEditorPanel->GetPhysicalPropertyValue(
          GdtfPhysicalPropertyField::PowerConsumption);
      table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          i);
    } else if (i == 17 && gdtfEditorPanel) {
      auto value = gdtfEditorPanel->GetPhysicalPropertyValue(
          GdtfPhysicalPropertyField::Weight);
      table->SetValue(
          wxVariant(wxString::FromUTF8(value.value_or(std::string()).c_str())), row,
          i);
    } else if (i == 18) {
      auto *category = wxDynamicCast(ctrls[i], wxChoice);
      if (category)
        table->SetValue(wxVariant(category->GetStringSelection()), row, i);
    } else if (i == static_cast<size_t>(FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::VisualColor)) ||
               i == static_cast<size_t>(FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::MvrColor))) {
      auto *picker = wxDynamicCast(ctrls[i], wxColourPickerCtrl);
      if (picker) {
        wxColour selectedColor = picker->GetColour();
        wxString colorText = selectedColor.GetAsString(wxC2S_HTML_SYNTAX);
        wxBitmap colorSwatch(16, 16);
        {
          wxMemoryDC dc(colorSwatch);
          dc.SetPen(*wxTRANSPARENT_PEN);
          dc.SetBrush(wxBrush(selectedColor));
          dc.DrawRectangle(0, 0, 16, 16);
          dc.SelectObject(wxNullBitmap);
        }
        wxVariant colorValue;
        colorValue << wxDataViewIconText(colorText, colorSwatch);
        table->SetValue(colorValue, row, i);
      }
    } else {
      wxTextCtrl *tc = wxDynamicCast(ctrls[i], wxTextCtrl);
      if (tc) {
        wxString txt = tc->GetValue();
        if (i == 0 || i == 5 || i == 6) {
          long val = 0;
          txt.ToLong(&val);
          table->SetValue(wxVariant(val), row, i);
        } else {
          table->SetValue(wxVariant(txt), row, i);
        }
      }
    }
  }
  const bool gdtfMetadataChanged =
      (modifiedColumns.size() > 2 && modifiedColumns[2]) ||
      (modifiedColumns.size() > 7 && modifiedColumns[7]) ||
      (modifiedColumns.size() > 9 && modifiedColumns[9]) ||
      (modifiedColumns.size() > 18 && modifiedColumns[18]);
  const bool gdtfPhysicalCandidateChanged =
      (modifiedColumns.size() > 16 && modifiedColumns[16]) ||
      (modifiedColumns.size() > 17 && modifiedColumns[17]);
  const bool gdtfTypeOrModelChanged =
      (modifiedColumns.size() > 2 && modifiedColumns[2]) ||
      (modifiedColumns.size() > 9 && modifiedColumns[9]);
  const bool fixtureColorChanged =
      (modifiedColumns.size() >
           static_cast<size_t>(FixtureTableColumns::ToIndex(
               FixtureTableColumns::Column::VisualColor)) &&
       modifiedColumns[static_cast<size_t>(FixtureTableColumns::ToIndex(
           FixtureTableColumns::Column::VisualColor))]);

  if (gdtfTypeOrModelChanged && !fixtureColorChanged) {
    wxVariant typeVar;
    table->GetValue(typeVar, row, 2);
    const std::string currentType = std::string(typeVar.GetString().ToUTF8());
    if (auto dictEntry = GdtfDictionary::Get(currentType)) {
      if (!dictEntry->visualColorHex.empty())
        SetFixtureColorCell(table, row, dictEntry->visualColorHex);
    }
  }

  std::unordered_set<std::string> changedWeightPositions;

  if (!gdtfPath.empty()) {
    if (ctrls.size() > 17) {
      float newPowerW = originalPowerW;
      float newWeightKg = originalWeightKg;
      if (gdtfEditorPanel) {
        ParseFloatOrDefault(
            wxString::FromUTF8(gdtfEditorPanel
                                   ->GetPhysicalPropertyValue(GdtfPhysicalPropertyField::PowerConsumption)
                                   .value_or(std::string())),
            newPowerW);
        ParseFloatOrDefault(
            wxString::FromUTF8(gdtfEditorPanel
                                   ->GetPhysicalPropertyValue(GdtfPhysicalPropertyField::Weight)
                                   .value_or(std::string())),
            newWeightKg);
      }
      const bool gdtfPhysicalChanged =
          std::fabs(newPowerW - originalPowerW) > 0.01f ||
          std::fabs(newWeightKg - originalWeightKg) > 0.01f;
      if (gdtfPhysicalCandidateChanged && gdtfPhysicalChanged) {
        std::string writableGdtfPath = gdtfPath.string();
        if (IsUserFixtureLibraryPath(writableGdtfPath)) {
          auto derivative =
              GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
              std::string(originalType.ToUTF8()), writableGdtfPath);
          if (derivative && !derivative->path.empty()) {
            writableGdtfPath = derivative->path;
            gdtfPath = PathUtils::PathFromUtf8(derivative->path);
            pendingSelectedGdtfPath = gdtfPath;
            if (gdtfEditorPanel)
              gdtfEditorPanel->SetIdentityValue(
                  GdtfTypeIdentityField::SourceFileReference,
                  gdtfPath.string());
            if (gdtfEditSession)
              gdtfEditSession->SetValue(gdtf::GdtfFieldId::SourceFileReference,
                                        gdtfPath.string());
            if (panel && row >= 0) {
              if (static_cast<size_t>(row) >= panel->gdtfPaths.size())
                panel->gdtfPaths.resize(static_cast<size_t>(row) + 1);
              panel->gdtfPaths[static_cast<size_t>(row)] =
                  wxString::FromUTF8(gdtfPath.string());
              table->SetValue(wxVariant(wxFileName(wxString::FromUTF8(
                                            gdtfPath.string())).GetFullName()),
                              row, 9);
            }
          }
        }
        if (!SetGdtfProperties(writableGdtfPath, newWeightKg, newPowerW,
                               GdtfMutationAudit::BuildPerastageModifiedBy())) {
          wxMessageBox("Could not update GDTF physical properties "
                       "(Weight/PowerConsumption).",
              "GDTF update", wxOK | wxICON_WARNING, this);
        } else {
          if (row >= 0 && static_cast<size_t>(row) < panel->rowUuids.size())
            changedWeightPositions = ApplySharedPhysicalPropertyEdit(
                table, panel->rowUuids,
                panel->rowUuids[static_cast<size_t>(row)], newWeightKg,
                newPowerW);
          originalPowerW = newPowerW;
          originalWeightKg = newWeightKg;
        }
      }
    }

    if (gdtfMetadataChanged) {
      std::string mode =
          gdtfEditorPanel ? gdtfEditorPanel->GetSelectedMode() : std::string();
      // Project fixture metadata edits stay project-scoped and do not promote
      // files into the user library.
      panel->ApplyModeForGdtf(wxString::FromUTF8(gdtfPath.string()),
                              wxString::FromUTF8(mode.c_str()));
    }
  }
  if (fixtureColorChanged) {
    wxDataViewItemArray colorSource;
    colorSource.Add(table->RowToItem(row));
    panel->PropagateTypeValues(
        colorSource,
        FixtureTableColumns::ToIndex(
            FixtureTableColumns::Column::VisualColor));
  }
  panel->ResyncRows(oldOrder, selectedUuids);
  auto updateType = FixtureTablePanel::SceneDataUpdateType::kVisualLabelOnly;
  for (size_t i = 0; i < modifiedColumns.size(); ++i) {
    if (!modifiedColumns[i])
      continue;
    updateType = FixtureTablePanel::CombineUpdateTypes(
        updateType,
        FixtureTablePanel::UpdateTypeForColumn(static_cast<int>(i)));
  }
  panel->UpdateSceneData(true, updateType);
  HoistLoadRecalculationPrompt::PromptAndApply(
      GetDefaultGuiConfigServices().LegacyConfigManager(), panel,
      changedWeightPositions);
  applied = true;
  const bool requiresFullSceneUpdate =
      FixtureTablePanel::RequiresFullViewerSceneUpdate(updateType);
  if (Viewer3DPanel::Instance()) {
    if (requiresFullSceneUpdate) {
      Viewer3DPanel::Instance()->UpdateScene();
    }
    Viewer3DPanel::Instance()->Refresh();
  } else if (Viewer2DPanel::Instance()) {
    if (requiresFullSceneUpdate)
      Viewer2DPanel::Instance()->UpdateScene();
    else
      Viewer2DPanel::Instance()->UpdateScene(false);
  }
  std::fill(modifiedColumns.begin(), modifiedColumns.end(), false);
  if (gdtfEditSession) {
    BuildEditSession();
    SyncSessionDirtyToLegacyFlags();
  }
  return true;
}
