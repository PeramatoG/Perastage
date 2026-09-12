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
#include "mvrimporter.h"
#include "../gui/gdtf_resolution_status_style.h"
#include "apppaths.h"
#include "build_info.h"
#include "configmanager.h"
#include "filesystem_path_utils.h"
#include "mvr_import_package.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_import_resource_resolver.h"
#include "mvr_scene_node_reader.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "credentialstore.h"
#endif
#include "dummyprofilelibrary.h"
#include "gdtfdictionary.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "gdtfnet.h"
#endif
#include "fixture_label_overrides.h"
#include "fixture_visual_color.h"
#include "gdtf_catalog_matcher.h"
#include "gdtf_catalog_parser.h"
#include "gdtf_catalog_service.h"
#include "gdtf_fixture_category.h"
#include "gdtf_import_matching.h"
#include "gdtfloader.h"
#include "geometry_bounds_resolver.h"
#include "groupobject.h"
#include "layer_service.h"
#include "matrixutils.h"
#include "primitive_model_resources.h"
#include "projectutils.h"
#include "runtime_storage.h"
#include "scene_grouping.h"
#include "sceneobject.h"
#include "support.h"
#include "truss_dimension_resolution.h"
#include "trussloader.h"
#include "utf8_utils.h"
#include "uuidutils.h"

#include "consolepanel.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "logindialog.h"
#endif
#include "json.hpp"
#include "logger.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream> // Required for std::ofstream
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// TinyXML2
#include <tinyxml2.h>

// wxWidgets zip support
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/listctrl.h>
#include <wx/mstream.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/wx.h>

namespace fs = std::filesystem;
namespace gdtf_catalog_matcher = mvr::gdtf_catalog_matcher;

// Helper to convert between std::u8string and std::string without
// losing the underlying UTF-8 byte sequence.
static std::string ToString(const std::u8string &s) {
  return std::string(s.begin(), s.end());
}

static std::string Trim(const std::string &s) {
  const char *ws = " \t\r\n";
  size_t start = s.find_first_not_of(ws);
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(ws);
  return s.substr(start, end - start + 1);
}

// Builds a stable per-parent key for SceneObject child geometry instances.
static std::string BuildSceneObjectGeometryInstanceKey(
    const std::string &parentUuid, const std::string &childKind,
    size_t childIndex, const std::string &symdefUuid = {}) {
  std::ostringstream key;
  key << parentUuid << '/' << childKind << '/' << childIndex;
  if (!symdefUuid.empty())
    key << '/' << symdefUuid;
  return key.str();
}

static std::string ToLowerCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

static std::string NormalizeSlashes(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

static bool IsNearlyEqualRelative(float a, float b, float relEps) {
  if (!std::isfinite(a) || !std::isfinite(b))
    return false;
  const float scale = std::max(std::max(std::fabs(a), std::fabs(b)), 1.0e-6f);
  return std::fabs(a - b) <= relEps * scale;
}

static bool IsGeometryMatrixContext(const std::string &contextTag) {
  if (contextTag == "SceneObject/Geometry3D" ||
      contextTag == "Truss/Geometry3D" || contextTag == "Symbol") {
    return true;
  }
  return contextTag.find("Geometry3D") != std::string::npos;
}

static std::string
JoinMatrixContextCounts(const std::unordered_map<std::string, size_t> &counts) {
  if (counts.empty())
    return "none";

  std::vector<std::pair<std::string, size_t>> sorted(counts.begin(),
                                                     counts.end());
  std::sort(sorted.begin(), sorted.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.second != rhs.second)
      return lhs.second > rhs.second;
    return lhs.first < rhs.first;
  });

  std::ostringstream oss;
  for (size_t i = 0; i < sorted.size(); ++i) {
    if (i > 0)
      oss << ", ";
    oss << sorted[i].first << "=" << sorted[i].second;
  }
  return oss.str();
}

static std::string GenerateShortToken(size_t length = 10) {
  static constexpr char kAlphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937_64 rng(rd());
  std::uniform_int_distribution<size_t> dist(0, sizeof(kAlphabet) - 2);

  std::string token;
  token.reserve(length);
  for (size_t i = 0; i < length; ++i)
    token.push_back(kAlphabet[dist(rng)]);
  return token;
}

// Parses a trimmed float token and accepts only full-token numeric input.
static bool TryParseFloat(const std::string &text, float &out) {
  if (text.empty())
    return false;

  const auto first =
      std::find_if_not(text.begin(), text.end(),
                       [](unsigned char c) { return std::isspace(c); });
  if (first == text.end())
    return false;
  const auto last =
      std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

  errno = 0;
  std::string trimmedText(trimmed);
  char *endPtr = nullptr;
  const double parsed = std::strtod(trimmedText.c_str(), &endPtr);
  if (endPtr == trimmedText.c_str() + trimmedText.size() && errno != ERANGE &&
      std::isfinite(parsed) && std::isfinite(static_cast<float>(parsed))) {
    out = static_cast<float>(parsed);
    return true;
  }
  return false;
}



static std::string CieToHex(const std::string &cie) {
  std::string t = cie;
  std::replace(t.begin(), t.end(), ',', ' ');
  std::stringstream ss(t);
  double x = 0.0, y = 0.0, Yv = 0.0;
  if (!(ss >> x >> y >> Yv) || y <= 0.0)
    return {};
  double X = x * (Yv / y);
  double Z = (1.0 - x - y) * (Yv / y);
  double r = 3.2406 * X - 1.5372 * Yv - 0.4986 * Z;
  double g = -0.9689 * X + 1.8758 * Yv + 0.0415 * Z;
  double b = 0.0557 * X - 0.2040 * Yv + 1.0570 * Z;
  auto gamma = [](double c) {
    c = std::max(0.0, c);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
  };
  r = gamma(r);
  g = gamma(g);
  b = gamma(b);
  r = std::clamp(r, 0.0, 1.0);
  g = std::clamp(g, 0.0, 1.0);
  b = std::clamp(b, 0.0, 1.0);
  int R = static_cast<int>(std::round(r * 255.0));
  int G = static_cast<int>(std::round(g * 255.0));
  int B = static_cast<int>(std::round(b * 255.0));
  std::ostringstream os;
  os << '#' << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
     << R << std::setw(2) << G << std::setw(2) << B;
  return os.str();
}

// Helper to log errors both to stderr and the application's console panel.
// Log a message to both the log file and the application's console panel.
// Console updates are queued to the GUI thread to avoid blocking.
// Describes the import source kind for diagnostic logging.
static const char *DescribeMvrImportSourceKind(MvrImportSourceKind sourceKind) {
  switch (sourceKind) {
  case MvrImportSourceKind::ExternalImport:
    return "ExternalImport";
  case MvrImportSourceKind::ProjectRestore:
    return "ProjectRestore";
  case MvrImportSourceKind::MergeImport:
    return "MergeImport";
  }
  return "Unknown";
}

static bool IsDetailedMvrImportLogEnabled() {
  return ConfigManager::Get().GetFloat("mvr_import_detailed_log") >= 0.5f;
}

static void LogMessage(Logger::Level level, const std::string &msg) {
  if (level == Logger::Level::Debug && !IsDetailedMvrImportLogEnabled())
    return;

  Logger::Instance().Log(level, msg);
  if (ConsolePanel::Instance() && wxTheApp) {
    constexpr size_t kMaxConsoleMessageLength = 8 * 1024;
    const std::string suffix = "... (truncated)";
    std::string panelMsg = msg;
    if (panelMsg.size() > kMaxConsoleMessageLength) {
      size_t keepLength = kMaxConsoleMessageLength > suffix.size()
              ? kMaxConsoleMessageLength - suffix.size()
              : 0;
      panelMsg = panelMsg.substr(0, keepLength) + suffix;
    }
    wxString wmsg = wxString::FromUTF8(panelMsg.c_str());
    wxTheApp->CallAfter([wmsg]() {
      if (ConsolePanel::Instance())
        ConsolePanel::Instance()->AppendMessage(wmsg);
    });
  }
}

static void LogMessage(const std::string &msg) {
  LogMessage(Logger::Level::Info, msg);
}

using GdtfConflict = mvr::SceneReadGdtfConflict;

enum class GdtfConflictChoice { Mvr, App, Download };

struct GdtfConflictSelection {
  GdtfConflictChoice choice = GdtfConflictChoice::App;
};

static std::unordered_map<std::string, GdtfConflictSelection>
PromptGdtfConflicts(const std::vector<GdtfConflict> &conflicts) {
  std::unordered_map<std::string, GdtfConflictSelection> chosen;
  if (conflicts.empty())
    return chosen;

  auto selectAll = [](const std::vector<wxRadioButton *> &buttons) {
    for (wxRadioButton *button : buttons) {
      if (button)
        button->SetValue(true);
    }
  };

  wxDialog dlg(nullptr, wxID_ANY, _("Resolve GDTF source conflicts"));
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText *subtitle = new wxStaticText(
      &dlg, wxID_ANY, _("Choose which source to keep for each fixture type."));
  subtitle->SetForegroundColour(wxColour(145, 145, 145));
  topSizer->Add(subtitle, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  wxFlexGridSizer *grid = new wxFlexGridSizer(4, 8, 10);
  grid->AddGrowableCol(0, 1);

  wxStaticText *typeHeader = new wxStaticText(&dlg, wxID_ANY, _("Type"));
  wxStaticText *mvrHeader = new wxStaticText(&dlg, wxID_ANY, _("MVR"));
  wxStaticText *appHeader = new wxStaticText(&dlg, wxID_ANY, _("App"));
  wxStaticText *downloadHeader =
      new wxStaticText(&dlg, wxID_ANY, _("Download GDTF"));
  wxFont headerFont = typeHeader->GetFont();
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);
  typeHeader->SetFont(headerFont);
  mvrHeader->SetFont(headerFont);
  appHeader->SetFont(headerFont);
  downloadHeader->SetFont(headerFont);
  grid->Add(typeHeader, 0, wxALIGN_CENTER_VERTICAL);
  grid->Add(mvrHeader, 0, wxALIGN_CENTER_HORIZONTAL);
  grid->Add(appHeader, 0, wxALIGN_CENTER_HORIZONTAL);
  grid->Add(downloadHeader, 0, wxALIGN_CENTER_HORIZONTAL);

  wxButton *selectAllMvrButton = new wxButton(&dlg, wxID_ANY, _("Select all"));
  wxButton *selectAllAppButton = new wxButton(&dlg, wxID_ANY, _("Select all"));
  wxButton *selectAllDownloadButton =
      new wxButton(&dlg, wxID_ANY, _("Select all"));
  grid->Add(new wxStaticText(&dlg, wxID_ANY, wxEmptyString));
  grid->Add(selectAllMvrButton, 0, wxALIGN_CENTER_HORIZONTAL);
  grid->Add(selectAllAppButton, 0, wxALIGN_CENTER_HORIZONTAL);
  grid->Add(selectAllDownloadButton, 0, wxALIGN_CENTER_HORIZONTAL);

  std::vector<wxRadioButton *> mvrBtns;
  std::vector<wxRadioButton *> appBtns;
  std::vector<wxRadioButton *> downloadBtns;
  for (const auto &c : conflicts) {
    grid->Add(
        new wxStaticText(&dlg, wxID_ANY, wxString::FromUTF8(c.type.c_str())));
    wxRadioButton *mvr = new wxRadioButton(
        &dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    wxRadioButton *app = nullptr;
    wxRadioButton *download = new wxRadioButton(&dlg, wxID_ANY, "");
    grid->Add(mvr, 0, wxALIGN_CENTER);
    if (c.hasDictionaryEntry) {
      app = new wxRadioButton(&dlg, wxID_ANY, "");
      app->SetValue(true);
      grid->Add(app, 0, wxALIGN_CENTER);
    } else {
      grid->Add(new wxStaticText(&dlg, wxID_ANY, "-"), 0, wxALIGN_CENTER);
    }
    grid->Add(download, 0, wxALIGN_CENTER);
    mvrBtns.push_back(mvr);
    appBtns.push_back(app);
    downloadBtns.push_back(download);
  }

  selectAllAppButton->Bind(wxEVT_BUTTON,
                           [&appBtns, &selectAll](wxCommandEvent &) {
                             std::vector<wxRadioButton *> existing;
                             for (wxRadioButton *btn : appBtns) {
                               if (btn)
                                 existing.push_back(btn);
                             }
                             selectAll(existing);
                           });
  selectAllMvrButton->Bind(
      wxEVT_BUTTON,
      [&mvrBtns, &selectAll](wxCommandEvent &) { selectAll(mvrBtns); });
  selectAllDownloadButton->Bind(wxEVT_BUTTON,
                                [&downloadBtns, &selectAll](wxCommandEvent &) {
                                  selectAll(downloadBtns);
                                });
  topSizer->Add(grid, 1, wxEXPAND | wxALL, 10);
  topSizer->Add(dlg.CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxEXPAND | wxALL, 10);
  dlg.SetSizerAndFit(topSizer);
  dlg.CentreOnScreen();

  if (dlg.ShowModal() != wxID_OK)
    return chosen;

  for (size_t i = 0; i < conflicts.size(); ++i) {
    const auto &c = conflicts[i];
    GdtfConflictSelection selection;
    if (mvrBtns[i]->GetValue()) {
      selection.choice = GdtfConflictChoice::Mvr;
    } else if (downloadBtns[i]->GetValue()) {
      selection.choice = GdtfConflictChoice::Download;
    } else if (appBtns[i] && appBtns[i]->GetValue()) {
      selection.choice = GdtfConflictChoice::App;
    } else {
      selection.choice = GdtfConflictChoice::Mvr;
    }
    chosen[c.type] = selection;
  }
  return chosen;
}

// Returns the preferred fallback GDTF path for download conflicts, prioritizing
// app fixtures over MVR fixtures.
static std::string GetDownloadFallbackPath(const GdtfConflict &conflict) {
  return !conflict.appPath.empty() ? conflict.appPath : conflict.mvrPath;
}

// Logs each structured MVR import diagnostic exactly once for discarded
// results.
static void
LogMvrImportDiagnostics(const std::vector<MvrImportDiagnostic> &diagnostics) {
  for (const MvrImportDiagnostic &diagnostic : diagnostics) {
    LogMessage(Logger::Level::Warn, "MVR metadata diagnostic [" +
                                        diagnostic.code +
                                        "]: " + diagnostic.message);
  }
}

// Imports an MVR file into the global application scene.
bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 bool promptConflicts, bool applyDictionary,
                                 ProgressCallback progressCallback) {
  MvrImportResult importResult;
  const bool imported =
      ImportFromFile(filePath, importResult, MvrImportMode::ReplaceProject,
                     promptConflicts, applyDictionary, progressCallback);
  LogMvrImportDiagnostics(importResult.diagnostics);
  return imported;
}

// Imports an MVR file into an import result and optionally replaces the global
// project.
bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 MvrImportResult &importResult,
                                 MvrImportMode mode, bool promptConflicts,
                                 bool applyDictionary,
                                 ProgressCallback progressCallback) {
  MvrImportOptions options;
  options.promptConflicts = promptConflicts;
  options.applyDictionary = applyDictionary;
  return ImportFromFile(filePath, importResult, mode, options,
                        progressCallback);
}

// Imports an MVR file into an import result using explicit import behavior
// options.
bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 MvrImportResult &importResult,
                                 MvrImportMode mode,
                                 const MvrImportOptions &options,
                                 ProgressCallback progressCallback) {
  return ImportFromFileIntoResult(filePath, importResult, mode, options,
                                  progressCallback);
}

// Imports an MVR file into the provided scene without resetting global
// configuration.
bool MvrImporter::ImportSceneFromFile(const std::string &filePath,
                                      MvrScene &targetScene,
                                      bool promptConflicts,
                                      bool applyDictionary,
                                      ProgressCallback progressCallback) {
  MvrImportOptions options;
  options.promptConflicts = promptConflicts;
  options.applyDictionary = applyDictionary;
  return ImportSceneFromFile(filePath, targetScene, options, progressCallback);
}

// Imports an MVR file into the provided scene using explicit import behavior
// options.
bool MvrImporter::ImportSceneFromFile(const std::string &filePath,
                                      MvrScene &targetScene,
                                      const MvrImportOptions &options,
                                      ProgressCallback progressCallback) {
  MvrImportResult importResult;
  const bool imported =
      ImportFromFile(filePath, importResult, MvrImportMode::ParseOnly, options,
                     progressCallback);
  LogMvrImportDiagnostics(importResult.diagnostics);
  if (!imported)
    return false;

  targetScene = std::move(importResult.scene);
  fixtureUuidRemap = std::move(importResult.fixtureUuidRemap);
  return true;
}

// Extracts an MVR package and parses its scene data into an import result
// payload.
bool MvrImporter::ImportFromFileIntoResult(const std::string &filePath,
                                           MvrImportResult &importResult,
                                           MvrImportMode mode,
                                           const MvrImportOptions &options,
                                           ProgressCallback progressCallback) {
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (!progressCallback)
      return;
    progressCallback(ProgressState{std::move(stage), completed, total});
  };

  pathRemap.clear();
  fixtureUuidRemap.clear();
  importResult = MvrImportResult{};
  // Treat the incoming path as UTF-8 to preserve any non-ASCII characters
  fs::path path = PathUtils::PathFromUtf8(filePath);

  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  reportProgress("Preparing import...");

  // macOS Finder can treat .mvr ZIP archives as packages, and in that case
  // std::filesystem::exists() may report false for a valid double-click path.
  // Keep the strict filesystem check on Windows, but on macOS fall back to
  // wxFileName::FileExists() before aborting so Finder-opened MVR imports work.
  std::error_code importExistsEc;
#if defined(__WXMSW__)
  if (!fs::exists(path, importExistsEc) || importExistsEc) {
    LogMessage("MVR file does not exist: " + filePath);
    return false;
  }
#elif defined(__WXOSX__) || defined(__APPLE__)
  if (!fs::exists(path, importExistsEc) || importExistsEc) {
    wxFileName finderPath(wxString::FromUTF8(filePath));
    wxFileInputStream stream(finderPath.GetFullPath());
    if (!finderPath.FileExists() && !stream.IsOk()) {
      LogMessage("MVR file does not exist: " + filePath);
      return false;
    }
  }
#else
  if (!fs::exists(path, importExistsEc) || importExistsEc) {
    LogMessage("MVR file does not exist: " + filePath);
    return false;
  }
#endif
  if (ext != ".mvr") {
    LogMessage("MVR file has invalid extension: " + ext);
    return false;
  }

  wxFileInputStream input(wxString::FromUTF8(filePath.c_str()));
  if (!input.IsOk()) {
    LogMessage("Failed to open MVR file.");
    return false;
  }
  return ImportFromStreamIntoResult(input, importResult, mode, options,
                                    progressCallback);
}

// Imports an MVR byte payload without a project-level temporary archive.
bool MvrImporter::ImportFromBuffer(
    const std::vector<std::uint8_t> &bytes, MvrImportResult &importResult,
    MvrImportMode mode, const MvrImportOptions &options,
    ProgressCallback progressCallback) {
  if (bytes.empty())
    return false;
  wxMemoryInputStream input(bytes.data(), bytes.size());
  return ImportFromStreamIntoResult(input, importResult, mode, options,
                                    progressCallback);
}

// Extracts and parses an MVR archive from an already-open stream.
bool MvrImporter::ImportFromStreamIntoResult(
    wxInputStream &input, MvrImportResult &importResult, MvrImportMode mode,
    const MvrImportOptions &options, ProgressCallback progressCallback) {
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (progressCallback)
      progressCallback(ProgressState{std::move(stage), completed, total});
  };
  pathRemap.clear();
  fixtureUuidRemap.clear();
  importResult = MvrImportResult{};
  reportProgress("Extracting package resources...");

  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(input, importResult.diagnostics);
  if (!package)
    return false;

  pathRemap = package->pathRemap;
  const std::string scenePath = ToString(package->sceneXmlPath.u8string());
  reportProgress("Parsing scene data...");
  const bool parsed =
      ParseSceneXml(scenePath, importResult, options, progressCallback);
  if (!parsed)
    return false;

  importResult.scene.runtimeResourceLeases.push_back(
      package->workspace.TransferToSceneLease());

  if (mode == MvrImportMode::ReplaceProject) {
    ConfigManager::Get().Reset();
    ConfigManager::Get().GetScene() = importResult.scene;
  }

  fixtureUuidRemap = importResult.fixtureUuidRemap;
  return true;
}

// Normalizes an MVR archive path so extracted resources can be found reliably.
std::string
MvrImporter::NormalizeArchivePath(const std::string &archivePath) const {
  return mvr::NormalizeImportArchivePath(archivePath);
}

// Returns a remapped extraction path for long archive entries when one exists.
std::string
MvrImporter::RemapArchivePathIfNeeded(const std::string &archivePath) const {
  const std::string normalized = NormalizeArchivePath(archivePath);
  auto it = pathRemap.find(normalized);
  if (it != pathRemap.end())
    return it->second;
  return archivePath;
}

// Parses GeneralSceneDescription.xml and populates the import result scene
// payload.
bool MvrImporter::ParseSceneXml(const std::string &sceneXmlPath,
                                MvrImportResult &importResult,
                                const MvrImportOptions &options,
                                ProgressCallback progressCallback) {
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (!progressCallback)
      return;
    progressCallback(ProgressState{std::move(stage), completed, total});
  };

  tinyxml2::XMLDocument doc;
  tinyxml2::XMLError result = doc.LoadFile(sceneXmlPath.c_str());
  if (result != tinyxml2::XML_SUCCESS) {
    LogMessage("Failed to load XML: " + sceneXmlPath);
    return false;
  }

  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  if (!root) {
    LogMessage("Missing GeneralSceneDescription node");
    return false;
  }

  MvrScene &scene = importResult.scene;
  scene.Clear();
  mvr::MvrImportReferenceResolver referenceResolver(
      [](const std::string &message) {
        LogMessage(Logger::Level::Warn, message);
      });
  scene.basePath =
      ToString(PathUtils::PathFromUtf8(sceneXmlPath).parent_path().u8string());
  LogMessage(
      Logger::Level::Info,
             std::string("MVR import mode: source=") +
                 DescribeMvrImportSourceKind(options.sourceKind) +
          ", promptConflicts=" + (options.promptConflicts ? "true" : "false") +
          ", applyDictionary=" + (options.applyDictionary ? "true" : "false") +
                 ", basePath='" + scene.basePath + "'.");

  root->QueryIntAttribute("verMajor", &scene.versionMajor);
  root->QueryIntAttribute("verMinor", &scene.versionMinor);

  // Warn if the MVR file uses a newer version than we officially support.
  // The importer still attempts to parse the file so that documents with a
  // higher minor version (e.g. 1.5) remain usable.
  constexpr int SUPPORTED_MAJOR = 1;
  constexpr int SUPPORTED_MINOR = 6;
  if (scene.versionMajor != SUPPORTED_MAJOR ||
      scene.versionMinor > SUPPORTED_MINOR) {
    LogMessage("Warning: unsupported MVR version " +
               std::to_string(scene.versionMajor) + "." +
               std::to_string(scene.versionMinor) +
               ". Results may be incomplete.");
  }

  const char *provider = root->Attribute("provider");
  const char *version = root->Attribute("providerVersion");

  if (provider)
    scene.provider = provider;
  if (version)
    scene.providerVersion = version;

  int rootUserDataCount = 0;
  for (tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
       userData; userData = userData->NextSiblingElement("UserData"))
    ++rootUserDataCount;
  if (rootUserDataCount > 1) {
    importResult.diagnostics.push_back(
        {"multiple_root_userdata",
         "Ignored additional root UserData elements beyond the first."});
  }

  // Preserves validated foreign provider blocks without interpreting their
  // schema.
  if (tinyxml2::XMLElement *userData = root->FirstChildElement("UserData")) {
    for (tinyxml2::XMLElement *data = userData->FirstChildElement(); data;
         data = data->NextSiblingElement()) {
      if (std::string(data->Name()) != "Data") {
        importResult.diagnostics.push_back(
            {"invalid_root_userdata_child",
             "Ignored a root UserData child that is not a Data element."});
        continue;
      }
      const std::string dataProvider =
          Trim(data->Attribute("provider") ? data->Attribute("provider") : "");
      if (dataProvider.empty()) {
        importResult.diagnostics.push_back(
            {"missing_userdata_provider",
             "Ignored a root UserData Data block without a provider."});
        continue;
      }
      if (ToLowerCopy(dataProvider) == "perastage")
        continue;
      tinyxml2::XMLPrinter printer(nullptr, true);
      data->Accept(&printer);
      MvrOpaqueUserDataBlock block{
          dataProvider,
          Trim(data->Attribute("ver") ? data->Attribute("ver") : ""),
          printer.CStr()};
      if (std::find(scene.opaqueUserDataBlocks.begin(),
                    scene.opaqueUserDataBlocks.end(),
                    block) == scene.opaqueUserDataBlocks.end())
        scene.opaqueUserDataBlocks.push_back(std::move(block));
    }
  }

  tinyxml2::XMLElement *sceneNode = root->FirstChildElement("Scene");
  if (!sceneNode) {
    LogMessage("No Scene node found in GeneralSceneDescription");
    return true;
  }

  auto textOf = [](tinyxml2::XMLElement *parent,
                   const char *name) -> std::string {
    tinyxml2::XMLElement *n = parent->FirstChildElement(name);
    if (n && n->GetText())
      return Trim(n->GetText());
    return {};
  };

  auto intOf = [](tinyxml2::XMLElement *parent, const char *name, int &out) {
    tinyxml2::XMLElement *n = parent->FirstChildElement(name);
    if (n && n->GetText())
      out = std::atoi(n->GetText());
  };

  auto fixtureIdOf = [&](tinyxml2::XMLElement *parent, std::string &textOut,
                         int &numericOut) {
    textOut = textOf(parent, "FixtureID");
    intOf(parent, "FixtureIDNumeric", numericOut);
    if (numericOut <= 0 && !textOut.empty())
      numericOut = std::atoi(textOut.c_str());
  };

  std::unordered_map<std::string, std::string> perastageTypeToGdtfPath;
  std::unordered_map<std::string, std::string> perastageInstanceToTypeKey;
  auto parsePerastageManifest = [&](tinyxml2::XMLElement *userDataNode,
                                    const char *originLabel) {
    if (!userDataNode)
      return false;
    bool found = false;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (tinyxml2::XMLElement *manifest =
              data->FirstChildElement("TrussSidecarManifest")) {
        found = true;
        for (tinyxml2::XMLElement *type = manifest->FirstChildElement("Type");
             type; type = type->NextSiblingElement("Type")) {
          const char *key = type->Attribute("key");
          const char *path = type->Attribute("gdtf");
          if (key && path)
            perastageTypeToGdtfPath[Trim(key)] = RemapArchivePathIfNeeded(path);
        }
        for (tinyxml2::XMLElement *inst =
                 manifest->FirstChildElement("Instance");
             inst; inst = inst->NextSiblingElement("Instance")) {
          const char *uuid = inst->Attribute("uuid");
          const char *key = inst->Attribute("typeKey");
          if (uuid && key)
            perastageInstanceToTypeKey[CanonicalizeUuid(Trim(uuid))] =
                Trim(key);
        }
      }
    }
    if (found) {
      LogMessage(
          Logger::Level::Info,
                 std::string("MVR import loaded Perastage sidecar manifest from ") +
                     originLabel);
    }
    return found;
  };

  const bool hasRootManifest = parsePerastageManifest(
      root->FirstChildElement("UserData"), "GeneralSceneDescription/UserData");
  if (!hasRootManifest &&
      parsePerastageManifest(sceneNode->FirstChildElement("UserData"),
                             "legacy Scene/UserData")) {
    LogMessage(Logger::Level::Warn, "MVR import used legacy Scene/UserData "
                                    "fallback for Perastage sidecar manifest");
  }

  std::unordered_map<std::string, std::string> layerColorByUuid;
  std::unordered_map<std::string, std::string> layerColorByName;
  auto isHexRgb = [](const std::string &color) {
    if (color.size() != 7 || color[0] != '#')
      return false;
    return std::all_of(color.begin() + 1, color.end(),
                       [](unsigned char ch) { return std::isxdigit(ch) != 0; });
  };
  auto parseLayerAppearanceMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("LayerAppearanceMap");
           map; map = map->NextSiblingElement("LayerAppearanceMap")) {
        auto parseAppearanceEntry = [&](tinyxml2::XMLElement *entry) {
          const std::string color =
              Trim(entry->Attribute("color") ? entry->Attribute("color") : "");
          if (!isHexRgb(color))
            return;
          const std::string uuid = CanonicalizeUuid(
              Trim(entry->Attribute("uuid") ? entry->Attribute("uuid") : ""));
          const std::string name =
              Trim(entry->Attribute("name") ? entry->Attribute("name") : "");
          if (!uuid.empty())
            layerColorByUuid[uuid] = color;
          if (!name.empty())
            layerColorByName[name] = color;
        };

        for (tinyxml2::XMLElement *entry =
                 map->FirstChildElement("PerastageLayerAppearance");
             entry;
             entry = entry->NextSiblingElement("PerastageLayerAppearance"))
          parseAppearanceEntry(entry);
        for (tinyxml2::XMLElement *entry = map->FirstChildElement("Layer");
             entry; entry = entry->NextSiblingElement("Layer"))
          parseAppearanceEntry(entry);
      }
    }
  };
  parseLayerAppearanceMap(root->FirstChildElement("UserData"));

  using RootFixtureTypeInfo = mvr::SceneReadFixtureTypeInfo;
  std::unordered_map<std::string, RootFixtureTypeInfo> rootFixtureTypeInfoByKey;

  // Builds the root UserData fixture type key used by Perastage exports.
  auto buildFixtureTypeInfoKey = [](const std::string &gdtfSpec,
                                        const std::string &gdtfMode,
                                        const std::string &typeName) {
    std::ostringstream key;
    key << Trim(gdtfSpec) << '|' << Trim(gdtfMode);
    if (Trim(gdtfSpec).empty())
      key << '|' << Trim(typeName);
    std::string value = key.str();
    for (char &ch : value) {
      const unsigned char uch = static_cast<unsigned char>(ch);
      if (uch < 32 || ch == '/' || ch == '\\')
        ch = '_';
    }
    return Trim(value);
  };

  // Collects root-level Perastage fixture type category metadata.
  auto parseRootFixtureTypeInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("FixtureTypeInfoMap");
           map; map = map->NextSiblingElement("FixtureTypeInfoMap")) {
        for (tinyxml2::XMLElement *info =
                 map->FirstChildElement("FixtureTypeInfo");
             info; info = info->NextSiblingElement("FixtureTypeInfo")) {
          std::string key =
              Trim(info->Attribute("key") ? info->Attribute("key") : "");
          if (key.empty()) {
            key = buildFixtureTypeInfoKey(
                info->Attribute("gdtfSpec") ? info->Attribute("gdtfSpec") : "",
                info->Attribute("gdtfMode") ? info->Attribute("gdtfMode") : "",
                info->Attribute("model") ? info->Attribute("model") : "");
          }
          if (key.empty())
            continue;
          RootFixtureTypeInfo typeInfo;
          if (tinyxml2::XMLElement *category =
                  info->FirstChildElement("Category")) {
            if (const char *txt = category->GetText())
              typeInfo.category =
                  GdtfFixtureCategory::NormalizeCategory(Trim(txt));
          }
          if (tinyxml2::XMLElement *source =
                  info->FirstChildElement("CategorySource")) {
            if (const char *txt = source->GetText())
              typeInfo.categorySource = Trim(txt);
          }
          if (!typeInfo.category.empty() && typeInfo.categorySource.empty())
            typeInfo.categorySource = GdtfFixtureCategory::kManualSource;
          if (tinyxml2::XMLElement *visualColor =
                  info->FirstChildElement("VisualColor")) {
            if (const char *txt = visualColor->GetText()) {
              const std::string value = Trim(txt);
              if (isHexRgb(value))
                typeInfo.visualColorHex = value;
            }
          }
          if (!typeInfo.category.empty() || !typeInfo.visualColorHex.empty())
            rootFixtureTypeInfoByKey[key] = typeInfo;
        }
      }
    }
  };
  parseRootFixtureTypeInfoMap(root->FirstChildElement("UserData"));

  std::unordered_set<tinyxml2::XMLElement *> diagnosedUnsupportedData;
  auto isSupportedPerastageMetadata = [&](tinyxml2::XMLElement *data) {
    const std::string version =
        Trim(data->Attribute("ver") ? data->Attribute("ver") : "");
    if (version == "1.0")
      return true;
    if (diagnosedUnsupportedData.insert(data).second) {
      importResult.diagnostics.push_back(
          {"unsupported_perastage_metadata_version",
           "Ignored Perastage root metadata with unsupported schema version '" +
               version + "'."});
    }
    return false;
  };

  std::unordered_map<std::string, std::string> projectFixtureColorsByUuid;
  using ProjectFixtureIdentifiers = mvr::SceneReadFixtureIdentifiers;
  std::unordered_map<std::string, ProjectFixtureIdentifiers>
      projectFixtureIdentifiersByUuid;
  std::unordered_set<std::string> projectFixtureColorMetadataUuids;
  std::unordered_set<std::string> consumedProjectFixtureColorUuids;
  // Collects canonical Perastage fixture fidelity metadata for every import
  // mode.
  auto parseProjectFixtureMetadata = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage" || !isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("ProjectFixtureMetadataMap");
           map; map = map->NextSiblingElement("ProjectFixtureMetadataMap")) {
        const std::string schemaVersion = Trim(
            map->Attribute("schemaVersion") ? map->Attribute("schemaVersion")
                                            : "");
        if (schemaVersion != "1.0") {
          importResult.diagnostics.push_back(
              {"unsupported_project_fixture_metadata_version",
               "Ignored project fixture metadata with unsupported schema "
               "version '" +
                   schemaVersion + "'."});
          continue;
        }
        for (tinyxml2::XMLElement *entry =
                 map->FirstChildElement("ProjectFixtureMetadata");
             entry;
             entry = entry->NextSiblingElement("ProjectFixtureMetadata")) {
          const std::string rawUuid =
              Trim(entry->Attribute("uuid") ? entry->Attribute("uuid") : "");
          const std::string uuid = CanonicalizeUuid(rawUuid);
          if (uuid.empty()) {
            importResult.diagnostics.push_back(
                {"malformed_project_fixture_metadata_uuid",
                 "Ignored project fixture metadata with malformed UUID '" +
                     rawUuid + "'."});
            continue;
          }
          ProjectFixtureIdentifiers identifiers;
          const bool hasIdentifiers =
              entry->QueryIntAttribute("fixtureId", &identifiers.fixtureId) ==
                  tinyxml2::XML_SUCCESS &&
              entry->QueryIntAttribute("fixtureIdNumeric",
                                       &identifiers.fixtureIdNumeric) ==
                  tinyxml2::XML_SUCCESS &&
              entry->QueryIntAttribute("unitNumber", &identifiers.unitNumber) ==
                  tinyxml2::XML_SUCCESS &&
              entry->Attribute("fixtureIdText") != nullptr;
          if (hasIdentifiers) {
            identifiers.fixtureIdText = entry->Attribute("fixtureIdText");
            projectFixtureIdentifiersByUuid.emplace(uuid,
                                                     std::move(identifiers));
          }
          const bool hasColorMarker =
              entry->Attribute("hasVisualColorHex") != nullptr;
          const std::string hasColor = ToLowerCopy(Trim(
              entry->Attribute("hasVisualColorHex")
                  ? entry->Attribute("hasVisualColorHex")
                  : ""));
          const std::string color = Trim(entry->Attribute("visualColorHex")
                                             ? entry->Attribute("visualColorHex")
                                             : "");
          if (!hasColorMarker && color.empty())
            continue;
          projectFixtureColorMetadataUuids.insert(uuid);
          const bool explicitClear = hasColorMarker && hasColor == "false";
          const bool present =
              (!hasColorMarker || hasColor == "true") && isHexRgb(color);
          if (!explicitClear && !present) {
            importResult.diagnostics.push_back(
                {"invalid_project_fixture_visual_color",
                 "Ignored invalid project fixture visualColorHex for UUID '" +
                     uuid + "'."});
            continue;
          }
          const std::string storedColor = explicitClear ? std::string{} : color;
          if (!projectFixtureColorsByUuid.emplace(uuid, storedColor).second) {
            importResult.diagnostics.push_back(
                {"duplicate_project_fixture_metadata_uuid",
                 "Ignored duplicate project fixture metadata for UUID '" +
                     uuid + "'; the first entry takes precedence."});
          }
        }
      }
    }
  };
  parseProjectFixtureMetadata(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, tinyxml2::XMLElement *> rootTrussInfoByUuid;
  std::unordered_set<std::string> consumedRootTrussInfoUuids;
  // Collects root-level Perastage truss metadata by canonical exported UUID.
  auto parseRootTrussInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (!isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap");
           map; map = map->NextSiblingElement("TrussInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo");
             info; info = info->NextSiblingElement("TrussInfo")) {
          const std::string uuid = CanonicalizeUuid(
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : ""));
          if (uuid.empty()) {
            importResult.diagnostics.push_back(
                {"invalid_truss_info_uuid",
                 "Ignored TrussInfo with a malformed UUID."});
          } else if (!rootTrussInfoByUuid.emplace(uuid, info).second) {
            importResult.diagnostics.push_back(
                {"duplicate_truss_info",
                 "Ignored duplicate TrussInfo for UUID '" + uuid + "'."});
          }
        }
      }
    }
  };
  parseRootTrussInfoMap(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, std::vector<std::string>>
      rootPrimitiveModelRefsBySceneObjectAndFile;
  // Collects root-level Perastage primitive geometry metadata by SceneObject
  // UUID and archive file name.
  auto parseRootPrimitiveGeometryMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map =
               data->FirstChildElement("PrimitiveGeometryMap");
           map; map = map->NextSiblingElement("PrimitiveGeometryMap")) {
        for (tinyxml2::XMLElement *entry = map->FirstChildElement("Entry");
             entry; entry = entry->NextSiblingElement("Entry")) {
          const char *fileName = entry->Attribute("fileName");
          const char *modelRef = entry->Attribute("perastageModelRef");
          if (!fileName || !modelRef)
            continue;
          const std::string rawSceneObjectUuid = Trim(
              entry->Attribute("sceneObjectUuid")
                  ? entry->Attribute("sceneObjectUuid")
                  : "");
          const std::string canonicalSceneObjectUuid =
              CanonicalizeUuid(rawSceneObjectUuid);
          if (rawSceneObjectUuid.empty() && canonicalSceneObjectUuid.empty())
            continue;
          const std::string normalizedFileName = ToLowerCopy(Trim(fileName));
          if (!canonicalSceneObjectUuid.empty()) {
            const std::string key =
                canonicalSceneObjectUuid + "|" + normalizedFileName;
            rootPrimitiveModelRefsBySceneObjectAndFile[key].push_back(
                Trim(modelRef));
          }
          if (!rawSceneObjectUuid.empty() &&
              rawSceneObjectUuid != canonicalSceneObjectUuid) {
            const std::string key =
                rawSceneObjectUuid + "|" + normalizedFileName;
            rootPrimitiveModelRefsBySceneObjectAndFile[key].push_back(
                Trim(modelRef));
          }
        }
      }
    }
  };
  parseRootPrimitiveGeometryMap(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, tinyxml2::XMLElement *> rootHoistInfoByUuid;
  std::unordered_set<std::string> consumedRootHoistInfoUuids;
  // Collects root-level Perastage hoist metadata by exported Support UUID.
  auto parseRootHoistInfoMap = [&](tinyxml2::XMLElement *userDataNode) {
    if (!userDataNode)
      return;
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (!isSupportedPerastageMetadata(data))
        continue;
      for (tinyxml2::XMLElement *map = data->FirstChildElement("HoistInfoMap");
           map; map = map->NextSiblingElement("HoistInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("HoistInfo");
             info; info = info->NextSiblingElement("HoistInfo")) {
          const std::string rawUuid =
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : "");
          const std::string canonicalUuid = CanonicalizeUuid(rawUuid);
          if (canonicalUuid.empty()) {
            importResult.diagnostics.push_back(
                {"invalid_hoist_info_uuid",
                 "Ignored HoistInfo with a malformed UUID."});
            continue;
          }
          if (!rootHoistInfoByUuid.emplace(canonicalUuid, info).second) {
            importResult.diagnostics.push_back(
                {"duplicate_hoist_info",
                 "Ignored duplicate HoistInfo for UUID '" + canonicalUuid +
                     "'."});
          }
        }
      }
    }
  };
  parseRootHoistInfoMap(root->FirstChildElement("UserData"));

  // ---- Parse AUXData for Symdefs and Positions ----
  if (tinyxml2::XMLElement *auxNode = sceneNode->FirstChildElement("AUXData")) {
    for (tinyxml2::XMLElement *pos = auxNode->FirstChildElement("Position");
         pos; pos = pos->NextSiblingElement("Position")) {
      const std::string rawUid =
          Trim(pos->Attribute("uuid") ? pos->Attribute("uuid") : "");
      const char *name = pos->Attribute("name");
      referenceResolver.ImportPosition(
          rawUid, name ? std::optional<std::string>{name} : std::nullopt, scene);
    }
    std::function<void(tinyxml2::XMLElement *, const Matrix &,
                       std::vector<SymdefGeometry> &)>
        parseSymdefChildList;
    parseSymdefChildList = [&](tinyxml2::XMLElement *childList,
                               const Matrix &parent,
                               std::vector<SymdefGeometry> &geometries) {
      for (tinyxml2::XMLElement *child =
               childList ? childList->FirstChildElement() : nullptr;
           child; child = child->NextSiblingElement()) {
        const char *name = child->Name();
        if (!name)
          continue;

        Matrix local = MatrixUtils::Identity();
        if (tinyxml2::XMLElement *matrix = child->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText()) {
            std::string raw = txt;
            if (!MatrixUtils::ParseMatrix(raw, local))
              local = MatrixUtils::Identity();
          }
        }
        Matrix composed = MatrixUtils::Multiply(parent, local);

        if (std::string(name) == "Geometry3D") {
          SymdefGeometry g;
          if (const char *fname = child->Attribute("fileName"))
            g.file = RemapArchivePathIfNeeded(fname);
          if (const char *type = child->Attribute("geometryType"))
            g.geometryType = Trim(type);
          g.transform = composed;
          if (!g.file.empty())
            geometries.push_back(std::move(g));
        }

        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseSymdefChildList(inner, composed, geometries);
      }
    };

    for (tinyxml2::XMLElement *sym = auxNode->FirstChildElement("Symdef"); sym;
         sym = sym->NextSiblingElement("Symdef")) {
      const char *uid = sym->Attribute("uuid");
      if (!uid)
        continue;

      if (const char *type = sym->Attribute("geometryType"))
        scene.symdefTypes[uid] = Trim(type);

      std::vector<SymdefGeometry> geometries;
      if (tinyxml2::XMLElement *childList = sym->FirstChildElement("ChildList"))
        parseSymdefChildList(childList, MatrixUtils::Identity(), geometries);

      if (!geometries.empty()) {
        scene.symdefGeometries[uid] = geometries;
        scene.symdefFiles[uid] = geometries.front().file;
        scene.symdefMatrices[uid] = geometries.front().transform;
        if (!geometries.front().geometryType.empty())
          scene.symdefTypes[uid] = geometries.front().geometryType;
      }
    }
  }

  constexpr float kTinyScaleMaxNorm = 0.01f;
  constexpr float kUniformScaleRelativeTolerance = 0.05f;
  constexpr float kMinOutlierNorm = 0.1f;
  constexpr float kMaxOutlierNorm = 10.0f;
  constexpr size_t kMaxSuspiciousExamples = 10;

  struct MatrixScaleAggregation {
    size_t acceptedTinyUniformScaleCount = 0;
    size_t suspiciousMatrixCount = 0;
    std::unordered_map<std::string, size_t> acceptedByContext;
    std::unordered_map<std::string, size_t> suspiciousByContext;
    std::vector<std::string> suspiciousExamples;
  } matrixScaleAggregation;

  auto parseMatrixOrIdentity = [&](tinyxml2::XMLElement *parent,
                                   const char *elementName,
                                   const std::string &contextTag, Matrix &out,
                                   bool inspectScale = false) {
    out = MatrixUtils::Identity();
    if (!parent)
      return;
    if (tinyxml2::XMLElement *matrix = parent->FirstChildElement(elementName)) {
      if (const char *txt = matrix->GetText()) {
        std::string raw = txt;
        if (!MatrixUtils::ParseMatrix(raw, out)) {
          LogMessage("Failed to parse matrix in " + contextTag + ": " + raw);
          out = MatrixUtils::Identity();
          return;
        }

        if (!inspectScale)
          return;

        auto norm = [](const std::array<float, 3> &v) {
          return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        };

        const float nu = norm(out.u);
        const float nv = norm(out.v);
        const float nw = norm(out.w);
        const float minNorm = std::min({nu, nv, nw});
        const float maxNorm = std::max({nu, nv, nw});

        const bool finiteNorms =
            std::isfinite(nu) && std::isfinite(nv) && std::isfinite(nw);
        const bool strictlyPositiveNorms = minNorm > 0.0f;
        const bool isUniformScale =
            IsNearlyEqualRelative(nu, nv, kUniformScaleRelativeTolerance) &&
                                    IsNearlyEqualRelative(nu, nw, kUniformScaleRelativeTolerance);
        const bool isTinyUniformGeometryScale =
            finiteNorms && strictlyPositiveNorms &&
            maxNorm <= kTinyScaleMaxNorm && isUniformScale &&
            IsGeometryMatrixContext(contextTag);
        if (isTinyUniformGeometryScale) {
          ++matrixScaleAggregation.acceptedTinyUniformScaleCount;
          ++matrixScaleAggregation.acceptedByContext[contextTag];
          return;
        }

        const bool hasInvalidNorm = !finiteNorms || !strictlyPositiveNorms;
        const bool hasOutlierNorm =
            minNorm < kMinOutlierNorm || maxNorm > kMaxOutlierNorm;
        if (!hasInvalidNorm && !hasOutlierNorm)
          return;

        ++matrixScaleAggregation.suspiciousMatrixCount;
        ++matrixScaleAggregation.suspiciousByContext[contextTag];
        if (matrixScaleAggregation.suspiciousExamples.size() <
            kMaxSuspiciousExamples) {
          std::ostringstream oss;
          oss << contextTag << " (|u|=" << nu << ", |v|=" << nv
              << ", |w|=" << nw << ")";
          matrixScaleAggregation.suspiciousExamples.push_back(oss.str());
        }
      }
    }
  };

  auto normalizeGeometryFileName = [](std::string fileName) {
    fileName = Trim(fileName);
    if (fileName.empty())
      return fileName;
    return ToString(PathUtils::PathFromUtf8(fileName).u8string());
  };

  std::unordered_map<std::string, GdtfFixtureCategory::InferenceResult>
      categoryInferenceByResolvedPath;
  mvr::MvrImportResourceResolver resources(
      PathUtils::PathFromUtf8(scene.basePath),
      [&](const std::string &path) { return RemapArchivePathIfNeeded(path); });
  auto normalizeAndResolveGeometryFileName = [&](const std::string &path) {
    return resources.NormalizeGeometryFile(path);
  };
  auto resolveGdtfPathCached = [&](const std::string &spec)
      -> const std::string & { return resources.ResolveGdtfPath(spec); };
  auto getFixtureMetadata = [&](const std::string &path)
      -> const mvr::ImportGdtfMetadata & {
    return resources.FixtureMetadata(path);
  };
  auto resolvedGdtfFileExists = [&](const std::string &path) {
    return resources.GdtfFileExists(path);
  };
  auto getGdtfModeChannelCountCached =
      [&](const std::string &path, const std::string &mode) {
        return resources.GdtfModeChannelCount(path, mode);
      };
  auto resolveExistingGdtfModeCached =
      [&](const std::string &path, const std::string &mode,
          std::optional<int> channelCount) {
        return resources.ResolveGdtfMode(path, mode, channelCount);
      };
  auto getDictionaryEntryCached = [&](const std::string &type)
      -> const std::optional<GdtfDictionary::Entry> & {
    return resources.DictionaryEntry(type);
  };

  auto appendGeometryInstance =
      [&](std::vector<GeometryInstance> &instances, const std::string &fileName,
          const Matrix &localTransform, const std::string &instanceKey,
                                    const std::string &sourceSymbolUuid = {},
                                    const std::string &sourceSymdefUuid = {}) {
    std::string normalized = normalizeAndResolveGeometryFileName(fileName);
    if (normalized.empty())
      return;
    GeometryInstance instance;
    instance.modelFile = normalized;
    instance.instanceKey = instanceKey;
    instance.sourceSymbolUuid = sourceSymbolUuid;
    instance.sourceSymdefUuid = sourceSymdefUuid;
    instance.localTransform = localTransform;
    instances.push_back(std::move(instance));
  };

  auto resolveSymdefReference = [&](tinyxml2::XMLElement *symbol,
                                    std::vector<SymdefGeometry> &outGeometries,
                                    std::string &outGeometryType,
                                    Matrix &outSymbolMatrix) {
    outGeometries.clear();
    outGeometryType.clear();
    outSymbolMatrix = MatrixUtils::Identity();

    if (!symbol)
      return;

    parseMatrixOrIdentity(symbol, "Matrix", "Symbol", outSymbolMatrix);

    const char *symdef = symbol->Attribute("symdef");
    if (!symdef)
      return;

    auto geosIt = scene.symdefGeometries.find(symdef);
    if (geosIt != scene.symdefGeometries.end() && !geosIt->second.empty()) {
      outGeometries = geosIt->second;
      for (auto &geo : outGeometries)
        geo.file = normalizeGeometryFileName(geo.file);
      for (const auto &geo : outGeometries) {
        if (!geo.geometryType.empty()) {
          outGeometryType = geo.geometryType;
          break;
        }
      }
      return;
    }

    auto it = scene.symdefFiles.find(symdef);
    if (it != scene.symdefFiles.end()) {
      SymdefGeometry fallback;
      fallback.file = normalizeGeometryFileName(it->second);
      auto mit = scene.symdefMatrices.find(symdef);
      if (mit != scene.symdefMatrices.end())
        fallback.transform = mit->second;
      auto tit = scene.symdefTypes.find(symdef);
      if (tit != scene.symdefTypes.end())
        fallback.geometryType = tit->second;
      if (!fallback.file.empty())
        outGeometries.push_back(std::move(fallback));
    }

    auto tit = scene.symdefTypes.find(symdef);
    if (tit != scene.symdefTypes.end())
      outGeometryType = tit->second;
  };

  // ---- Helper lambdas for object parsing ----
  int preservedGroupObjectCount = 0;
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const std::string &)>
      parseChildList;

  auto ensurePositionEntry = [&](const std::string &positionId) {
    return referenceResolver.EnsurePosition(positionId, scene);
  };

  using CachedCategory = mvr::SceneReadCachedCategory;
  std::unordered_map<std::string, CachedCategory> categoryByTypeKey;
  auto resolveStableUuid = [&](const char *kind, tinyxml2::XMLElement *node,
                               const std::string &layerName,
                               const Matrix &nodeTransform,
                               const std::string &legacyStableId = {}) {
    const char *uuidAttr = node->Attribute("uuid");
    const char *nameAttr = node->Attribute("name");
    return referenceResolver.ResolveStableUuid(
        {kind, layerName, nameAttr ? Trim(nameAttr) : "",
         MatrixUtils::FormatMatrix(nodeTransform),
         uuidAttr ? Trim(uuidAttr) : "", Trim(legacyStableId)});
  };

  auto referenceUuidForNode = [&](const char *kind, tinyxml2::XMLElement *node,
                                  const std::string &layerName,
                                  const Matrix &nodeTransform) {
    const char *uuidAttr = node->Attribute("uuid");
    const char *nameAttr = node->Attribute("name");
    return referenceResolver.ReferenceUuid(
        {kind, layerName, nameAttr ? Trim(nameAttr) : "",
         MatrixUtils::FormatMatrix(nodeTransform),
         uuidAttr ? Trim(uuidAttr) : "", {}});
  };
  std::unordered_map<std::string, GdtfConflict> pendingGdtfConflictByType;
  mvr::MvrSceneReadServices sceneReadServices{
      resources,
      textOf,
      intOf,
      fixtureIdOf,
      parseMatrixOrIdentity,
      buildFixtureTypeInfoKey,
      resolveStableUuid,
      referenceUuidForNode,
      [&](const std::string &rawUuid, const std::string &resolvedUuid) {
        referenceResolver.RecordFixtureUuid(rawUuid, resolvedUuid);
      },
      ensurePositionEntry,
      resolveSymdefReference,
      appendGeometryInstance,
      reportProgress,
      [](const std::string &message) {
        LogMessage(Logger::Level::Debug, message);
      },
      [](const std::string &message) {
        LogMessage(Logger::Level::Info, message);
      },
      [](const std::string &message) {
        LogMessage(Logger::Level::Warn, message);
      },
      [](const std::string &message) {
        LogMessage(Logger::Level::Error, message);
      }};
  mvr::MvrSceneReadMetadata sceneReadMetadata{
      {rootFixtureTypeInfoByKey, projectFixtureIdentifiersByUuid,
       projectFixtureColorsByUuid, projectFixtureColorMetadataUuids},
      {rootTrussInfoByUuid, rootHoistInfoByUuid, perastageTypeToGdtfPath,
       perastageInstanceToTypeKey},
      {rootPrimitiveModelRefsBySceneObjectAndFile,
       referenceResolver.LegacyPositionRemap()},
      {layerColorByUuid, layerColorByName}};
  mvr::MvrSceneReadState sceneReadState{
      {pendingGdtfConflictByType, categoryByTypeKey,
       categoryInferenceByResolvedPath, consumedProjectFixtureColorUuids},
      {consumedRootTrussInfoUuids, consumedRootHoistInfoUuids}};
  mvr::MvrSceneReadMetrics sceneReadMetrics;
  mvr::ReadMvrSceneNodes(sceneNode, scene, importResult, options,
                         sceneReadServices, sceneReadMetadata, sceneReadState,
                         sceneReadMetrics);

  const int trussSymbolSymdefPreservedCount =
      sceneReadMetrics.trussSymbolSymdefPreservedCount;
  const auto &trussSymbolSymdefPreservedBySymdef =
      sceneReadMetrics.trussSymbolSymdefPreservedBySymdef;

  auto resolveFixtureGdtfPathForRead = [&](const std::string &spec) {
    if (spec.empty())
      return std::string{};
    std::string remapped = RemapArchivePathIfNeeded(spec);
    return resolveGdtfPathCached(remapped);
  };

  // After parsing the entire scene, resolve any GDTF conflicts using the
  // dictionary only if requested. This occurs before rendering so user choices
  // are applied to the final scene data.
  if (options.applyDictionary) {
    std::unordered_map<std::string, GdtfConflict> gdtfConflictByType =
        pendingGdtfConflictByType;
    const int totalFixturesForConflictScan =
        static_cast<int>(scene.fixtures.size());
    int scannedFixturesForConflictScan = 0;
    for (const auto &[uid, f] : scene.fixtures) {
      (void)uid;
      ++scannedFixturesForConflictScan;
      if (totalFixturesForConflictScan > 0 &&
          (scannedFixturesForConflictScan == 1 ||
           scannedFixturesForConflictScan == totalFixturesForConflictScan ||
           scannedFixturesForConflictScan % 50 == 0)) {
        reportProgress("Preparing GDTF conflict analysis...",
                       scannedFixturesForConflictScan,
                       totalFixturesForConflictScan);
      }
      if (f.typeName.empty())
        continue;
      GdtfConflict &conflict = gdtfConflictByType[f.typeName];
      conflict.type = f.typeName;
      if (conflict.mvrPath.empty())
        conflict.mvrPath = f.gdtfSpec;
      if (conflict.requestedFixtureName.empty()) {
        conflict.requestedFixtureName =
            f.requestedFixtureName.empty()
                ? mvr::gdtf_import_matching::ExtractFixtureNameFromGdtfSpec(
                      f.gdtfSpec)
                : f.requestedFixtureName;
      }
      if (conflict.fixtureName.empty())
        conflict.fixtureName = f.typeName;
      if (conflict.modeName.empty())
        conflict.modeName = f.gdtfMode;
      if (conflict.fixtureTypeId.empty()) {
        const std::string resolvedGdtfPath =
            resolveFixtureGdtfPathForRead(f.gdtfSpec);
        conflict.fixtureTypeId =
            getFixtureMetadata(resolvedGdtfPath).fixtureTypeId;
      }
      if (conflict.footprint <= 0) {
        const std::string resolvedGdtfPath =
            resolveFixtureGdtfPathForRead(f.gdtfSpec);
        if (!resolvedGdtfPath.empty() && !f.gdtfMode.empty())
          conflict.footprint =
              getGdtfModeChannelCountCached(resolvedGdtfPath, f.gdtfMode);
      }

      const auto &dictEntry = getDictionaryEntryCached(f.typeName);
      if (dictEntry) {
        conflict.appPath = dictEntry->path;
        conflict.hasDictionaryEntry = true;
      }
    }

    std::vector<GdtfConflict> gdtfConflicts;
    gdtfConflicts.reserve(gdtfConflictByType.size());
    for (const auto &[typeName, conflict] : gdtfConflictByType) {
      (void)typeName;
      if (conflict.type.empty())
        continue;
      gdtfConflicts.push_back(conflict);
    }
    if (!gdtfConflicts.empty()) {
      if (options.promptConflicts) {
        reportProgress("Conflict dialog:show");
        auto choices = PromptGdtfConflicts(gdtfConflicts);
        reportProgress("Conflict dialog:hide");
        if (!choices.empty()) {
          std::unordered_map<std::string, std::string> selectedPathByType;
          std::unordered_map<std::string, std::string> selectedModeByType;
          std::vector<GdtfConflict> downloadRequests;
          for (const auto &conflict : gdtfConflicts) {
            const auto it = choices.find(conflict.type);
            if (it == choices.end() ||
                it->second.choice == GdtfConflictChoice::App) {
              selectedPathByType[conflict.type] = conflict.appPath;
            } else if (it->second.choice == GdtfConflictChoice::Mvr) {
              selectedPathByType[conflict.type] = conflict.mvrPath;
            } else {
              downloadRequests.push_back(conflict);
              const std::string fallbackPath =
                  GetDownloadFallbackPath(conflict);
              selectedPathByType[conflict.type] = fallbackPath;
            }
          }

          if (!downloadRequests.empty()) {
            auto parseAddressToAbsoluteChannel =
                [](const std::string &address) {
              const std::string trimmed = Trim(address);
              const size_t dotPos = trimmed.find('.');
              if (dotPos == std::string::npos)
                return -1;
                  const int universe =
                      std::atoi(trimmed.substr(0, dotPos).c_str());
                  const int channel =
                      std::atoi(trimmed.substr(dotPos + 1).c_str());
              if (universe <= 0 || channel <= 0)
                return -1;
              return (universe - 1) * 512 + channel;
            };
            auto inferFootprintFromAddresses =
                [&](const std::string &typeName) {
              std::vector<int> channels;
              for (const auto &[fixtureUuid, fixture] : scene.fixtures) {
                (void)fixtureUuid;
                if (fixture.typeName != typeName)
                  continue;
                    const int absolute =
                        parseAddressToAbsoluteChannel(fixture.address);
                if (absolute > 0)
                  channels.push_back(absolute);
              }
              if (channels.size() < 2)
                return 0;
              std::sort(channels.begin(), channels.end());
              int best = 0;
              for (size_t i = 1; i < channels.size(); ++i) {
                const int diff = channels[i] - channels[i - 1];
                if (diff > 0 && (best == 0 || diff < best))
                  best = diff;
              }
              return best;
            };

            reportProgress("Conflict dialog:show");
            reportProgress("Trying to download selected GDTFs...");
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
            CredentialStore::LoadResult loadedCredentials =
                CredentialStore::LoadDetailed();
            std::optional<CredentialStore::Credentials> activeCredentials =
                loadedCredentials.credentials;
            GdtfShareClient gdtfClient;
            auto requestCredentials = [&]() -> bool {
              const std::string initialUser = activeCredentials
                                                  ? activeCredentials->username
                                                  : loadedCredentials.usernameHint.value_or(std::string());
              const std::string initialPass = activeCredentials
                                                  ? activeCredentials->password
                                                  : std::string();
              GdtfLoginDialog loginDlg(nullptr, initialUser, initialPass);
              if (loginDlg.ShowModal() != wxID_OK)
                return false;
              CredentialStore::Credentials entered;
              entered.username = Trim(loginDlg.GetUsername());
              entered.password = loginDlg.GetPassword();
              if (entered.username.empty() || entered.password.empty())
                return false;
              activeCredentials = entered;
              return true;
            };

            GdtfShareResult loginResult;
            bool loginOk = false;
            if (activeCredentials.has_value()) {
              loginResult = gdtfClient.Login(activeCredentials->username,
                                             activeCredentials->password);
              loginOk = loginResult.Succeeded();
            }
            if (!loginOk && (!activeCredentials ||
                             loginResult.category ==
                                 GdtfShareResultCategory::AuthenticationRejected)) {
              if (requestCredentials()) {
                gdtfClient.ResetSession();
                loginResult = gdtfClient.Login(activeCredentials->username,
                                               activeCredentials->password);
                loginOk = loginResult.Succeeded();
                if (loginOk) {
                  const CredentialStore::Result saveResult =
                      CredentialStore::Save(*activeCredentials);
                  if (!saveResult.Succeeded()) {
                    reportProgress("[WARN] GDTF Share credentials authenticated but the password was not persisted: " +
                                   CredentialStore::StatusName(saveResult.status));
                    wxMessageBox(saveResult.status == CredentialStore::Status::SecureStoreUnavailable
                                     ? _("The username was saved, but secure password storage is unavailable. The password must be entered again after restart.")
                                     : wxString::Format(_("GDTF Share credentials were authenticated for this operation, but were not saved (%s)."),
                                                        wxString::FromUTF8(CredentialStore::StatusName(saveResult.status))),
                                 _("GDTF Share credentials"), wxOK | wxICON_WARNING);
                  }
                }
              }
            }

            if (loginOk) {
              wxWindow *dialogParent =
                  wxTheApp ? wxDynamicCast(wxTheApp->GetTopWindow(), wxWindow)
                           : nullptr;
              wxDialog downloadInfoDialog(dialogParent, wxID_ANY,
                                          _("GDTF download queue"),
                                          wxDefaultPosition, wxSize(1140, 580));
              wxBoxSizer *infoSizer = new wxBoxSizer(wxVERTICAL);
              enum class DownloadRowState {
                Pending,
                Downloading,
                Downloaded,
                Fallback,
                Canceled
              };

              auto rowTextColor = [](DownloadRowState state) -> wxColour {
                switch (state) {
                case DownloadRowState::Downloaded:
                  return GdtfResolutionStatusColour(
                      rider_fixture_resolution::StatusSemantic::Success);
                case DownloadRowState::Fallback:
                  return GdtfResolutionStatusColour(
                      rider_fixture_resolution::StatusSemantic::Warning);
                case DownloadRowState::Canceled:
                  return GdtfResolutionStatusColour(
                      rider_fixture_resolution::StatusSemantic::Muted);
                case DownloadRowState::Downloading:
                  return GdtfResolutionStatusColour(
                      rider_fixture_resolution::StatusSemantic::Information);
                case DownloadRowState::Pending:
                default:
                  return GdtfResolutionStatusColour(
                      rider_fixture_resolution::StatusSemantic::Neutral);
                }
              };

              wxStaticText *summaryText =
                  new wxStaticText(&downloadInfoDialog, wxID_ANY,
                                   _("Selected fixture types for download"));
              wxFont summaryFont = summaryText->GetFont();
              summaryFont.SetWeight(wxFONTWEIGHT_BOLD);
              summaryText->SetFont(summaryFont);
              infoSizer->Add(summaryText, 0, wxLEFT | wxRIGHT | wxTOP, 8);
              wxStaticText *progressPhaseText =
                  new wxStaticText(&downloadInfoDialog, wxID_ANY,
                                   _("Preparing download queue..."));
              progressPhaseText->SetForegroundColour(wxColour(140, 140, 140));
              infoSizer->Add(progressPhaseText, 0, wxLEFT | wxRIGHT | wxTOP, 8);
              wxGauge *progressGauge = new wxGauge(
                  &downloadInfoDialog, wxID_ANY, 100, wxDefaultPosition,
                  wxSize(-1, 6), wxGA_HORIZONTAL | wxGA_SMOOTH);
              progressGauge->SetForegroundColour(wxColour(80, 145, 90));
              progressGauge->SetBackgroundColour(wxColour(52, 52, 52));
              progressGauge->SetValue(0);
              infoSizer->Add(progressGauge, 0,
                             wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

              wxListCtrl *downloadInfoList = new wxListCtrl(
                  &downloadInfoDialog, wxID_ANY, wxDefaultPosition,
                  wxDefaultSize,
                  wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
              downloadInfoList->InsertColumn(0, _("Fixture type"),
                                             wxLIST_FORMAT_LEFT, 260);
              downloadInfoList->InsertColumn(1, _("Selected GDTF"),
                                             wxLIST_FORMAT_LEFT, 460);
              downloadInfoList->InsertColumn(2, _("Status"), wxLIST_FORMAT_LEFT,
                                             170);
              downloadInfoList->InsertColumn(3, _("Progress"),
                                             wxLIST_FORMAT_LEFT, 220);
              downloadInfoList->InsertColumn(4, _("Details"),
                                             wxLIST_FORMAT_LEFT, 180);
              infoSizer->Add(downloadInfoList, 1, wxEXPAND | wxALL, 8);
              wxStaticText *footerSummary = new wxStaticText(
                  &downloadInfoDialog, wxID_ANY,
                                   _("0 processed  |  0 downloaded  |  0 fallback"));
              footerSummary->SetForegroundColour(wxColour(100, 100, 100));
              infoSizer->Add(footerSummary, 0, wxLEFT | wxRIGHT, 8);
              wxStaticText *bytesSummary =
                  new wxStaticText(&downloadInfoDialog, wxID_ANY, "0 B / ? B");
              bytesSummary->SetForegroundColour(wxColour(110, 110, 110));
              infoSizer->Add(bytesSummary, 0, wxLEFT | wxRIGHT | wxTOP, 8);
              wxBoxSizer *actionSizer = new wxBoxSizer(wxHORIZONTAL);
              wxButton *cancelButton =
                  new wxButton(&downloadInfoDialog, wxID_CANCEL, _("Cancel"));
              wxButton *ackButton =
                  new wxButton(&downloadInfoDialog, wxID_OK, _("OK"));
              ackButton->Disable();
              actionSizer->Add(cancelButton, 0, wxRIGHT, 8);
              actionSizer->Add(ackButton, 0);
              infoSizer->Add(actionSizer, 0,
                             wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);
              downloadInfoDialog.SetSizer(infoSizer);
              if (dialogParent) {
                downloadInfoDialog.CentreOnParent();
              } else {
                downloadInfoDialog.CentreOnScreen();
              }
              bool isDownloadInfoFinished = false;
              std::atomic<bool> cancelRequested{false};
              auto downloadUiActive = std::make_shared<std::atomic<bool>>(true);
              downloadInfoDialog.Bind(wxEVT_CLOSE_WINDOW,
                                      [&](wxCloseEvent &closeEvent) {
                                        if (!isDownloadInfoFinished) {
                                          cancelRequested.store(true);
                                          closeEvent.Veto();
                                          return;
                                        }
                                        downloadUiActive->store(false);
                                        downloadInfoDialog.Hide();
                                      });
              cancelButton->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
                cancelRequested.store(true);
                cancelButton->Disable();
                progressPhaseText->SetLabel(
                    _("Cancel requested. Finishing current transfer..."));
              });
              ackButton->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
                downloadUiActive->store(false);
                downloadInfoDialog.Hide();
              });

              std::unordered_map<std::string, long> rowByType;
              std::unordered_map<std::string, DownloadRowState> rowStateByType;
              struct DownloadProgressStats {
                long long downloadedBytes = 0;
                long long totalBytes = -1;
              };
              std::unordered_map<std::string, DownloadProgressStats>
                  rowProgressByType;
              const auto queueStartTime = std::chrono::steady_clock::now();
              auto formatBytes = [](long long bytes) -> wxString {
                if (bytes < 0)
                  return "? B";
                static const char *kUnits[] = {"B", "KB", "MB", "GB", "TB"};
                double value = static_cast<double>(bytes);
                size_t unitIndex = 0;
                while (value >= 1024.0 && unitIndex < 4) {
                  value /= 1024.0;
                  ++unitIndex;
                }
                if (unitIndex == 0)
                  return wxString::Format("%lld %s", bytes, kUnits[unitIndex]);
                return wxString::Format("%.1f %s", value, kUnits[unitIndex]);
              };
              auto formatEta = [](long long seconds) -> wxString {
                if (seconds < 0)
                  return "ETA --:--";
                const long long mins = seconds / 60;
                const long long secs = seconds % 60;
                return wxString::Format("ETA %02lld:%02lld", mins, secs);
              };
              auto refreshFooterSummary = [&]() {
                int downloaded = 0;
                int fallback = 0;
                int canceled = 0;
                int processed = 0;
                for (const auto &[typeKey, state] : rowStateByType) {
                  (void)typeKey;
                  if (state == DownloadRowState::Downloaded) {
                    ++downloaded;
                    ++processed;
                  } else if (state == DownloadRowState::Fallback) {
                    ++fallback;
                    ++processed;
                  } else if (state == DownloadRowState::Canceled) {
                    ++canceled;
                    ++processed;
                  }
                }
                footerSummary->SetLabel(
                    wxString::Format(_("%d/%zu processed  |  %d downloaded  |  "
                                       "%d fallback  |  %d canceled"),
                                     processed, rowStateByType.size(),
                                     downloaded, fallback, canceled));
              };
              auto refreshBytesSummary = [&]() {
                long long downloaded = 0;
                long long knownTotal = 0;
                bool hasUnknownTotal = false;
                for (const auto &[typeKey, progress] : rowProgressByType) {
                  (void)typeKey;
                  downloaded +=
                      std::max<long long>(0, progress.downloadedBytes);
                  if (progress.totalBytes > 0) {
                    knownTotal += progress.totalBytes;
                  } else {
                    hasUnknownTotal = true;
                  }
                }
                const wxString totalLabel =
                    hasUnknownTotal ? wxString("? B") : formatBytes(knownTotal);
                wxString label = formatBytes(downloaded) + " / " + totalLabel;
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - queueStartTime)
                        .count();
                if (!hasUnknownTotal && knownTotal > downloaded &&
                    elapsed > 0 && downloaded > 0) {
                  const double speed = static_cast<double>(downloaded) /
                                       static_cast<double>(elapsed);
                  if (speed > 0.0) {
                    const long long remainingSeconds = static_cast<long long>(
                        static_cast<double>(knownTotal - downloaded) / speed);
                    label += "  |  " + formatEta(remainingSeconds);
                  }
                }
                bytesSummary->SetLabel(label);
              };
              auto updateStatusRow = [&](const std::string &typeKey,
                                         const wxString &selectedGdtf,
                                         const wxString &status,
                                         const wxString &progressText,
                                         const wxString &details,
                                         DownloadRowState state) {
                const auto rowIt = rowByType.find(typeKey);
                if (rowIt == rowByType.end())
                  return;
                const long row = rowIt->second;
                downloadInfoList->SetItem(row, 1, selectedGdtf);
                downloadInfoList->SetItem(row, 2, status);
                downloadInfoList->SetItem(row, 3, progressText);
                downloadInfoList->SetItem(row, 4, details);
                downloadInfoList->SetItemTextColour(row, rowTextColor(state));
                rowStateByType[typeKey] = state;
                refreshFooterSummary();
                refreshBytesSummary();
              };
              auto fallbackStatusText = [&](const GdtfConflict &request) {
                if (!request.appPath.empty() &&
                    resolvedGdtfFileExists(
                        resolveFixtureGdtfPathForRead(request.appPath)))
                  return wxString(_("Fallback to App"));
                if (!request.mvrPath.empty() &&
                    resolvedGdtfFileExists(
                        resolveFixtureGdtfPathForRead(request.mvrPath)))
                  return wxString(_("Fallback to MVR"));
                return wxString(_("Fallback to dummy"));
              };

              auto updateProgressGauge = [&]() {
                long long downloaded = 0;
                long long knownTotal = 0;
                bool hasUnknownTotal = false;
                for (const auto &[typeKey, progress] : rowProgressByType) {
                  (void)typeKey;
                  downloaded +=
                      std::max<long long>(0, progress.downloadedBytes);
                  if (progress.totalBytes > 0) {
                    knownTotal += progress.totalBytes;
                  } else {
                    hasUnknownTotal = true;
                  }
                }
                if (!hasUnknownTotal && knownTotal > 0) {
                  const int value = static_cast<int>(
                      std::clamp((static_cast<double>(downloaded) * 100.0) /
                                     static_cast<double>(knownTotal),
                                 0.0, 100.0));
                  progressGauge->SetValue(value);
                  return;
                }
                const int total = static_cast<int>(rowStateByType.size());
                int finished = 0;
                for (const auto &[typeKey, state] : rowStateByType) {
                  (void)typeKey;
                  if (state == DownloadRowState::Downloaded ||
                      state == DownloadRowState::Fallback ||
                      state == DownloadRowState::Canceled) {
                    ++finished;
                  }
                }
                const int safeTotal = std::max(1, total);
                progressGauge->SetValue((finished * 100) / safeTotal);
              };
              // Schedules progress updates on the UI thread without blocking
              // worker callbacks.
              auto runOnUiThread = [&](std::function<void()> task) {
                // Drops worker-thread progress updates to avoid deferred UI
                // races during teardown.
                if (!wxThread::IsMain())
                  return;
                if (!downloadUiActive->load())
                  return;
                task();
              };

              downloadInfoDialog.Show();
              wxYieldIfNeeded();
              for (const GdtfConflict &req : downloadRequests) {
                const long row = downloadInfoList->InsertItem(
                    downloadInfoList->GetItemCount(),
                    wxString::FromUTF8(req.type));
                downloadInfoList->SetItem(row, 1, "-");
                downloadInfoList->SetItem(row, 2, _("Pending"));
                downloadInfoList->SetItem(row, 3, "0 B / ? B");
                downloadInfoList->SetItem(row, 4,
                                          _("Waiting to match catalog entry"));
                downloadInfoList->SetItemTextColour(
                    row, rowTextColor(DownloadRowState::Pending));
                rowByType[req.type] = row;
                rowStateByType[req.type] = DownloadRowState::Pending;
                rowProgressByType[req.type] = DownloadProgressStats{};
              }
              refreshFooterSummary();
              refreshBytesSummary();
              updateProgressGauge();
              progressPhaseText->SetLabel(_("Loading GDTF catalog..."));
              wxYieldIfNeeded();

              std::string listPayload;
              GdtfShareResult listResult;
              reportProgress("Downloading selected GDTFs: loading catalog...");

              GdtfCatalogService catalogService;
              const std::string refreshNowUtc =
                  wxDateTime::UNow().FormatISOCombined(' ').ToStdString();
              const GdtfCatalogRefreshResult catalogResult =
                  catalogService.RefreshCatalogIfStale(
                      [&](std::string &onlineListData) {
                        listResult = gdtfClient.GetCatalog();
                        onlineListData = listResult.payload;
                        return listResult.Succeeded() &&
                               !onlineListData.empty();
                      },
                      refreshNowUtc);
              if (catalogResult.snapshot) {
                listPayload = catalogResult.snapshot->listData;
              }
              reportProgress(wxString::Format(
                                 "[METRIC] GDTF import catalog cache_hit=%d "
                                 "cache_miss=%d cache_age_s=%lld "
                                 "refresh_attempted=%d refresh_succeeded=%d",
                                   catalogResult.metrics.cacheHit ? 1 : 0,
                                   catalogResult.metrics.cacheMiss ? 1 : 0,
                                 static_cast<long long>(
                                     catalogResult.metrics.cacheAgeSeconds),
                                   catalogResult.metrics.refreshAttempted ? 1 : 0,
                                   catalogResult.metrics.refreshSucceeded ? 1 : 0)
                      .ToStdString());

              std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
                  catalogEntries;
              mvr::gdtf_catalog_parser::GdtfCatalogParseResult parsedCatalog;
              GdtfCatalogResultSource effectiveCatalogSource =
                  catalogResult.source;
              std::string effectiveCatalogUpdatedAt =
                  catalogResult.snapshot ? catalogResult.snapshot->updatedAt
                                         : std::string{};
              std::string catalogFailureReason;
              if (!listPayload.empty()) {
                parsedCatalog =
                    mvr::gdtf_catalog_parser::ParseCatalog(listPayload);
                catalogEntries = parsedCatalog.entries;
              }

              auto hasDownloadableCatalogEntry = [&]() {
                return std::any_of(
                    catalogEntries.begin(), catalogEntries.end(),
                    [](const auto &entry) { return entry.downloadable; });
              };
              if (!hasDownloadableCatalogEntry()) {
                reportProgress("[INFO] Cached catalog did not provide usable "
                               "entries; forcing online refresh.");
                const GdtfCatalogRefreshResult forcedCatalogResult =
                    catalogService.RefreshCatalogIfStale(
                        [&](std::string &onlineListData) {
                          listResult = gdtfClient.GetCatalog();
                          onlineListData = listResult.payload;
                          return listResult.Succeeded() &&
                                 !onlineListData.empty();
                        },
                        refreshNowUtc, 0);
                if (forcedCatalogResult.snapshot) {
                  listPayload = forcedCatalogResult.snapshot->listData;
                  parsedCatalog =
                      mvr::gdtf_catalog_parser::ParseCatalog(listPayload);
                  catalogEntries = parsedCatalog.entries;
                  effectiveCatalogSource = forcedCatalogResult.source;
                  effectiveCatalogUpdatedAt =
                      forcedCatalogResult.snapshot->updatedAt;
                }
                reportProgress(
                    wxString::Format(
                        "[METRIC] GDTF import forced_refresh attempted=%d "
                        "succeeded=%d entries=%zu",
                                     forcedCatalogResult.metrics.refreshAttempted ? 1 : 0,
                                     forcedCatalogResult.metrics.refreshSucceeded ? 1 : 0,
                                     catalogEntries.size())
                        .ToStdString());
              }

              if (!hasDownloadableCatalogEntry()) {
                catalogFailureReason =
                    wxString::Format(
                        "Catalog fetch/parsing failed (%s, HTTP %ld, bytes=%zu)",
                                     wxString::FromUTF8(GdtfShareResultCategoryName(listResult.category)),
                                     listResult.httpStatus, listPayload.size())
                        .ToStdString();
                reportProgress("[WARN] " + catalogFailureReason);
              }

              if (hasDownloadableCatalogEntry()) {
                // Reuses one authoritative download when distinct import aliases
                // explicitly resolve to the same GDTF Share revision.
                std::unordered_map<std::string, std::string>
                    downloadedPathByReplacement;
                std::unordered_map<std::string, std::string>
                    downloadedModeByReplacement;
                summaryText->SetLabel(
                    wxString::Format(_("Selected fixture types for download (catalog entries: %zu)"),
                    catalogEntries.size()));
                progressGauge->SetValue(25);
                progressPhaseText->SetLabel(_("Downloading selected fixtures..."));
                for (GdtfConflict req : downloadRequests) {
                  if (cancelRequested.load()) {
                    break;
                  }
                  reportProgress("Downloading selected GDTFs: matching " +
                                 req.type + "...");
                  if (req.footprint <= 0)
                    req.footprint = inferFootprintFromAddresses(req.type);
                  mvr::gdtf_import_matching::AutomaticMatchEvidence
                      matchEvidence;
                  matchEvidence.displayTypeKey = req.type;
                  matchEvidence.resolvedFixtureName = req.fixtureName;
                  matchEvidence.requestedFixtureName = req.requestedFixtureName;
                  matchEvidence.manufacturer = req.manufacturer;
                  matchEvidence.fixtureTypeId = req.fixtureTypeId;
                  matchEvidence.modeName = req.modeName;
                  matchEvidence.footprint = req.footprint;
                  const auto matchRequest =
                      mvr::gdtf_import_matching::BuildDownloadRequest(
                          matchEvidence);
                  auto diagnosticMatchRequest = matchRequest;
                  diagnosticMatchRequest.catalogSnapshotSource =
                      effectiveCatalogSource == GdtfCatalogResultSource::Online
                          ? "online"
                          : "cache";
                  diagnosticMatchRequest.catalogUpdatedAt =
                      effectiveCatalogUpdatedAt;
                  diagnosticMatchRequest.catalogPayloadBytes =
                      parsedCatalog.payloadBytes;
                  diagnosticMatchRequest.catalogPayloadFingerprint =
                      parsedCatalog.payloadFingerprint;
                  diagnosticMatchRequest.catalogParsedEntryCount =
                      parsedCatalog.entries.size();
                  const auto bestMatch =
                      gdtf_catalog_matcher::SelectBestDownloadMatch(
                          diagnosticMatchRequest, catalogEntries);

                  if (!bestMatch.found || bestMatch.rid.empty()) {
                    if (!bestMatch.selectionReason.empty())
                      reportProgress("GDTF catalog no-match diagnostics: " +
                                     bestMatch.selectionReason);
                    const wxString progressText =
                        rowProgressByType[req.type].totalBytes > 0
                            ? formatBytes(
                                  rowProgressByType[req.type].downloadedBytes) +
                                  " / " +
                                  formatBytes(
                                      rowProgressByType[req.type].totalBytes)
                            : wxString("0 B / ? B");
                    updateStatusRow(req.type, "-", fallbackStatusText(req),
                                    progressText, _("No catalog match found"),
                                    DownloadRowState::Fallback);
                    updateProgressGauge();
                    wxYieldIfNeeded();
                    continue;
                  }
                  reportProgress(
                      "GDTF automatic match diagnostics: build-version='" +
                      std::string(perastage::build_info::appVersion()) +
                      "'; build-commit='" +
                      std::string(perastage::build_info::gitCommit()) +
                      "'; queue-type='" + req.type + "'; resolved-fixture='" +
                      req.fixtureName + "'; winner-rid='" + bestMatch.rid +
                      "'; " + bestMatch.selectionReason);

                  const std::string baseFixturesPath =
#ifdef NDEBUG
                      ProjectUtils::GetWritableLibraryPath("fixtures");
#else
                      (PathUtils::PathFromUtf8(wxStandardPaths::Get()
                                                   .GetExecutablePath()
                                                   .ToStdString())
                           .parent_path() /
                       "library" / "fixtures")
                          .string();
#endif
                  fs::create_directories(baseFixturesPath);
                  const std::string filePath =
                      (fs::path(baseFixturesPath) / (req.type + ".gdtf"))
                          .string();
                  GdtfShareResult downloadResult;
                  wxString selectedFixtureName = wxString::FromUTF8(req.type);
                  for (const auto &entry : catalogEntries) {
                    if (entry.rid == bestMatch.rid) {
                      wxString manufacturer =
                          wxString::FromUTF8(entry.manufacturer);
                      wxString fixtureName =
                          wxString::FromUTF8(entry.fixtureName);
                      if (!manufacturer.empty() && !fixtureName.empty()) {
                        selectedFixtureName =
                            manufacturer + " / " + fixtureName;
                      } else if (!fixtureName.empty()) {
                        selectedFixtureName = fixtureName;
                      }
                      break;
                    }
                  }
                  updateStatusRow(req.type, selectedFixtureName,
                                  _("Downloading"), "0 B / ? B",
                                  _("Fetching fixture package"),
                                  DownloadRowState::Downloading);
                  reportProgress("Downloading selected GDTFs: downloading " +
                                 req.type + "...");
                  auto formatRowProgress =
                      [&](const DownloadProgressStats &stats,
                                               double percent) -> wxString {
                    wxString totalText = stats.totalBytes > 0
                                             ? formatBytes(stats.totalBytes)
                                             : wxString("? B");
                    wxString bytesText =
                        formatBytes(stats.downloadedBytes) + " / " + totalText;
                    if (stats.totalBytes > 0) {
                      bytesText += wxString::Format(
                          " (%.0f%%)", std::clamp(percent, 0.0, 100.0));
                    }
                    return bytesText;
                  };
                  const std::string replacementIdentity = mvr::
                      gdtf_import_matching::BuildSelectedReplacementIdentity(
                          bestMatch.rid, bestMatch.modeName);
                  const auto reusedDownload =
                      downloadedPathByReplacement.find(replacementIdentity);
                  if (reusedDownload != downloadedPathByReplacement.end()) {
                    selectedPathByType[req.type] = reusedDownload->second;
                    const auto reusedMode =
                        downloadedModeByReplacement.find(replacementIdentity);
                    if (reusedMode != downloadedModeByReplacement.end())
                      selectedModeByType[req.type] = reusedMode->second;
                    downloadResult.category = GdtfShareResultCategory::Success;
                    reportProgress(
                        "[INFO] GDTF replacement reuse revision='" +
                        bestMatch.rid + "' alias='" + req.type +
                        "' canonical='" +
                        fs::path(reusedDownload->second).filename().string() +
                        "'");
                  } else {
                    downloadResult = gdtfClient.DownloadRevision(
                          bestMatch.rid, filePath,
                                   [&](const GdtfDownloadProgress &progress) {
                                     const long long downloadedBytes =
                                std::max<long long>(0,
                                                    progress.downloadedBytes);
                                     const long long totalBytes =
                                progress.totalBytes > 0 ? progress.totalBytes
                                                        : -1;
                                     const double percentage = progress.percentage;
                                     const std::string typeKey = req.type;
                                     const wxString selectedFixtureNameCopy =
                                         selectedFixtureName;
                            runOnUiThread([=, &rowProgressByType,
                                           &updateStatusRow, &formatRowProgress,
                                           &updateProgressGauge]() {
                                       auto &stats = rowProgressByType[typeKey];
                                       stats.downloadedBytes = downloadedBytes;
                                       stats.totalBytes = totalBytes;
                              updateStatusRow(
                                  typeKey, selectedFixtureNameCopy,
                                                       _("Downloading"),
                                                       formatRowProgress(stats, percentage),
                                                       _("Fetching fixture package"),
                                                       DownloadRowState::Downloading);
                                       updateProgressGauge();
                                     });
                                   },
                                   [&]() { return cancelRequested.load(); });
                  }
                  if (downloadResult.Succeeded()) {
                    const std::string canonicalPath =
                        reusedDownload != downloadedPathByReplacement.end()
                            ? reusedDownload->second
                            : filePath;
                    selectedPathByType[req.type] = canonicalPath;
                    if (!bestMatch.modeName.empty())
                      selectedModeByType[req.type] = bestMatch.modeName;
                    downloadedPathByReplacement.emplace(replacementIdentity,
                                                     canonicalPath);
                    downloadedModeByReplacement.emplace(replacementIdentity,
                                                     bestMatch.modeName);
                    wxString details = _("Downloaded and assigned");
                    if (!bestMatch.modeName.empty())
                      details +=
                          wxString::Format(_(" (Mode: %s)"), wxString::FromUTF8(bestMatch.modeName));
                    if (!bestMatch.selectionReason.empty())
                      details += " [" +
                                 wxString::FromUTF8(bestMatch.selectionReason) +
                                 "]";
                    auto &stats = rowProgressByType[req.type];
                    if (stats.totalBytes > 0) {
                      stats.downloadedBytes = stats.totalBytes;
                    }
                    updateStatusRow(
                        req.type, selectedFixtureName, _("Success"),
                                    formatRowProgress(rowProgressByType[req.type], 100.0),
                        details, DownloadRowState::Downloaded);
                  } else if (cancelRequested.load()) {
                    const wxString totalText =
                        rowProgressByType[req.type].totalBytes > 0
                            ? formatBytes(
                                  rowProgressByType[req.type].totalBytes)
                            : wxString("? B");
                    updateStatusRow(
                        req.type, selectedFixtureName, _("Canceled"),
                        formatBytes(
                            rowProgressByType[req.type].downloadedBytes) +
                                        " / " + totalText,
                        _("Canceled by user"), DownloadRowState::Canceled);
                    break;
                  } else {
                    const wxString totalText =
                        rowProgressByType[req.type].totalBytes > 0
                            ? formatBytes(
                                  rowProgressByType[req.type].totalBytes)
                            : wxString("? B");
                    updateStatusRow(
                        req.type, selectedFixtureName, fallbackStatusText(req),
                        formatBytes(
                            rowProgressByType[req.type].downloadedBytes) +
                                        " / " + totalText,
                                    _("Download failed"), DownloadRowState::Fallback);
                  }
                  updateProgressGauge();
                  wxYieldIfNeeded();
                }
                if (cancelRequested.load()) {
                  for (const GdtfConflict &req : downloadRequests) {
                    if (rowStateByType[req.type] == DownloadRowState::Pending) {
                      updateStatusRow(req.type, "-", _("Canceled"), "0 B / ? B",
                                      _("Canceled before download start"),
                                      DownloadRowState::Canceled);
                    }
                  }
                  progressPhaseText->SetLabel(
                      _("Queue canceled. Keeping downloaded fixtures."));
                } else {
                  progressPhaseText->SetLabel(_("Queue finished."));
                }
              } else {
                if (catalogFailureReason.empty())
                  catalogFailureReason = std::string(_("Catalog fetch/parsing failed.").ToUTF8());
                summaryText->SetLabel(wxString::Format(
                    _("Selected fixture types for download (catalog load failed: %s)"),
                    wxString::FromUTF8(catalogFailureReason)));
                for (const GdtfConflict &req : downloadRequests) {
                  updateStatusRow(req.type, "-", fallbackStatusText(req),
                                  "0 B / ? B",
                                  _("Failed to load catalog list"),
                                  DownloadRowState::Fallback);
                }
                updateProgressGauge();
                progressPhaseText->SetLabel(wxString::Format(
                    _("Catalog load failed. %s. Keeping available fallbacks."),
                    wxString::FromUTF8(catalogFailureReason)));
              }
              isDownloadInfoFinished = true;
              downloadUiActive->store(false);
              cancelButton->Disable();
              summaryText->SetLabel(wxString::Format(_("%s - queue finished"),
                                    summaryText->GetLabel()));
              ackButton->Enable();
              downloadInfoDialog.Hide();
            } else {
              wxMessageBox(wxString::FromUTF8(FormatGdtfShareUserMessage(
                               loginResult, "login")),
                           _("GDTF Share login"), wxOK | wxICON_WARNING);
            }
#else
            wxMessageBox(
                "GDTF Share download is unavailable in this build target.",
                         "GDTF Share download", wxOK | wxICON_WARNING);
#endif
            reportProgress("Conflict dialog:hide");
          }

          reportProgress("Applying GDTF conflict selection...");
          const int totalFixturesForConflictApply =
              static_cast<int>(scene.fixtures.size());
          int appliedFixturesForConflictApply = 0;
          for (auto &[uid, f] : scene.fixtures) {
          ++appliedFixturesForConflictApply;
          if (totalFixturesForConflictApply > 0 &&
              (appliedFixturesForConflictApply == 1 ||
                 appliedFixturesForConflictApply ==
                     totalFixturesForConflictApply ||
               appliedFixturesForConflictApply % 50 == 0)) {
            reportProgress("Applying GDTF conflict selection...",
                           appliedFixturesForConflictApply,
                           totalFixturesForConflictApply);
          }
            auto typeKey = f.typeName;
            auto it = choices.find(typeKey);
            if (it != choices.end()) {
            const std::string resolvedGdtfPath =
                resolveFixtureGdtfPathForRead(f.gdtfSpec);
            const int previousChannelCount =
                (!resolvedGdtfPath.empty() && !f.gdtfMode.empty())
                      ? getGdtfModeChannelCountCached(resolvedGdtfPath,
                                                      f.gdtfMode)
                    : -1;
              const auto selectedPathIt = selectedPathByType.find(typeKey);
              if (selectedPathIt == selectedPathByType.end())
                continue;
              f.gdtfSpec = selectedPathIt->second;
            f.gdtfSpec = resources.MakeSceneRelative(
                  PathUtils::PathFromUtf8(
                      resolveFixtureGdtfPathForRead(f.gdtfSpec)));
              const std::string selectedResolvedGdtfPath =
                  resolveFixtureGdtfPathForRead(f.gdtfSpec);
              std::string parsed =
                  resolvedGdtfFileExists(selectedResolvedGdtfPath)
                      ? Trim(GetGdtfFixtureName(selectedResolvedGdtfPath))
                      : std::string{};
            if (!parsed.empty())
              f.typeName = parsed;
            const auto &dictEntry = getDictionaryEntryCached(typeKey);
            if (dictEntry) {
              if (f.gdtfMode.empty())
                f.gdtfMode = dictEntry->mode;
            }
              const auto selectedModeIt = selectedModeByType.find(typeKey);
              if (selectedModeIt != selectedModeByType.end())
                f.gdtfMode = selectedModeIt->second;
              f.gdtfMode = resolveExistingGdtfModeCached(
                  resolveFixtureGdtfPathForRead(f.gdtfSpec), f.gdtfMode,
                  previousChannelCount > 0
                      ? std::optional<int>(previousChannelCount)
                                           : std::nullopt);
            }
          }
        }
      } else {
        const int totalFixturesForDictionaryApply =
            static_cast<int>(scene.fixtures.size());
        int appliedFixturesForDictionaryApply = 0;
        for (auto &[uid, f] : scene.fixtures) {
          ++appliedFixturesForDictionaryApply;
          if (totalFixturesForDictionaryApply > 0 &&
              (appliedFixturesForDictionaryApply == 1 ||
               appliedFixturesForDictionaryApply ==
                   totalFixturesForDictionaryApply ||
               appliedFixturesForDictionaryApply % 50 == 0)) {
            reportProgress("Applying dictionary GDTF mappings...",
                           appliedFixturesForDictionaryApply,
                           totalFixturesForDictionaryApply);
          }
          const auto &dictEntry = getDictionaryEntryCached(f.typeName);
          if (dictEntry && !dictEntry->path.empty()) {
            const std::string dictionaryResolvedPath =
                resolveFixtureGdtfPathForRead(dictEntry->path);
            std::error_code dictionaryPathEc;
            if (dictionaryResolvedPath.empty() ||
                !fs::exists(PathUtils::PathFromUtf8(dictionaryResolvedPath),
                            dictionaryPathEc) ||
                dictionaryPathEc) {
              LogMessage(Logger::Level::Warn,
                         "Skipping dictionary GDTF mapping for fixture '" +
                             f.instanceName +
                             "' because the mapped path is unavailable: " +
                             dictEntry->path);
              continue;
            }
            const std::string resolvedGdtfPath =
                resolveFixtureGdtfPathForRead(f.gdtfSpec);
            const int previousChannelCount =
                (!resolvedGdtfPath.empty() && !f.gdtfMode.empty())
                    ? getGdtfModeChannelCountCached(resolvedGdtfPath,
                                                    f.gdtfMode)
                    : -1;
            f.gdtfSpec = resources.MakeSceneRelative(
                PathUtils::PathFromUtf8(dictionaryResolvedPath));
            if (f.gdtfMode.empty())
              f.gdtfMode = dictEntry->mode;
            f.gdtfMode = resolveExistingGdtfModeCached(
                dictionaryResolvedPath, f.gdtfMode,
                previousChannelCount > 0
                    ? std::optional<int>(previousChannelCount)
                                         : std::nullopt);
            std::string parsed =
                Trim(GetGdtfFixtureName(dictionaryResolvedPath));
            if (!parsed.empty())
              f.typeName = parsed;
          }
        }
      }
    }
  }

  reportProgress("Applying fixture categories...");
  const int totalFixturesForCategoryApply =
      static_cast<int>(scene.fixtures.size());
  int appliedFixturesForCategoryApply = 0;
  int autoFallbackAppliedCount = 0;
  std::unordered_map<std::string, int> autoFallbackReasons;
  std::unordered_map<std::string, int> autoFallbackCategories;
  categoryByTypeKey.clear();
  std::unordered_map<std::string, std::string> pendingCategoryUpdatesByType;
  for (auto &[uid, fixture] : scene.fixtures) {
    (void)uid;
    ++appliedFixturesForCategoryApply;
    if (totalFixturesForCategoryApply > 0 &&
        (appliedFixturesForCategoryApply == 1 ||
         appliedFixturesForCategoryApply == totalFixturesForCategoryApply ||
         appliedFixturesForCategoryApply % 10 == 0)) {
      reportProgress("Applying fixture categories...",
                     appliedFixturesForCategoryApply,
                     totalFixturesForCategoryApply);
    }

    const std::string categoryKey =
        !fixture.typeName.empty() ? fixture.typeName : fixture.gdtfSpec;

    if (fixture.category.empty() && !fixture.typeName.empty()) {
      const auto &dictionaryEntry = getDictionaryEntryCached(fixture.typeName);
      if (dictionaryEntry) {
        fixture.category =
            GdtfFixtureCategory::NormalizeCategory(dictionaryEntry->category);
      }
      if (!fixture.category.empty()) {
        fixture.categorySource = GdtfFixtureCategory::kManualSource;
        fixture.categorySourceReason.clear();
      }
    }

    if (fixture.category.empty() && !categoryKey.empty()) {
      auto cacheIt = categoryByTypeKey.find(categoryKey);
      if (cacheIt != categoryByTypeKey.end()) {
        fixture.category = cacheIt->second.category;
        fixture.categorySource = cacheIt->second.source;
        fixture.categorySourceReason = cacheIt->second.reason;
      }
    }

    if (fixture.category.empty() && !fixture.gdtfSpec.empty()) {
      std::string resolvedCategoryPath =
          resolveFixtureGdtfPathForRead(fixture.gdtfSpec);
      if (!resolvedCategoryPath.empty()) {
        resolvedCategoryPath = resources.ResolveGdtfPath(resolvedCategoryPath);
      } else {
        resolvedCategoryPath = resources.ResolveGdtfPath(fixture.gdtfSpec);
      }

      GdtfFixtureCategory::InferenceResult inferred;
      if (!resolvedCategoryPath.empty() &&
          resolvedGdtfFileExists(resolvedCategoryPath)) {
        auto inferenceCacheIt =
            categoryInferenceByResolvedPath.find(resolvedCategoryPath);
        if (inferenceCacheIt != categoryInferenceByResolvedPath.end()) {
          inferred = inferenceCacheIt->second;
        } else {
          inferred = GdtfFixtureCategory::InferFromGdtf(resolvedCategoryPath);
          categoryInferenceByResolvedPath.emplace(resolvedCategoryPath,
                                                  inferred);
        }
      } else {
        inferred.reason = "GDTF file is missing";
      }

      fixture.category =
          GdtfFixtureCategory::NormalizeCategory(inferred.category);
      if (fixture.category.empty())
        fixture.category = GdtfFixtureCategory::kUnknown;
      fixture.categorySource = GdtfFixtureCategory::kAutoFallbackSource;
      fixture.categorySourceReason = inferred.reason;
      ++autoFallbackAppliedCount;
      ++autoFallbackReasons[inferred.reason.empty() ? "unknown"
                                                    : inferred.reason];
      ++autoFallbackCategories[fixture.category];
      if (!categoryKey.empty()) {
        categoryByTypeKey[categoryKey] = {
            fixture.category, fixture.categorySource, inferred.reason};
      }
      LogMessage(Logger::Level::Debug,
                 "Auto category fallback: " + fixture.instanceName + " -> " +
                     fixture.category + " [" + inferred.reason + "]");
    } else if (!fixture.category.empty() && !categoryKey.empty()) {
      categoryByTypeKey[categoryKey] = {
          fixture.category,
          fixture.categorySource.empty() ? GdtfFixtureCategory::kManualSource
                                         : fixture.categorySource,
          fixture.categorySourceReason.empty() ? "cached"
                                               : fixture.categorySourceReason};
    }

    if (!fixture.category.empty() && !fixture.typeName.empty() &&
        fixture.categorySource == GdtfFixtureCategory::kManualSource) {
      pendingCategoryUpdatesByType[fixture.typeName] = fixture.category;
    }
  }
  auto formatBreakdown =
      [](const std::unordered_map<std::string, int> &counts) {
    if (counts.empty())
      return std::string("none");
        std::vector<std::pair<std::string, int>> sortedCounts(counts.begin(),
                                                              counts.end());
    std::sort(sortedCounts.begin(), sortedCounts.end(),
              [](const auto &lhs, const auto &rhs) {
                if (lhs.second != rhs.second)
                  return lhs.second > rhs.second;
                return lhs.first < rhs.first;
              });

    std::ostringstream oss;
    bool first = true;
    for (const auto &[label, count] : sortedCounts) {
      if (!first)
        oss << ", ";
      first = false;
      oss << label << "=" << count;
    }
    return oss.str();
  };
  LogMessage(Logger::Level::Info,
             "Auto category fallback applied to " +
                 std::to_string(autoFallbackAppliedCount) +
                 " fixtures; reasons: " + formatBreakdown(autoFallbackReasons) +
                 "; categories: " + formatBreakdown(autoFallbackCategories));
  GdtfDictionary::UpdateCategoriesBulk(pendingCategoryUpdatesByType);

  reportProgress("Building fixtures, trusses, and objects...");

  const int totalFixturesForModeResolve =
      static_cast<int>(scene.fixtures.size());
  int resolvedFixturesForModeResolve = 0;
  for (auto &[uid, fixture] : scene.fixtures) {
    ++resolvedFixturesForModeResolve;
    if (totalFixturesForModeResolve > 0 &&
        (resolvedFixturesForModeResolve == 1 ||
         resolvedFixturesForModeResolve == totalFixturesForModeResolve ||
         resolvedFixturesForModeResolve % 50 == 0)) {
      reportProgress("Resolving GDTF modes...", resolvedFixturesForModeResolve,
                     totalFixturesForModeResolve);
    }
    if (fixture.gdtfSpec.empty())
      continue;
    const std::string resolvedGdtfPath =
        resolveFixtureGdtfPathForRead(fixture.gdtfSpec);
    const int currentChannelCount =
        (!fixture.gdtfMode.empty() && !resolvedGdtfPath.empty())
            ? getGdtfModeChannelCountCached(resolvedGdtfPath, fixture.gdtfMode)
            : -1;
    fixture.gdtfMode = resolveExistingGdtfModeCached(
        resolvedGdtfPath, fixture.gdtfMode,
        currentChannelCount > 0 ? std::optional<int>(currentChannelCount)
                                : std::nullopt);
  }

  bool hasDefaultLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME) {
      hasDefaultLayer = true;
      break;
    }
  }
  if (!hasDefaultLayer) {
    Layer l;
    l.uuid = "layer_default";
    l.name = DEFAULT_LAYER_NAME;
    scene.layers[l.uuid] = l;
  }

  if (matrixScaleAggregation.acceptedTinyUniformScaleCount > 0) {
    std::ostringstream oss;
    oss << "MVR import matrix summary: accepted "
        << matrixScaleAggregation.acceptedTinyUniformScaleCount
        << " tiny uniform geometry scales without warning. Contexts: "
        << JoinMatrixContextCounts(matrixScaleAggregation.acceptedByContext);
    LogMessage(Logger::Level::Info, oss.str());
  }

  if (matrixScaleAggregation.suspiciousMatrixCount > 0) {
    std::ostringstream oss;
    oss << "MVR import matrix anomalies: "
        << matrixScaleAggregation.suspiciousMatrixCount
        << " suspicious matrices detected. Contexts: "
        << JoinMatrixContextCounts(matrixScaleAggregation.suspiciousByContext);
    LogMessage(Logger::Level::Warn, oss.str());

    for (const std::string &example : matrixScaleAggregation.suspiciousExamples)
      LogMessage(Logger::Level::Warn,
                 "MVR import suspicious matrix example: " + example);
  }

  if (trussSymbolSymdefPreservedCount > 0) {
    std::vector<std::pair<std::string, int>> sortedSymdefCounts(
        trussSymbolSymdefPreservedBySymdef.begin(),
        trussSymbolSymdefPreservedBySymdef.end());
    std::sort(sortedSymdefCounts.begin(), sortedSymdefCounts.end(),
              [](const auto &lhs, const auto &rhs) {
                if (lhs.second != rhs.second)
                  return lhs.second > rhs.second;
                return lhs.first < rhs.first;
              });

    std::ostringstream oss;
    oss << "MVR import truss Symbol/Symdef representation preserved for "
        << trussSymbolSymdefPreservedCount << " trusses";
    if (!sortedSymdefCounts.empty()) {
      oss << ". Symdef counts: ";
      for (size_t i = 0; i < sortedSymdefCounts.size(); ++i) {
        if (i > 0)
          oss << ", ";
        oss << "'" << sortedSymdefCounts[i].first
            << "'=" << sortedSymdefCounts[i].second;
      }
    }
    LogMessage(Logger::Level::Info, oss.str());
  }

  int fixturesWithGdtfSpec = 0;
  int fixturesWithResolvedGdtf = 0;
  std::vector<std::string> unresolvedGdtfExamples;
  for (const auto &[uid, fixture] : scene.fixtures) {
    (void)uid;
    const std::string preservedSpec = fixture.originalMvrGdtfSpec.empty()
                                          ? fixture.gdtfSpec
                                          : fixture.originalMvrGdtfSpec;
    if (preservedSpec.empty())
      continue;
    ++fixturesWithGdtfSpec;
    const std::string resolvedGdtf =
        resolveFixtureGdtfPathForRead(preservedSpec);
    std::error_code resolvedEc;
    if (!resolvedGdtf.empty() &&
        fs::exists(PathUtils::PathFromUtf8(resolvedGdtf), resolvedEc) &&
        !resolvedEc) {
      ++fixturesWithResolvedGdtf;
    } else if (unresolvedGdtfExamples.size() < 5) {
      unresolvedGdtfExamples.push_back(preservedSpec);
    }
  }

  std::ostringstream importDiagnostics;
  importDiagnostics << "MVR import GDTF diagnostics: basePath='"
                    << scene.basePath
                    << "', fixturesWithGdtfSpec=" << fixturesWithGdtfSpec
                    << ", resolvedFixtureGdtfs=" << fixturesWithResolvedGdtf
                    << ", unresolvedFixtureGdtfs="
                    << (fixturesWithGdtfSpec - fixturesWithResolvedGdtf)
                    << ", dictionaryMapping="
                    << (options.applyDictionary ? "enabled" : "disabled");
  if (!unresolvedGdtfExamples.empty()) {
    importDiagnostics << ", unresolvedExamples=";
    for (size_t i = 0; i < unresolvedGdtfExamples.size(); ++i) {
      if (i > 0)
        importDiagnostics << "; ";
      importDiagnostics << unresolvedGdtfExamples[i];
    }
  }
  LogMessage(Logger::Level::Info, importDiagnostics.str());

  auto metadataUuids = [](const auto &entries) {
    std::unordered_set<std::string> uuids;
    for (const auto &[uuid, value] : entries) {
      (void)value;
      uuids.insert(uuid);
    }
    return uuids;
  };
  const auto trussInfoUuids = metadataUuids(rootTrussInfoByUuid);
  const auto hoistInfoUuids = metadataUuids(rootHoistInfoByUuid);
  const auto projectFixtureMetadataUuids =
      metadataUuids(projectFixtureColorsByUuid);
  referenceResolver.Reconcile(
      importResult,
      {trussInfoUuids, consumedRootTrussInfoUuids, hoistInfoUuids,
       consumedRootHoistInfoUuids, projectFixtureMetadataUuids,
       consumedProjectFixtureColorUuids});

  std::string summary =
      "Parsed scene: " + std::to_string(scene.fixtures.size()) + " fixtures, " +
      std::to_string(scene.trusses.size()) + " trusses, " +
      std::to_string(scene.supports.size()) + " supports, " +
      std::to_string(scene.sceneObjects.size()) + " objects";
  LogMessage(summary);
  return true;
}

// Imports an MVR file and migrates labels after replacing the active project
// scene.
bool MvrImporter::ImportAndRegister(const std::string &filePath,
                                    bool promptConflicts, bool applyDictionary,
                                    ProgressCallback progressCallback) {
  MvrImportOptions options;
  options.promptConflicts = promptConflicts;
  options.applyDictionary = applyDictionary;
  return ImportAndRegister(filePath, options, progressCallback);
}

// Imports and registers in-memory MVR bytes with explicit project options.
bool MvrImporter::ImportAndRegisterFromBuffer(
    const std::vector<std::uint8_t> &bytes, const MvrImportOptions &options,
    ProgressCallback progressCallback) {
  MvrImporter importer;
  MvrImportResult importResult;
  const bool imported = importer.ImportFromBuffer(bytes, importResult,
                                                  MvrImportMode::ReplaceProject,
                                                  options, progressCallback);
  LogMvrImportDiagnostics(importResult.diagnostics);
  if (!imported)
    return false;
  size_t collisionCount = 0;
  viewer2d::RemapFixtureLabelOverrideKeys(
      ConfigManager::Get(), importResult.fixtureUuidRemap, &collisionCount);
  return true;
}

// Imports and registers an MVR file with explicit import behavior options.
bool MvrImporter::ImportAndRegister(const std::string &filePath,
                                    const MvrImportOptions &options,
                                    ProgressCallback progressCallback) {
  MvrImporter importer;
  MvrImportResult importResult;
  const bool imported = importer.ImportFromFile(filePath, importResult,
                                                MvrImportMode::ReplaceProject,
                                                options, progressCallback);
  LogMvrImportDiagnostics(importResult.diagnostics);
  if (!imported)
    return false;

  size_t collisionCount = 0;
  const size_t migratedCount = viewer2d::RemapFixtureLabelOverrideKeys(
      ConfigManager::Get(), importer.fixtureUuidRemap, &collisionCount);
  if (!importer.fixtureUuidRemap.empty()) {
    std::ostringstream oss;
    oss << "MVR import fixture label override migration: remapped "
        << migratedCount << " fixture override entries from "
        << importer.fixtureUuidRemap.size() << " fixture UUID changes";
    if (collisionCount > 0)
      oss << " (" << collisionCount << " collisions skipped)";
    LogMessage(Logger::Level::Info, oss.str());
  }
  return true;
}
