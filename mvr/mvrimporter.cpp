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
#include "layer_identity.h"
#include "mvrimporter.h"
#include "apppaths.h"
#include "build_info.h"
#include "configmanager.h"
#include "filesystem_path_utils.h"
#include "mvr_import_application_read_services.h"
#include "mvr_import_package.h"
#include "mvr_import_project_application.h"
#include "mvr_import_resource_resolver.h"
#include "mvr_read_service.h"
#include "mvr_scene_node_reader.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "credentialstore.h"
#endif
#include "dummyprofilelibrary.h"
#include "gdtfdictionary.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "gdtfnet.h"
#endif
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
#include "gdtf_download_queue_dialog.h"
#include "mvr_preferences.h"
#endif
#include "json.hpp"
#include "logger.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream> // Required for std::ofstream
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
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

#include <wx/filename.h>
#include <wx/intl.h>
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

// Applies application conflict choices after the shared structural parse.
static void ApplyApplicationDictionaryMappings(
    MvrImportResult &result, mvr::MvrImportResourceResolver &resources,
    const mvr::MvrReadContext &readContext, const MvrImportOptions &options,
    MvrImporter::ProgressCallback progressCallback) {
  MvrScene &scene = result.scene;
  std::unordered_map<std::string, GdtfConflict> pendingGdtfConflictByType;
  for (const GdtfConflict &conflict : readContext.gdtfConflicts)
    pendingGdtfConflictByType[conflict.type] = conflict;
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (progressCallback)
      progressCallback({std::move(stage), completed, total});
  };
  auto resolveFixtureGdtfPathForRead = [&](const std::string &spec) {
    return resources.ResolveGdtfPath(spec);
  };
  auto getFixtureMetadata =
      [&](const std::string &path) -> const mvr::ImportGdtfMetadata & {
    return resources.FixtureMetadata(path);
  };
  auto getGdtfModeChannelCountCached = [&](const std::string &path,
                                           const std::string &mode) {
    return resources.GdtfModeChannelCount(path, mode);
  };
  auto getDictionaryEntryCached = [&](const std::string &type)
      -> const std::optional<GdtfDictionary::Entry> & {
    return resources.DictionaryEntry(type);
  };
  auto resolveExistingGdtfModeCached = [&](const std::string &path,
                                           const std::string &mode,
                                           std::optional<int> count) {
    return resources.ResolveGdtfMode(path, mode, count);
  };
  auto resolvedGdtfFileExists = [&](const std::string &path) {
    return resources.GdtfFileExists(path);
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
              const std::string initialUser =
                  activeCredentials
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
            if (!loginOk &&
                (!activeCredentials ||
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
                    reportProgress(
                        "[WARN] GDTF Share credentials authenticated but the "
                        "password was not persisted: " +
                        CredentialStore::StatusName(saveResult.status));
                    wxMessageBox(
                        saveResult.status ==
                                CredentialStore::Status::SecureStoreUnavailable
                            ? _("The username was saved, but secure password "
                                "storage is unavailable. The password must be "
                                "entered again after restart.")
                            : wxString::Format(
                                  _("GDTF Share credentials were authenticated "
                                    "for this operation, but were not saved "
                                    "(%s)."),
                                  wxString::FromUTF8(
                                      CredentialStore::StatusName(
                                          saveResult.status))),
                        _("GDTF Share credentials"), wxOK | wxICON_WARNING);
                  }
                }
              }
            }

            if (loginOk) {
              wxWindow *dialogParent =
                  wxTheApp ? wxDynamicCast(wxTheApp->GetTopWindow(), wxWindow)
                           : nullptr;
              GdtfDownloadQueueDialog queue(
                  dialogParent, mvr::preferences::AutoCloseGdtfDownloadWindow(
                                    ConfigManager::Get()));
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

              auto formatBytes = GdtfDownloadQueueDialog::FormatBytes;
              auto updateStatusRow =
                  [&](const std::string &type, const wxString &selected,
                      const wxString &status, const wxString &progress,
                      const wxString &details, DownloadRowState state) {
                    queue.UpdateStatusRow(type, selected, status, progress,
                                          details, state);
                  };
              auto updateProgressGauge = [&]() { queue.UpdateProgressGauge(); };
              auto runOnUiThread = [&](std::function<void()> task) {
                if (wxThread::IsMain() && !queue.IsFinished())
                  task();
              };
              queue.Run([&]() {
                for (const GdtfConflict &req : downloadRequests)
                  queue.AddRow(req.type);
                queue.SetPhase(_("Loading GDTF catalog..."));
                wxYieldIfNeeded();

                std::string listPayload;
                GdtfShareResult listResult;
                reportProgress(
                    "Downloading selected GDTFs: loading catalog...");

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
                reportProgress(
                    wxString::Format(
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
                          "Catalog fetch/parsing failed (%s, HTTP %ld, "
                          "bytes=%zu)",
                          wxString::FromUTF8(
                              GdtfShareResultCategoryName(listResult.category)),
                          listResult.httpStatus, listPayload.size())
                          .ToStdString();
                  reportProgress("[WARN] " + catalogFailureReason);
                }

                if (hasDownloadableCatalogEntry()) {
                  // Reuses one authoritative download when distinct import
                  // aliases explicitly resolve to the same GDTF Share revision.
                  std::unordered_map<std::string, std::string>
                      downloadedPathByReplacement;
                  std::unordered_map<std::string, std::string>
                      downloadedModeByReplacement;
                  queue.SetSummary(
                      wxString::Format(_("Selected fixture types for download "
                                         "(catalog entries: %zu)"),
                                       catalogEntries.size()));
                  queue.SetProgress(25);
                  queue.SetPhase(_("Downloading selected fixtures..."));
                  for (GdtfConflict req : downloadRequests) {
                    if (queue.IsCancellationRequested()) {
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
                    matchEvidence.requestedFixtureName =
                        req.requestedFixtureName;
                    matchEvidence.manufacturer = req.manufacturer;
                    matchEvidence.fixtureTypeId = req.fixtureTypeId;
                    matchEvidence.modeName = req.modeName;
                    matchEvidence.footprint = req.footprint;
                    const auto matchRequest =
                        mvr::gdtf_import_matching::BuildDownloadRequest(
                            matchEvidence);
                    auto diagnosticMatchRequest = matchRequest;
                    diagnosticMatchRequest.catalogSnapshotSource =
                        effectiveCatalogSource ==
                                GdtfCatalogResultSource::Online
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
                          queue.Progress(req.type).totalBytes > 0
                              ? formatBytes(
                                    queue.Progress(req.type).downloadedBytes) +
                                    " / " +
                                    formatBytes(
                                        queue.Progress(req.type).totalBytes)
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
                      wxString bytesText = formatBytes(stats.downloadedBytes) +
                                           " / " + totalText;
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
                      downloadResult.category =
                          GdtfShareResultCategory::Success;
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
                            runOnUiThread([=, &queue, &updateStatusRow,
                                           &formatRowProgress,
                                           &updateProgressGauge]() {
                              auto &stats = queue.Progress(typeKey);
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
                          [&]() { return queue.IsCancellationRequested(); });
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
                        details += wxString::Format(
                            _(" (Mode: %s)"),
                            wxString::FromUTF8(bestMatch.modeName));
                      if (!bestMatch.selectionReason.empty())
                        details +=
                            " [" +
                            wxString::FromUTF8(bestMatch.selectionReason) + "]";
                      auto &stats = queue.Progress(req.type);
                      if (stats.totalBytes > 0) {
                        stats.downloadedBytes = stats.totalBytes;
                      }
                      updateStatusRow(
                          req.type, selectedFixtureName, _("Success"),
                          formatRowProgress(queue.Progress(req.type), 100.0),
                          details, DownloadRowState::Downloaded);
                    } else if (queue.IsCancellationRequested()) {
                      const wxString totalText =
                          queue.Progress(req.type).totalBytes > 0
                              ? formatBytes(queue.Progress(req.type).totalBytes)
                              : wxString("? B");
                      updateStatusRow(
                          req.type, selectedFixtureName, _("Canceled"),
                          formatBytes(
                              queue.Progress(req.type).downloadedBytes) +
                              " / " + totalText,
                          _("Canceled by user"), DownloadRowState::Canceled);
                      break;
                    } else {
                      const wxString totalText =
                          queue.Progress(req.type).totalBytes > 0
                              ? formatBytes(queue.Progress(req.type).totalBytes)
                              : wxString("? B");
                      updateStatusRow(
                          req.type, selectedFixtureName,
                          fallbackStatusText(req),
                          formatBytes(
                              queue.Progress(req.type).downloadedBytes) +
                              " / " + totalText,
                          _("Download failed"), DownloadRowState::Fallback);
                    }
                    updateProgressGauge();
                    wxYieldIfNeeded();
                  }
                  if (queue.IsCancellationRequested()) {
                    for (const GdtfConflict &req : downloadRequests) {
                      if (queue.State(req.type) == DownloadRowState::Pending) {
                        updateStatusRow(req.type, "-", _("Canceled"),
                                        "0 B / ? B",
                                        _("Canceled before download start"),
                                        DownloadRowState::Canceled);
                      }
                    }
                    queue.SetPhase(
                        _("Queue canceled. Keeping downloaded fixtures."));
                  } else {
                    queue.SetPhase(_("Queue finished."));
                  }
                } else {
                  if (catalogFailureReason.empty())
                    catalogFailureReason = std::string(
                        _("Catalog fetch/parsing failed.").ToUTF8());
                  queue.SetSummary(wxString::Format(
                      _("Selected fixture types for download "
                        "(catalog load failed: %s)"),
                      wxString::FromUTF8(catalogFailureReason)));
                  for (const GdtfConflict &req : downloadRequests) {
                    updateStatusRow(req.type, "-", fallbackStatusText(req),
                                    "0 B / ? B",
                                    _("Failed to load catalog list"),
                                    DownloadRowState::Fallback);
                  }
                  updateProgressGauge();
                  queue.SetPhase(wxString::Format(
                      _("Catalog load failed. %s. Keeping available "
                        "fallbacks."),
                      wxString::FromUTF8(catalogFailureReason)));
                }
              });
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
              f.gdtfSpec = resources.MakeSceneRelative(PathUtils::PathFromUtf8(
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
}

// Logs application-specific GDTF resolution results after mapping policy.
static void
LogApplicationGdtfResolutionSummary(const MvrImportResult &result,
                                    mvr::MvrImportResourceResolver &resources,
                                    const MvrImportOptions &options) {
  int fixturesWithGdtfSpec = 0;
  int resolvedFixtureGdtfs = 0;
  std::vector<std::string> unresolvedExamples;
  for (const auto &[uuid, fixture] : result.scene.fixtures) {
    (void)uuid;
    const std::string spec = fixture.originalMvrGdtfSpec.empty()
                                 ? fixture.gdtfSpec
                                 : fixture.originalMvrGdtfSpec;
    if (spec.empty())
      continue;
    ++fixturesWithGdtfSpec;
    const std::string resolved = resources.ResolveGdtfPath(spec);
    if (!resolved.empty() && resources.GdtfFileExists(resolved)) {
      ++resolvedFixtureGdtfs;
    } else if (unresolvedExamples.size() < 5) {
      unresolvedExamples.push_back(spec);
    }
  }
  std::ostringstream summary;
  summary << "MVR import GDTF diagnostics: basePath='" << result.scene.basePath
          << "', fixturesWithGdtfSpec=" << fixturesWithGdtfSpec
          << ", resolvedFixtureGdtfs=" << resolvedFixtureGdtfs
          << ", unresolvedFixtureGdtfs="
          << (fixturesWithGdtfSpec - resolvedFixtureGdtfs)
          << ", dictionaryMapping="
          << (options.applyDictionary ? "enabled" : "disabled");
  if (!unresolvedExamples.empty()) {
    summary << ", unresolvedExamples=";
    for (std::size_t index = 0; index < unresolvedExamples.size(); ++index) {
      if (index > 0)
        summary << "; ";
      summary << unresolvedExamples[index];
    }
  }
  LogMessage(Logger::Level::Info, summary.str());
}

// Applies application-only category, mode, and default-layer enrichment.
static void
ApplyApplicationSceneEnrichment(MvrImportResult &result,
                                mvr::MvrImportResourceResolver &resources,
                                MvrImporter::ProgressCallback progress) {
  std::unordered_map<std::string, mvr::SceneReadCachedCategory> byType;
  std::unordered_map<std::string, GdtfFixtureCategory::InferenceResult>
      byResolvedPath;
  std::unordered_map<std::string, size_t> fallbackReasons;
  std::unordered_map<std::string, size_t> fallbackCategories;
  int fallbackCount = 0;
  int completed = 0;
  const int total = static_cast<int>(result.scene.fixtures.size());
  if (progress)
    progress({"Applying fixture categories...", 0, total});
  for (auto &[uuid, fixture] : result.scene.fixtures) {
    (void)uuid;
    const std::string key =
        !fixture.typeName.empty() ? fixture.typeName : fixture.gdtfSpec;
    if (fixture.category.empty() && !fixture.typeName.empty()) {
      const auto &entry = resources.DictionaryEntry(fixture.typeName);
      if (entry)
        fixture.category =
            GdtfFixtureCategory::NormalizeCategory(entry->category);
      if (!fixture.category.empty()) {
        fixture.categorySource = GdtfFixtureCategory::kManualSource;
        fixture.categorySourceReason.clear();
      }
    }
    if (fixture.category.empty() && !key.empty() && byType.contains(key)) {
      const auto &cached = byType.at(key);
      fixture.category = cached.category;
      fixture.categorySource = cached.source;
      fixture.categorySourceReason = cached.reason;
    }
    if (fixture.category.empty() && !fixture.gdtfSpec.empty()) {
      const std::string resolved = resources.ResolveGdtfPath(fixture.gdtfSpec);
      auto [it, inserted] = byResolvedPath.try_emplace(resolved);
      if (inserted) {
        if (!resolved.empty() && resources.GdtfFileExists(resolved))
          it->second = GdtfFixtureCategory::InferFromGdtf(resolved);
        else
          it->second.reason = "GDTF file is missing";
      }
      fixture.category =
          GdtfFixtureCategory::NormalizeCategory(it->second.category);
      if (fixture.category.empty())
        fixture.category = GdtfFixtureCategory::kUnknown;
      fixture.categorySource = GdtfFixtureCategory::kAutoFallbackSource;
      fixture.categorySourceReason = it->second.reason;
      ++fallbackCount;
      ++fallbackReasons[it->second.reason.empty() ? "unknown"
                                                  : it->second.reason];
      ++fallbackCategories[fixture.category];
      LogMessage(Logger::Level::Debug,
                 "Auto category fallback: " + fixture.instanceName + " -> " +
                     fixture.category + " [" + it->second.reason + "]");
    }
    if (!fixture.category.empty() && !key.empty())
      byType[key] = {
          fixture.category,
          fixture.categorySource.empty() ? GdtfFixtureCategory::kManualSource
                                         : fixture.categorySource,
          fixture.categorySourceReason.empty() ? "cached"
                                               : fixture.categorySourceReason};
    ++completed;
    if (progress && (completed == total || completed % 10 == 0))
      progress({"Applying fixture categories...", completed, total});
  }
  LogMessage(
      Logger::Level::Info,
      "Auto category fallback applied to " + std::to_string(fallbackCount) +
          " fixtures; reasons: " + JoinMatrixContextCounts(fallbackReasons) +
          "; categories: " + JoinMatrixContextCounts(fallbackCategories));
  if (progress)
    progress({"Building fixtures, trusses, and objects...", 0, 0});
  completed = 0;
  if (progress)
    progress({"Resolving GDTF modes...", 0, total});
  for (auto &[uuid, fixture] : result.scene.fixtures) {
    (void)uuid;
    if (!fixture.gdtfSpec.empty()) {
      const std::string resolved = resources.ResolveGdtfPath(fixture.gdtfSpec);
      const int channelCount =
          (!resolved.empty() && !fixture.gdtfMode.empty())
              ? resources.GdtfModeChannelCount(resolved, fixture.gdtfMode)
              : -1;
      fixture.gdtfMode = resources.ResolveGdtfMode(
          resolved, fixture.gdtfMode,
          channelCount > 0 ? std::optional<int>(channelCount) : std::nullopt);
    }
    ++completed;
    if (progress && (completed == total || completed % 50 == 0))
      progress({"Resolving GDTF modes...", completed, total});
  }
  layerdomain::EnsureDefaultLayer(result.scene);
  layerdomain::MigrateLayerMembership(result.scene);

}

// Persists manually authored fixture categories after an application import.
static void PersistManualFixtureCategories(const MvrImportResult &result) {
  std::unordered_map<std::string, std::string> updates;
  for (const auto &[uuid, fixture] : result.scene.fixtures) {
    (void)uuid;
    if (!fixture.typeName.empty() && !fixture.category.empty() &&
        fixture.categorySource == GdtfFixtureCategory::kManualSource)
      updates[fixture.typeName] = fixture.category;
  }
  GdtfDictionary::UpdateCategoriesBulk(updates);
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
  const bool imported = ImportFromFileIntoResult(filePath, importResult,
                                                 options, progressCallback);
  if (imported && mode == MvrImportMode::ReplaceProject) {
    mvr::MvrImportProjectApplication application(ConfigManager::Get());
    application.Apply(importResult);
  }
  return imported;
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
  return true;
}

// Extracts an MVR package and parses its scene data into an import result
// payload.
bool MvrImporter::ImportFromFileIntoResult(const std::string &filePath,
                                           MvrImportResult &importResult,
                                           const MvrImportOptions &options,
                                           ProgressCallback progressCallback) {
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (!progressCallback)
      return;
    progressCallback(ProgressState{std::move(stage), completed, total});
  };

  pathRemap.clear();
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
  return ImportFromStreamIntoResult(input, importResult, options,
                                    progressCallback);
}

// Imports an MVR byte payload without a project-level temporary archive.
bool MvrImporter::ImportFromBuffer(const std::vector<std::uint8_t> &bytes,
                                   MvrImportResult &importResult,
                                   MvrImportMode mode,
                                   const MvrImportOptions &options,
                                   ProgressCallback progressCallback) {
  if (bytes.empty())
    return false;
  wxMemoryInputStream input(bytes.data(), bytes.size());
  const bool imported = ImportFromStreamIntoResult(input, importResult, options,
                                                   progressCallback);
  if (imported && mode == MvrImportMode::ReplaceProject) {
    mvr::MvrImportProjectApplication application(ConfigManager::Get());
    application.Apply(importResult);
  }
  return imported;
}

// Extracts and parses an MVR archive from an already-open stream.
bool MvrImporter::ImportFromStreamIntoResult(
    wxInputStream &input, MvrImportResult &importResult,
    const MvrImportOptions &options, ProgressCallback progressCallback) {
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (progressCallback)
      progressCallback(ProgressState{std::move(stage), completed, total});
  };
  pathRemap.clear();
  importResult = MvrImportResult{};
  reportProgress("Extracting package resources...");

  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(input, importResult.diagnostics);
  if (!package)
    return false;

  pathRemap = package->pathRemap;
  reportProgress("Parsing scene data...");
  mvr::MvrImportResourceResolver resources(
      package->rootPath,
      [&](const std::string &path) { return RemapArchivePathIfNeeded(path); });
  mvr::MvrReadEnvironment environment =
      mvr::MakeApplicationReadEnvironment(resources);
  mvr::MvrReadContext readContext;
  auto readProgress = [&](std::string stage, int completed, int total) {
    reportProgress(std::move(stage), completed, total);
  };
  const bool parsed = mvr::ReadAcquiredMvrPackage(
      *package, importResult, options, &environment, &readContext, readProgress,
      [](mvr::MvrReadLogLevel level, const std::string &message) {
        const Logger::Level mapped =
            level == mvr::MvrReadLogLevel::Error     ? Logger::Level::Error
            : level == mvr::MvrReadLogLevel::Warning ? Logger::Level::Warn
            : level == mvr::MvrReadLogLevel::Debug   ? Logger::Level::Debug
                                                     : Logger::Level::Info;
        LogMessage(mapped, message);
      });
  if (!parsed)
    return false;
  ApplyApplicationDictionaryMappings(importResult, resources, readContext,
                                     options, progressCallback);
  ApplyApplicationSceneEnrichment(importResult, resources, progressCallback);
  LogApplicationGdtfResolutionSummary(importResult, resources, options);
  PersistManualFixtureCategories(importResult);

  importResult.scene.runtimeResourceLeases.push_back(
      package->workspace.TransferToSceneLease());

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

  return true;
}
