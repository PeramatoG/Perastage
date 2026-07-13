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
#include "fixture_gdtf_apply_services.h"
#include "fixturetable/fixture_table_columns.h"
#include "fixturetablepanel.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "gdtf/gdtf_color_cie.h"
#include "gdtf/gdtf_channel_summary_panel.h"
#include "gdtf/gdtf_editor_panel.h"
#include "gdtf/gdtf_wheel_inspector_panel.h"
#include "gdtf/gdtf_mode_browser_presenter.h"
#include "gdtf_archive_reader.h"
#include "gdtf/gdtf_editor_layout_preferences.h"
#include "gdtf/gdtf_editor_visual_metrics.h"
#include "gdtf_source_fingerprint.h"
#include "gdtf/gdtf_session_panel_binding.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_metadata_summary.h"
#include "gdtf/editor/gdtf_document.h"
#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"
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
#include <sstream>
#include <set>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>
#include <wx/bmpbndl.h>
#include <wx/clrpicker.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/splitter.h>
#include <wx/statbox.h>
#include <wx/wfstream.h>
#include <wx/statline.h>
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

// Resolves the themed background color used by preview placeholders and margins.
wxColour ResolvePreviewBackground(wxWindow *window) {
  for (wxWindow *current = window; current; current = current->GetParent()) {
    const wxColour colour = current->GetBackgroundColour();
    if (colour.IsOk())
      return colour;
  }
  return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}

// Draws a bitmap centered on a themed square preview canvas.
wxBitmap ComposePreviewBitmap(const wxBitmap &content, int size,
                              const wxColour &background) {
  wxBitmap canvas(size, size);
  wxMemoryDC dc(canvas);
  dc.SetBackground(wxBrush(background));
  dc.Clear();
  if (content.IsOk()) {
    dc.DrawBitmap(content, (size - content.GetWidth()) / 2,
                  (size - content.GetHeight()) / 2, true);
  }
  dc.SelectObject(wxNullBitmap);
  return canvas;
}

// Creates a themed placeholder bitmap with centered text.
wxBitmap CreatePreviewPlaceholder(const wxString &label, const wxColour &background) {
  constexpr int kPreviewSize = 220;
  wxBitmap fallback(kPreviewSize, kPreviewSize);
  wxMemoryDC dc(fallback);
  dc.SetBackground(wxBrush(background));
  dc.Clear();
  dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
  dc.DrawLabel(label, wxRect(0, 0, kPreviewSize, kPreviewSize), wxALIGN_CENTER);
  dc.SelectObject(wxNullBitmap);
  return fallback;
}

// Loads the thumbnail bitmap from a GDTF archive when available.
bool LoadGdtfThumbnail(const std::string &gdtfPath, const wxColour &background,
                       wxBitmap &outBitmap) {
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

    outBitmap = ComposePreviewBitmap(wxBitmap(image), kPreviewSize, background);
    return outBitmap.IsOk();
  }
  return false;
}

// Loads the official SVG thumbnail resource from the GDTF archive root.
bool LoadGdtfOfficialSvgSymbol(const std::string &gdtfPath,
                               const wxColour &background,
                               wxBitmap &outBitmap) {
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
  const std::string thumbnailBase = thumbnailAttr ? thumbnailAttr : "";
  if (thumbnailBase.empty())
    return false;

  std::vector<std::string> candidates;
  candidates.push_back(thumbnailBase);
  candidates.push_back(thumbnailBase + ".svg");
  for (const auto &candidate : candidates) {
    if (candidate.find('/') != std::string::npos ||
        candidate.find('\\') != std::string::npos)
      continue;
    auto it = entries.find(candidate);
    if (it == entries.end())
      continue;
    const wxSize desiredSize(220, 220);
    wxBitmapBundle bundle =
        wxBitmapBundle::FromSVG(it->second.c_str(), desiredSize);
    outBitmap = ComposePreviewBitmap(bundle.GetBitmap(desiredSize),
                                     desiredSize.GetWidth(), background);
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
      !resolvedPath.empty() ? PathUtils::PathToUtf8(resolvedPath) : fixture.gdtfSpec;
  if (IsExistingRegularFile(input.resolvedGdtfPath))
    input.document = gdtf::LoadGdtfDocument(input.resolvedGdtfPath);
  input.sourceKind =
      IsUserFixtureLibraryPath(PathUtils::PathToUtf8(input.resolvedGdtfPath))
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

// Builds the fixture editing dialog and arranges fixture, GDTF, and preview panels.
FixtureEditDialog::FixtureEditDialog(FixtureTablePanel *p, int r)
    : wxDialog(p, wxID_ANY, "Edit Fixture", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
      panel(p), row(r) {
  BuildEditSession();
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  auto *contentPanel = new wxPanel(this, wxID_ANY);
  auto *contentSizer = new wxBoxSizer(wxVERTICAL);
  contentPanel->SetSizer(contentSizer);
  contextSplitter = new wxSplitterWindow(contentPanel, wxID_ANY, wxDefaultPosition,
                                         wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3DSASH);
  auto *fixtureScroll = new wxScrolledWindow(contextSplitter, wxID_ANY);
  fixtureScroll->SetScrollRate(0, gui::gdtf_layout::Dip(this, 12));
  auto *fixtureSpecificSizer = new wxBoxSizer(wxVERTICAL);
  fixtureScroll->SetSizer(fixtureSpecificSizer);
  auto *fixtureTitle = new wxStaticText(fixtureScroll, wxID_ANY, "Fixture instance");
  wxFont fixtureTitleFont = fixtureTitle->GetFont();
  fixtureTitleFont.SetWeight(wxFONTWEIGHT_BOLD);
  fixtureTitle->SetFont(fixtureTitleFont);
  fixtureSpecificSizer->Add(fixtureTitle, 0, wxEXPAND | wxBOTTOM, gui::gdtf_layout::SectionPadding(this));
  wxFlexGridSizer *fixtureGrid = new wxFlexGridSizer(2, gui::gdtf_layout::CompactFieldGap(this), gui::gdtf_layout::CompactLabelGap(this));
  fixtureGrid->AddGrowableCol(1, 1);
  wxWindow *fixtureSpecificParent = fixtureScroll;

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

  auto *workspacePanel = new wxPanel(contextSplitter, wxID_ANY);
  auto *workspaceSizer = new wxBoxSizer(wxVERTICAL);
  workspacePanel->SetSizer(workspaceSizer);
  visualSplitter = new wxSplitterWindow(workspacePanel, wxID_ANY, wxDefaultPosition,
                                        wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3DSASH);
  auto *gdtfPanelHost = new wxPanel(visualSplitter, wxID_ANY);
  auto *gdtfHostSizer = new wxBoxSizer(wxVERTICAL);
  gdtfPanelHost->SetSizer(gdtfHostSizer);
  gdtfEditorPanel = new GdtfEditorPanel(gdtfPanelHost);
  GdtfEditorPanelConfiguration gdtfConfiguration;
  gdtfConfiguration.layout = GdtfEditorPanelLayout::TwoPane;
  gdtfConfiguration.twoPaneInitialRatio = 0.45;
  gdtfConfiguration.twoPaneOrder = {
      {GdtfEditorPane::Overview, GdtfEditorSection::TypeIdentity, 0},
      {GdtfEditorPane::Overview, GdtfEditorSection::Metadata, 1},
      {GdtfEditorPane::Overview, GdtfEditorSection::PhysicalProperties, 0},
      {GdtfEditorPane::Workspace, GdtfEditorSection::Modes, 1}};
  gdtfConfiguration.metadata.title = "GDTF metadata";
  gdtfConfiguration.typeIdentity.title = "Fixture type";
  gdtfConfiguration.physicalProperties.title = "Physical properties";
  gdtfConfiguration.channelSummary.visible = false;
  gdtfConfiguration.channelSummary.title = "Mode channels";
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
               PathUtils::PathToUtf8(initialGdtfPath)),
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
                               : GetGdtfModes(PathUtils::PathToUtf8(initialGdtfPath)),
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
  gdtfEditorPanel->SetWheelInspectionCallback([this](const GdtfWheelInspectorPresentation &presentation) {
    if (gdtfWheelInspectorPanel)
      gdtfWheelInspectorPanel->SetPresentation(BuildWheelInspectorVisualPresentation(presentation));
  });

  fixtureSpecificSizer->Add(fixtureGrid, 0, wxEXPAND | wxALL, gui::gdtf_layout::SectionPadding(this));
  auto *modeChannelsSizer = new wxStaticBoxSizer(wxVERTICAL, fixtureScroll,
                                                 "Mode channels");
  fixtureChannelSummaryPanel = new GdtfChannelSummaryPanel(fixtureScroll);
  modeChannelsSizer->Add(fixtureChannelSummaryPanel, 1, wxEXPAND | wxALL,
                         gui::gdtf_layout::SectionPadding(this));
  fixtureSpecificSizer->Add(modeChannelsSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                            gui::gdtf_layout::SectionPadding(this));
  fixtureScroll->SetMinSize(wxSize(gui::gdtf_layout::MinimumContextPaneWidth(this), -1));

  gdtfHostSizer->Add(gdtfEditorPanel, 1, wxEXPAND | wxALL, gui::gdtf_layout::SectionPadding(this));
  gdtfHostSizer->Add(new wxStaticText(gdtfPanelHost, wxID_ANY,
                                      "Type, source, and mode controls select project fixture context. Power and weight edits update the GDTF file and append a revision entry."),
                     0, wxLEFT | wxRIGHT | wxBOTTOM, gui::gdtf_layout::SectionPadding(this));

  auto *visualPanel = new wxPanel(visualSplitter, wxID_ANY);
  auto *visualSizer = new wxBoxSizer(wxVERTICAL);
  visualPanel->SetSizer(visualSizer);
  visualNotebook = new wxNotebook(visualPanel, wxID_ANY);
  auto *previewPage = new wxPanel(visualNotebook, wxID_ANY);
  auto *previewSizer = new wxBoxSizer(wxVERTICAL);
  previewPage->SetSizer(previewSizer);
  preview = new FixturePreviewPanel(previewPage);
  preview->SetMinSize(wxSize(gui::gdtf_layout::MinimumVisualPaneWidth(this),
                             gui::gdtf_layout::MinimumPreviewHeight(this)));
  previewSizer->Add(preview, 2, wxEXPAND | wxALL, gui::gdtf_layout::SectionPadding(this));
  previewSizer->Add(new wxStaticLine(previewPage), 0, wxEXPAND | wxLEFT | wxRIGHT,
                    gui::gdtf_layout::SectionPadding(this));
  fixtureImagePreview = new wxStaticBitmap(previewPage, wxID_ANY, wxBitmap(220, 220));
  previewSizer->Add(fixtureImagePreview, 1, wxALIGN_CENTER | wxALL,
                    gui::gdtf_layout::SectionPadding(this));
  visualNotebook->AddPage(previewPage, "Preview");

  gdtfWheelInspectorPanel = new GdtfWheelInspectorPanel(visualNotebook);
  visualNotebook->AddPage(gdtfWheelInspectorPanel, "GDTF wheels");

  auto *symbolPage = new wxPanel(visualNotebook, wxID_ANY);
  wxBoxSizer *symbolRootSizer = new wxBoxSizer(wxVERTICAL);
  symbolPage->SetSizer(symbolRootSizer);
  officialSymbolPreview =
      new wxStaticBitmap(symbolPage, wxID_ANY, wxBitmap(220, 220));
  symbolRootSizer->Add(officialSymbolPreview, 1, wxALIGN_CENTER | wxALL,
                       gui::gdtf_layout::SectionPadding(this));
  symbolRootSizer->Add(new wxStaticLine(symbolPage), 0, wxEXPAND | wxLEFT | wxRIGHT,
                       gui::gdtf_layout::SectionPadding(this));
  wxBoxSizer *symbolSizer = new wxBoxSizer(wxHORIZONTAL);
  symbolRootSizer->Add(symbolSizer, 1, wxEXPAND | wxALL,
                       gui::gdtf_layout::SectionPadding(this));
  wxWindow *symbolParent = symbolPage;
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
  visualNotebook->AddPage(symbolPage, "Symbols");

  visualSizer->Add(visualNotebook, 1, wxEXPAND);
  visualPanel->SetMinSize(wxSize(gui::gdtf_layout::MinimumVisualPaneWidth(this), -1));

  visualSplitter->SplitVertically(gdtfPanelHost, visualPanel);
  visualSplitter->SetMinimumPaneSize(gui::gdtf_layout::MinimumVisualPaneWidth(this));
  workspaceSizer->Add(visualSplitter, 1, wxEXPAND);
  contextSplitter->SplitVertically(fixtureScroll, workspacePanel);
  contextSplitter->SetMinimumPaneSize(gui::gdtf_layout::MinimumContextPaneWidth(this));
  contentSizer->Add(contextSplitter, 1, wxEXPAND);
  topSizer->Add(contentPanel, 1, wxEXPAND | wxALL, gui::gdtf_layout::OuterMargin(this));

  wxStdDialogButtonSizer *btns = new wxStdDialogButtonSizer();
  btns->AddButton(new wxButton(this, wxID_APPLY));
  btns->AddButton(new wxButton(this, wxID_OK));
  btns->AddButton(new wxButton(this, wxID_CANCEL));
  btns->Realize();
  topSizer->Add(btns, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, gui::gdtf_layout::ButtonRowMargin(this));

  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnApply, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnOk, this, wxID_OK);
  Bind(wxEVT_BUTTON, &FixtureEditDialog::OnCancel, this, wxID_CANCEL);
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &event) {
    SaveLayoutPreferences();
    event.Skip();
  });

  SetSizer(topSizer);
  RestoreLayoutPreferences();
  SetMinSize(gui::gdtf_layout::ClampDialogSize(this, wxSize(1100, 700), wxSize(1100, 700), wxSize(1, 1)));
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
  const bool accepted = gdtfEditSession->SetValue(fieldId, value);
  if (!accepted) {
    rejectedSessionInputs[fieldId] = "Enter a valid GDTF editor value.";
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
  rejectedSessionInputs.erase(fieldId);
  ClearSessionValidation();
  for (const auto &entry : rejectedSessionInputs) {
    if (entry.first == gdtf::GdtfFieldId::PowerConsumption)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::PowerConsumption, entry.second);
    else if (entry.first == gdtf::GdtfFieldId::Weight)
      gdtfEditorPanel->SetPhysicalPropertyValidation(
          GdtfPhysicalPropertyField::Weight, entry.second);
  }
  SyncSessionDirtyToLegacyFlags();
  return true;
}

// Validates session state before any legacy apply mutation starts.
bool FixtureEditDialog::ValidateSessionBeforeApply() {
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

// Handles selecting a replacement GDTF source and refreshing dependent presentations.
void FixtureEditDialog::OnBrowse(wxCommandEvent &) {
  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fdlg.ShowModal() != wxID_OK)
    return;
  wxString path = fdlg.GetPath();
  pendingSelectedGdtfPath = PathFromWxString(path);
  cachedModeChannelSource.clear();
  cachedModeChannelDocument = {};
  cachedWheelCatalog = {};
  wheelBitmapCache.Clear();
  if (gdtfEditSession && panel && row >= 0 &&
      static_cast<size_t>(row) < panel->rowUuids.size()) {
    const auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    auto it = scene.fixtures.find(panel->rowUuids[static_cast<size_t>(row)]);
    if (it != scene.fixtures.end()) {
      gdtf::ProjectFixtureGdtfContextInput input;
      input.fixture = it->second;
      input.resolvedGdtfPath = pendingSelectedGdtfPath;
      input.editorSourceFileReference = PathUtils::PathToUtf8(input.resolvedGdtfPath);
      if (IsExistingRegularFile(input.resolvedGdtfPath))
        input.document = gdtf::LoadGdtfDocument(input.resolvedGdtfPath);
      input.sourceKind =
          IsUserFixtureLibraryPath(PathUtils::PathToUtf8(input.resolvedGdtfPath))
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
  const wxColour background = ResolvePreviewBackground(panelWindow->GetParent());
  wxBrush backgroundBrush(background);
  dc.SetBackground(backgroundBrush);
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
  gc->SetBrush(backgroundBrush);
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
    gc->SetBrush(backgroundBrush);
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
  const std::string path = PathUtils::PathToUtf8(GetActiveResolvedGdtfPath());
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

  if (officialSymbolPreview) {
    wxBitmap officialSymbol;
    const wxColour background =
        ResolvePreviewBackground(officialSymbolPreview->GetParent());
    if (LoadGdtfOfficialSvgSymbol(path, background, officialSymbol)) {
      officialSymbolPreview->SetBitmap(officialSymbol);
      officialSymbolPreview->SetToolTip("Official GDTF SVG thumbnail resource.");
    } else {
      officialSymbolPreview->SetBitmap(
          CreatePreviewPlaceholder("No official SVG", background));
      officialSymbolPreview->SetToolTip(
          "No official SVG thumbnail resource found in this GDTF.");
    }
  }

  if (fixtureImagePreview) {
    wxBitmap image;
    const wxColour background =
        ResolvePreviewBackground(fixtureImagePreview->GetParent());
    if (LoadGdtfThumbnail(path, background, image)) {
      fixtureImagePreview->SetBitmap(image);
      fixtureImagePreview->SetToolTip("");
    } else {
      fixtureImagePreview->SetBitmap(CreatePreviewPlaceholder("No image", background));
      fixtureImagePreview->SetToolTip("No thumbnail image found in this GDTF.");
    }
    Layout();
  }
}

void FixtureEditDialog::UpdateMetadataSummary() {
  const std::string path = PathUtils::PathToUtf8(GetActiveResolvedGdtfPath());
  GdtfMetadataSummary metadata;
  if (LoadGdtfMetadataSummary(path, metadata)) {
    if (gdtfEditorPanel)
      gdtfEditorPanel->SetMetadata(metadata);
  } else if (gdtfEditorPanel) {
    gdtfEditorPanel->SetMetadataUnavailable();
  }
  Layout();
}

// Updates the cached hierarchical mode browser and channel footprint presentation.
void FixtureEditDialog::UpdateChannels(bool markChannelCountDirty) {
  const std::filesystem::path gdtfPath = GetActiveResolvedGdtfPath();
  wxString mode = gdtfEditorPanel ? wxString::FromUTF8(gdtfEditorPanel->GetSelectedMode()) : wxString();
  if (preview)
    preview->LoadFixture(PathUtils::PathToUtf8(gdtfPath));
  if (gdtfPath.empty() || mode.empty()) {
    if (gdtfEditorPanel) {
      gdtfEditorPanel->ClearModeDetails();
      gdtfEditorPanel->SetInspectionData(nullptr, nullptr);
    }
    if (fixtureChannelSummaryPanel)
      fixtureChannelSummaryPanel->ClearChannels();
    return;
  }
  if (cachedModeChannelSource != gdtfPath)
    ReloadModeChannelDocument();
  const std::string modeName(mode.ToUTF8());
  const auto *modeNode = cachedModeChannelDocument.FindMode(modeName);
  if (gdtfEditorPanel) {
    const auto channelSummary = BuildGdtfModeChannelSummaryPresentation(modeNode);
    gdtfEditorPanel->SetInspectionData(modeNode, &cachedWheelCatalog);
    gdtfEditorPanel->SetModeBrowserNodes(BuildGdtfModeBrowserPresentation(modeNode));
    gdtfEditorPanel->SetChannels(channelSummary);
    if (fixtureChannelSummaryPanel)
      fixtureChannelSummaryPanel->SetChannels(channelSummary);
  }
  const int chCount = modeNode ? modeNode->calculatedFootprint
                               : GetGdtfModeChannelCount(PathUtils::PathToUtf8(gdtfPath), modeName);
  if (gdtfEditorPanel)
    gdtfEditorPanel->SetChannelCount(chCount >= 0 ? std::string(wxString::Format("%d", chCount).ToUTF8()) : std::string());
  (void)markChannelCountDirty;
}

// Enriches wheel inspection rows with lazy media thumbnails and color swatches.
GdtfWheelInspectorPresentation FixtureEditDialog::BuildWheelInspectorVisualPresentation(
    const GdtfWheelInspectorPresentation &presentation) {
  GdtfWheelInspectorPresentation enriched = presentation;
  const std::filesystem::path gdtfPath = GetActiveResolvedGdtfPath();
  const std::string sourceId = gui::BuildGdtfSourceFingerprint(gdtfPath);
  std::vector<std::filesystem::path> resourceRoots;
  const std::string extractedRoot = GetCachedGdtfExtractionDirectory(PathUtils::PathToUtf8(gdtfPath));
  if (!extractedRoot.empty())
    resourceRoots.push_back(PathUtils::PathFromUtf8(extractedRoot));
  auto applyColor = [](GdtfWheelInspectorSlotPresentation &slot) {
    if (slot.rawColor.empty())
      return;
    const auto cie = gdtf::ParseGdtfColorCie(slot.rawColor, gdtf::GdtfValueOrigin::Explicit);
    const auto srgb = gdtf::ConvertCieXyyToSrgb(cie);
    if (!srgb.valid)
      return;
    slot.swatch = wxColour(static_cast<unsigned char>(std::clamp(srgb.red, 0.0, 1.0) * 255.0),
                           static_cast<unsigned char>(std::clamp(srgb.green, 0.0, 1.0) * 255.0),
                           static_cast<unsigned char>(std::clamp(srgb.blue, 0.0, 1.0) * 255.0));
    slot.hasSwatch = true;
  };

  enriched.previewStatus = "No wheel slot preview is available for the current mapping.";
  for (auto &slot : enriched.slots) {
    applyColor(slot);
    std::vector<std::pair<std::string, std::string>> resourceAttempts;
    if (!slot.mediaResource.empty())
      resourceAttempts.push_back({"standard MediaFileName", slot.mediaResource});
    if (!slot.graphicResource.empty() && slot.graphicResource != slot.mediaResource)
      resourceAttempts.push_back({"compatibility graphic resource", slot.graphicResource});
    std::string slotStatus;
    std::string selectedStatus;
    if (!resourceAttempts.empty() && gdtfPath.empty()) {
      slotStatus = "Resource resolution/read failed. Active GDTF source path is unavailable.";
      if (slot.selected)
        selectedStatus = slotStatus;
    }
    for (const auto &[resourceOrigin, resource] : resourceAttempts) {
      if (resource.empty() || gdtfPath.empty())
        continue;
      const auto resourceRead = gdtf::ReadGdtfArchiveResource(
          gdtfPath, resource, 4ull * 1024ull * 1024ull, resourceRoots);
      std::string status = "Raw resource (" + resourceOrigin + "): " + resource;
      bool decodedThumbnail = false;
      if (resourceRead.Success()) {
        const auto thumb = wheelBitmapCache.GetOrCreate(sourceId, resourceRead.entryPath,
                                                        resourceRead.bytes, wxSize(48, 48),
                                                        wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        decodedThumbnail = thumb.decoded;
        status += "\nResolved archive entry: " + resourceRead.entryPath;
        const bool standardCanonical = resourceOrigin == "standard MediaFileName" &&
                                       resourceRead.entryPath == "wheels/" + resource + ".png";
        if (resourceRead.filesystemFallback)
          status += "\nResolution: extracted resource folder fallback";
        else if (standardCanonical)
          status += "\nResolution: standard canonical wheel resource";
        else
          status += resourceRead.caseInsensitiveFallback ? "\nResolution: compatibility fallback"
                                                         : "\nResolution: exact supplied path";
        status += "\nResource bytes: " + std::to_string(resourceRead.size);
        status += "\nDecode result: " + thumb.diagnostic;
        if (thumb.decoded) {
          status += "\nImage dimensions: " + std::to_string(thumb.sourceWidth) + "x" + std::to_string(thumb.sourceHeight);
          slot.thumbnail = thumb.bitmap;
          slot.hasThumbnail = true;
          const auto preview = wheelBitmapCache.GetOrCreate(
              sourceId, resourceRead.entryPath, resourceRead.bytes, wxSize(180, 180),
              wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
          if (preview.decoded) {
            slot.preview = preview.bitmap;
            slot.hasPreview = true;
          } else {
            status += "\nLarge preview decode failed: " + preview.diagnostic;
          }
        }
      } else {
        status += "\nResource resolution/read failed.";
        for (const auto &diagnostic : resourceRead.diagnostics)
          status += "\n" + diagnostic.message;
      }
      slotStatus += slotStatus.empty() ? status : "\n\n" + status;
      if (slot.selected) {
        selectedStatus += selectedStatus.empty() ? status : "\n\n" + status;
        if (resourceRead.Success()) {
          if (slot.hasPreview && !enriched.hasActivePreview) {
            enriched.activePreview = slot.preview;
            enriched.hasActivePreview = true;
          } else if (!slot.hasPreview) {
            selectedStatus += "\nActive preview decode failed or unavailable.";
          }
        }
      }
      if (decodedThumbnail)
        break;
    }
    if (resourceAttempts.empty() && slot.hasSwatch) {
      slotStatus = "Approximate CIE xyY color preview: " + slot.rawColor;
      if (slot.selected)
        selectedStatus = slotStatus;
    }
    if (!slotStatus.empty())
      slot.previewStatus = slotStatus;
    if (slot.selected && !selectedStatus.empty())
      enriched.previewStatus = selectedStatus;
    if (slot.selected && slot.hasSwatch && !enriched.hasActivePreview && resourceAttempts.empty()) {
      enriched.activeSwatch = slot.swatch;
      enriched.hasActiveSwatch = true;
      enriched.previewStatus = "Approximate CIE xyY color preview: " + slot.rawColor;
    }
  }
  return enriched;
}

// Reloads the cached hierarchical GDTF mode/channel document for the active source.
void FixtureEditDialog::ReloadModeChannelDocument() {
  cachedModeChannelSource.clear();
  cachedModeChannelDocument = {};
  cachedWheelCatalog = {};
  wheelBitmapCache.Clear();
  const std::filesystem::path gdtfPath = GetActiveResolvedGdtfPath();
  if (gdtfPath.empty())
    return;
  auto archive = gdtf::ReadGdtfArchive(gdtfPath);
  if (!archive.Success())
    return;
  cachedModeChannelDocument = gdtf::ReadGdtfModeChannelDocument(archive.descriptionXml);
  cachedWheelCatalog = gdtf::ReadGdtfWheelCatalog(archive.descriptionXml);
  cachedModeChannelSource = gdtfPath;
}


// Saves the Fixture Edit visual layout preferences on dialog close paths.
void FixtureEditDialog::SaveLayoutPreferences() {
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  gui::gdtf_layout::FixtureLayoutPreferences preferences;
  preferences.dialogSize = GetSize();
  if (contextSplitter)
    preferences.contextRatio = gui::gdtf_layout::SashToRatio(
        contextSplitter->GetSashPosition(), contextSplitter->GetClientSize().GetWidth(), 0.2);
  if (visualSplitter)
    preferences.visualRatio = gui::gdtf_layout::SashToRatio(
        visualSplitter->GetSashPosition(), visualSplitter->GetClientSize().GetWidth(), 0.75);
  if (gdtfEditorPanel) {
    preferences.gdtfRatio = gdtfEditorPanel->GetTwoPaneSplitterRatio();
    preferences.modeBrowserRatio = gdtfEditorPanel->GetModeBrowserSplitterRatio();
  }
  if (visualNotebook)
    preferences.visualTab = visualNotebook->GetSelection();
  gui::gdtf_layout::SaveFixtureLayoutPreferences(config, preferences);
}

// Restores Fixture Edit size, splitters, and visual tab with display clamping.
void FixtureEditDialog::RestoreLayoutPreferences() {
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto preferences = gui::gdtf_layout::LoadFixtureLayoutPreferences(config, this);
  SetSize(preferences.dialogSize);
  Layout();
  if (contextSplitter)
    contextSplitter->SetSashPosition(gui::gdtf_layout::RatioToSash(
        contextSplitter->GetClientSize().GetWidth(),
        gui::gdtf_layout::MinimumContextPaneWidth(this),
        gui::gdtf_layout::MinimumWorkspacePaneWidth(this), preferences.contextRatio));
  if (visualSplitter)
    visualSplitter->SetSashPosition(gui::gdtf_layout::RatioToSash(
        visualSplitter->GetClientSize().GetWidth(),
        gui::gdtf_layout::MinimumWorkspacePaneWidth(this),
        gui::gdtf_layout::MinimumVisualPaneWidth(this), preferences.visualRatio));
  if (gdtfEditorPanel) {
    gdtfEditorPanel->SetTwoPaneSplitterRatio(preferences.gdtfRatio);
    gdtfEditorPanel->SetModeBrowserSplitterRatio(preferences.modeBrowserRatio);
  }
  if (visualNotebook)
    visualNotebook->SetSelection(preferences.visualTab);
}

void FixtureEditDialog::OnApply(wxCommandEvent &) { ApplyChanges(); }

void FixtureEditDialog::OnOk(wxCommandEvent &) {
  if (!ApplyChanges())
    return;
  SaveLayoutPreferences();
  EndModal(wxID_OK);
}

void FixtureEditDialog::OnCancel(wxCommandEvent &) {
  SaveLayoutPreferences();
  EndModal(wxID_CANCEL);
}

// Applies fixture edits only after GDTF adapter preparation succeeds.
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

  std::unordered_set<std::string> changedWeightPositions;
  gdtf::ProjectFixtureGdtfApplyResult fixtureApplyResult;
  if (gdtfEditSession) {
    auto request = gdtfEditSession->BuildApplyRequest();
    const bool hasAdapterChanges = !request.changedDocumentFields.empty() ||
                                   !request.changedContextFields.empty();
    if (hasAdapterChanges) {
      gdtf::ProjectFixtureGdtfApplyAdapter adapter(
          gui::MakeFixtureGdtfApplyServices());
      const auto &scene =
          GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
      gdtf::ProjectFixtureGdtfApplyInput applyInput;
      applyInput.request = request;
      applyInput.fixtures = &scene.fixtures;
      fixtureApplyResult = adapter.Apply(applyInput);
      if (!fixtureApplyResult.common.success) {
        const std::string message = fixtureApplyResult.common.diagnostics.empty()
                                        ? "Could not apply fixture GDTF changes."
                                        : fixtureApplyResult.common.diagnostics.front();
        wxMessageBox(wxString::FromUTF8(message), "GDTF update",
                     wxOK | wxICON_WARNING, this);
        return false;
      }
      gdtfPath = fixtureApplyResult.common.resultingGdtfPath.empty()
                     ? gdtfPath
                     : fixtureApplyResult.common.resultingGdtfPath;
      pendingSelectedGdtfPath = gdtfPath;
      cachedModeChannelSource.clear();
      cachedModeChannelDocument = {};
      cachedWheelCatalog = {};
      wheelBitmapCache.Clear();
      for (const auto &position : fixtureApplyResult.changedWeightPositionNames)
        changedWeightPositions.insert(position);
      originalPowerW = fixtureApplyResult.resultingPowerConsumptionW;
      originalWeightKg = fixtureApplyResult.resultingWeightKg;
    }
  }

  for (size_t i = 0; i < ctrls.size(); ++i) {
    if (i >= modifiedColumns.size() || !modifiedColumns[i])
      continue;
    if (i == 7 && gdtfEditorPanel) {
      table->SetValue(wxVariant(wxString::FromUTF8(gdtfEditorPanel->GetSelectedMode())), row, i);
    } else if (i == 8 && gdtfEditorPanel) {
      int chCount = gdtfPath.empty()
                        ? -1
                        : GetGdtfModeChannelCount(
                              PathUtils::PathToUtf8(gdtfPath),
                              gdtfEditorPanel->GetSelectedMode());
      table->SetValue(wxVariant(chCount >= 0 ? wxString::Format("%d", chCount)
                                             : wxString()),
                      row, i);
    } else if (i == 9 && gdtfEditorPanel) {
      wxFileName fn(wxString::FromUTF8(PathUtils::PathToUtf8(gdtfPath)));
      table->SetValue(wxVariant(fn.GetFullName()), row, i);
      if ((size_t)row >= panel->gdtfPaths.size())
        panel->gdtfPaths.resize(row + 1);
      panel->gdtfPaths[row] = wxString::FromUTF8(PathUtils::PathToUtf8(gdtfPath));
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
  if (!fixtureApplyResult.updatedFixtures.empty()) {
    auto &fixtures =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().fixtures;
    for (const auto &[uuid, fixture] : fixtureApplyResult.updatedFixtures)
      fixtures[uuid] = fixture;
  }
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
