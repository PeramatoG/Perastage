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
#include "apppaths.h"
#include "configmanager.h"
#include "filesystem_path_utils.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "credentialstore.h"
#endif
#include "dummyprofilelibrary.h"
#include "gdtfdictionary.h"
#ifdef PERASTAGE_ENABLE_MVR_GDTF_DOWNLOAD_API
#include "gdtfnet.h"
#endif
#include "fixture_label_overrides.h"
#include "gdtf_catalog_matcher.h"
#include "gdtf_catalog_service.h"
#include "gdtf_fixture_category.h"
#include "gdtf_import_matching.h"
#include "gdtfloader.h"
#include "layer_service.h"
#include "utf8_utils.h"
#include "groupobject.h"
#include "matrixutils.h"
#include "primitive_model_resources.h"
#include "projectutils.h"
#include "scene_grouping.h"
#include "sceneobject.h"
#include "support.h"
#include "trussloader.h"
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
#include <wx/intl.h>
#include <wx/listctrl.h>
#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/zipstrm.h>

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

static std::string DecodeLegacyCredentialValue(const std::string &encoded) {
  constexpr unsigned char kKey = 0x5A;
  std::string out;
  out.reserve(encoded.size() / 2);
  for (size_t i = 0; i + 1 < encoded.size(); i += 2) {
    unsigned int value = 0;
    std::istringstream iss(encoded.substr(i, 2));
    iss >> std::hex >> value;
    out.push_back(
        static_cast<char>((static_cast<unsigned char>(value)) ^ kKey));
  }
  return out;
}

static std::optional<std::pair<std::string, std::string>>
LoadStoredGdtfShareCredentials() {
  const fs::path credPath =
      AppPaths::GetUserDataDir() / "gdtf_credentials.json";
  std::ifstream in(credPath);
  if (!in.is_open())
    return std::nullopt;

  nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
  if (j.is_discarded())
    return std::nullopt;

  const std::string username = j.value("username", "");
  const std::string encodedPassword = j.value("password", "");
  if (username.empty())
    return std::nullopt;
  return std::make_pair(username, DecodeLegacyCredentialValue(encodedPassword));
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

static std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

static std::string ResolveGdtfPath(const std::string &baseDir,
                                   const std::string &spec);

static std::string NormalizeArchivePathValue(const std::string &archivePath) {
  std::string normalized = Trim(NormalizeSlashes(archivePath));
  // Be permissive with vendor MVRs that include an extra blank right before
  // ".gdtf" (e.g. "Fixture Name .gdtf").
  const std::string lowered = ToLowerAscii(normalized);
  const size_t gdtfPos = lowered.rfind(".gdtf");
  if (gdtfPos != std::string::npos && gdtfPos > 0) {
    size_t trimPos = gdtfPos;
    while (trimPos > 0 && normalized[trimPos - 1] == ' ')
      --trimPos;
    if (trimPos != gdtfPos)
      normalized.erase(trimPos, gdtfPos - trimPos);
  }
#ifdef _WIN32
  normalized = ToLowerAscii(normalized);
#endif
  return normalized;
}

static std::string NormalizeGdtfLookupKey(const std::string &value) {
  std::string normalized = NormalizeArchivePathValue(value);
  if (normalized.empty())
    return normalized;

  fs::path path = PathUtils::PathFromUtf8(normalized);
  std::string stem = Trim(path.stem().string());
  std::string ext = ToLowerAscii(path.extension().string());
  if (ext.empty())
    ext = ".gdtf";
  return ToLowerAscii(stem + ext);
}

// Normalizes fixture names for filename-based GDTF identity matching.
static std::string NormalizeFixtureNameLookupKey(std::string value) {
  value = ToLowerAscii(Trim(value));
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char ch) {
                               return std::isspace(ch) != 0 || ch == '_' ||
                                      ch == '-';
                             }),
              value.end());
  return value;
}

// Extracts the fixture-name segment from a Perastage canonical GDTF filename.
static std::string ExtractPerastageFixtureNameFromFileName(
    const fs::path &path) {
  const std::string stem = path.stem().string();
  const size_t firstAt = stem.find('@');
  if (firstAt == std::string::npos)
    return {};
  const size_t secondAt = stem.find('@', firstAt + 1);
  if (secondAt == std::string::npos)
    return {};
  if (NormalizeFixtureNameLookupKey(stem.substr(secondAt + 1)) != "perastage")
    return {};
  return stem.substr(firstAt + 1, secondAt - firstAt - 1);
}

static std::string ExtractDigitSignature(const std::string &text) {
  std::string digits;
  digits.reserve(text.size());
  for (unsigned char c : text) {
    if (std::isdigit(c))
      digits.push_back(static_cast<char>(c));
  }
  if (digits.empty())
    return digits;

  const size_t firstNonZero = digits.find_first_not_of('0');
  if (firstNonZero == std::string::npos)
    return "0";
  return digits.substr(firstNonZero);
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

static bool IsPathLikelyTooLong(const fs::path &path) {
#ifdef _WIN32
  constexpr size_t kLegacyMaxPathSafetyLimit = 245;
  return ToString(path.u8string()).size() >= kLegacyMaxPathSafetyLimit;
#else
  (void)path;
  return false;
#endif
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
  if (endPtr == trimmedText.c_str() + trimmedText.size() && errno != ERANGE) {
    out = static_cast<float>(parsed);
    return true;
  }
  return false;
}

static bool IsRenderableTrussGeometry(const std::string &path) {
  if (path.empty())
    return false;

  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext == ".3ds" || ext == ".glb" || ext == ".gltf";
}

static fs::path ResolveSceneRelativePath(const std::string &basePath,
                                         const std::string &pathText) {
  fs::path path = PathUtils::PathFromUtf8(pathText);
  if (path.is_absolute() || basePath.empty())
    return path;
  return PathUtils::PathFromUtf8(basePath) / path;
}

// Compares path components using platform filesystem case-sensitivity rules.
static bool SameFilesystemPathComponent(const fs::path &lhs,
                                        const fs::path &rhs) {
#if defined(_WIN32)
  return ToLowerAscii(lhs.string()) == ToLowerAscii(rhs.string());
#else
  return lhs == rhs;
#endif
}

// Returns true when candidate is inside base after both paths have been
// normalized.
static bool IsPathWithinDirectoryByComponents(const fs::path &candidate,
                                              const fs::path &base) {
  auto candidateIt = candidate.begin();
  auto baseIt = base.begin();
  for (; baseIt != base.end(); ++baseIt, ++candidateIt) {
    if (candidateIt == candidate.end() ||
        !SameFilesystemPathComponent(*candidateIt, *baseIt))
      return false;
  }
  return true;
}

// Builds a normalized absolute path without throwing filesystem exceptions.
static fs::path NormalizedAbsolutePathForImport(const fs::path &path,
                                                std::error_code &ec) {
  ec.clear();
  fs::path absolute = path;
  if (!absolute.is_absolute()) {
    absolute = fs::absolute(path, ec);
    if (ec)
      return {};
  }

  std::error_code canonicalEc;
  fs::path canonical = fs::weakly_canonical(absolute, canonicalEc);
  if (!canonicalEc)
    return canonical.lexically_normal();

  return absolute.lexically_normal();
}

// Converts imported resource paths to scene-relative references only when they
// stay under scene.basePath.
static std::string
ToSceneRelativePathIfPossible(const std::string &basePath,
                                                 const fs::path &candidatePath) {
  if (candidatePath.empty())
    return {};

  if (basePath.empty() || !candidatePath.is_absolute())
    return ToString(candidatePath.u8string());

  std::error_code ec;
  const fs::path base =
      NormalizedAbsolutePathForImport(PathUtils::PathFromUtf8(basePath), ec);
  if (ec)
    return ToString(candidatePath.u8string());

  const fs::path candidate = NormalizedAbsolutePathForImport(candidatePath, ec);
  if (ec)
    return ToString(candidatePath.u8string());

  if (!IsPathWithinDirectoryByComponents(candidate, base))
    return ToString(candidatePath.u8string());

  fs::path relative = fs::relative(candidate, base, ec);
  if (ec || relative.empty())
    return ToString(candidatePath.u8string());

  return ToString(relative.u8string());
}

static std::string ResolveScenePathForRead(const std::string &basePath,
                                           const std::string &pathText) {
  if (pathText.empty())
    return {};
  const std::string normalized = NormalizeArchivePathValue(pathText);
  if (normalized.empty())
    return {};
  const std::string gdtfResolved = ResolveGdtfPath(basePath, normalized);
  if (!gdtfResolved.empty())
    return gdtfResolved;
  return ToString(ResolveSceneRelativePath(basePath, normalized).u8string());
}

// Resolves a scene-provided GDTF spec to the real extracted file path.
// MVR files may omit ".gdtf" in <GDTFSpec> or use a different filename case,
// so we progressively try exact, extension-appended and case-insensitive
// matches.
static std::string ResolveGdtfPath(const std::string &baseDir,
                                   const std::string &spec) {
  const std::string normalizedSpec = NormalizeArchivePathValue(spec);
  if (normalizedSpec.empty())
    return {};

  fs::path candidate = baseDir.empty()
                           ? PathUtils::PathFromUtf8(normalizedSpec)
                           : PathUtils::PathFromUtf8(baseDir) /
                                 PathUtils::PathFromUtf8(normalizedSpec);

  std::error_code ec;
  const std::string candidateExt = ToLowerAscii(candidate.extension().string());
  if (candidateExt == ".gdtf" && fs::exists(candidate, ec) && !ec)
    return ToString(candidate.u8string());
  ec.clear();

  if (!candidate.has_extension()) {
    fs::path withExtension = candidate;
    withExtension += ".gdtf";
    if (fs::exists(withExtension, ec) && !ec)
      return ToString(withExtension.u8string());
    ec.clear();
  }

#if defined(_WIN32)
  // Restricts fallback directory scans to the extracted MVR base directory on
  // Windows.
  if (baseDir.empty())
    return {};
  fs::path lookupDir = PathUtils::PathFromUtf8(baseDir);
#else
  fs::path lookupDir =
      baseDir.empty() ? fs::current_path(ec) : PathUtils::PathFromUtf8(baseDir);
#endif
  if (ec || !fs::exists(lookupDir, ec) || ec)
    return {};

  const std::string expectedStem = ToLowerAscii(
      Trim(PathUtils::PathFromUtf8(normalizedSpec).filename().stem().string()));
  const std::string expectedFixtureNameKey =
      NormalizeFixtureNameLookupKey(expectedStem);
  const std::string normalizedSpecKey = NormalizeGdtfLookupKey(normalizedSpec);
  for (const auto &entry : fs::directory_iterator(lookupDir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;

    const fs::path entryPath = entry.path();
    if (ToLowerAscii(entryPath.extension().string()) != ".gdtf")
      continue;
    const std::string entryStem = ToLowerAscii(Trim(entryPath.stem().string()));
    if (entryStem == expectedStem)
      return ToString(entryPath.u8string());

    // Last fallback: compare normalized names ignoring case and extra blanks
    // before extension.
    if (!normalizedSpecKey.empty() &&
        NormalizeGdtfLookupKey(entryPath.filename().generic_string()) ==
            normalizedSpecKey) {
      return ToString(entryPath.u8string());
    }

    const std::string perastageFixtureName =
        ExtractPerastageFixtureNameFromFileName(entryPath.filename());
    if (!perastageFixtureName.empty() &&
        NormalizeFixtureNameLookupKey(perastageFixtureName) ==
            expectedFixtureNameKey) {
      return ToString(entryPath.u8string());
    }
  }

  return {};
}

static std::string DescribeTrussForLog(const Truss &truss) {
  const std::string displayName = truss.name.empty() ? "(unnamed)" : truss.name;
  std::ostringstream oss;
  oss << "uuid='" << truss.uuid << "', name='" << displayName << "', model='"
      << truss.model << "', modelFile='" << truss.modelFile << "', gdtfSpec='"
      << truss.gdtfSpec << "', symbolFile='" << truss.symbolFile << "'";
  return oss.str();
}

static Truss::GeometryRepresentation
ParseTrussRepresentation(const std::string &value) {
  const std::string lower = ToLowerCopy(Trim(value));
  if (lower == "symbolsymdef")
    return Truss::GeometryRepresentation::SymbolSymdef;
  if (lower == "geometry3d")
    return Truss::GeometryRepresentation::Geometry3D;
  if (lower == "publicgdtf")
    return Truss::GeometryRepresentation::PublicGdtf;
  if (lower == "nativeperastage")
    return Truss::GeometryRepresentation::NativePerastage;
  return Truss::GeometryRepresentation::Unknown;
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

struct GdtfConflict {
  std::string type;
  std::string requestedFixtureName;
  std::string mvrPath;
  std::string appPath;
  std::string manufacturer;
  std::string fixtureName;
  std::string modeName;
  int footprint = 0;
  bool hasDictionaryEntry = false;
};

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

// Parses GDTF catalog JSON into normalized entries used by automatic downloads.
static std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
ParseGdtfCatalogEntries(const std::string &listData) {
  using json = nlohmann::json;
  std::vector<gdtf_catalog_matcher::GdtfCatalogEntry> entries;
  json j = json::parse(listData, nullptr, false);
  if (j.is_discarded())
    return entries;

  if (j.is_object()) {
    if (j.contains("data"))
      j = j["data"];
    if (j.contains("fixtures"))
      j = j["fixtures"];
    if (j.contains("list"))
      j = j["list"];
  }
  if (!j.is_array())
    return entries;

  auto jsonToString = [](const json &v) -> std::string {
    if (v.is_string())
      return v.get<std::string>();
    if (v.is_number())
      return v.dump();
    if (v.is_array()) {
      std::string result;
      for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0)
          result += ", ";
        const auto &el = v[i];
        if (el.is_string())
          result += el.get<std::string>();
        else if (el.is_object() && el.contains("name") &&
                 el["name"].is_string())
          result += el["name"].get<std::string>();
        else
          result += el.dump();
      }
      return result;
    }
    if (v.is_object())
      return v.dump();
    return {};
  };
  auto jsonToLongLong = [](const json &v) -> long long {
    if (v.is_number_integer())
      return v.get<long long>();
    if (v.is_number())
      return static_cast<long long>(v.get<double>());
    if (v.is_string()) {
      long long parsed = 0;
      auto begin = v.get_ref<const std::string &>().data();
      auto end = begin + v.get_ref<const std::string &>().size();
      std::from_chars_result result = std::from_chars(begin, end, parsed);
      if (result.ec == std::errc{})
        return parsed;
    }
    return 0;
  };
  auto jsonToFloat = [](const json &v) -> float {
    if (v.is_number())
      return static_cast<float>(v.get<double>());
    if (v.is_string()) {
      try {
        return std::stof(v.get<std::string>());
      } catch (...) {
      }
    }
    return 0.0f;
  };
  auto getValue = [&](const json &item,
                      std::initializer_list<const char *> keys) -> std::string {
    for (const char *key : keys) {
      auto it = item.find(key);
      if (it != item.end())
        return jsonToString(*it);
    }
    return {};
  };

  auto parseModes = [&](const json &item) {
    std::vector<gdtf_catalog_matcher::GdtfCatalogModeCandidate> modes;
    if (item.contains("dmxModes") && item["dmxModes"].is_array()) {
      for (const auto &mode : item["dmxModes"]) {
        gdtf_catalog_matcher::GdtfCatalogModeCandidate parsed;
        if (mode.is_object()) {
          parsed.name = jsonToString(mode.value("name", json{}));
          parsed.footprint = static_cast<int>(
              jsonToLongLong(mode.value("dmxFootprint", json{})));
        }
        if (!parsed.name.empty() || parsed.footprint > 0)
          modes.push_back(std::move(parsed));
      }
    }
    return modes;
  };

  for (const auto &item : j) {
    if (!item.is_object())
      continue;
    gdtf_catalog_matcher::GdtfCatalogEntry entry;
    entry.rid = getValue(item, {"rid", "revisionId"});
    entry.manufacturer = getValue(item, {"manufacturer", "brand", "mfr"});
    entry.fixtureName = getValue(item, {"fixture", "name", "model"});
    entry.lastModifiedUnix = jsonToLongLong(item.value("lastModified", json{}));
    entry.rating = jsonToFloat(item.value("rating", json{}));
    entry.modes = parseModes(item);
    if (!entry.rid.empty())
      entries.push_back(std::move(entry));
  }
  return entries;
}

static void ApplySupportHoistInfoDefaults(Support &support) {
  if (support.dummyProfileId.empty() && !support.dummyPreset.empty()) {
    const auto profile =
        DummyProfileLibrary::FindByDisplayName(support.dummyPreset);
    if (profile.has_value())
      support.dummyProfileId = profile->id;
  }

  support.hoistFunction = NormalizeHoistFunction(
      support.hoistFunction.empty() ? support.function : support.hoistFunction);
  support.hoistDataSource = NormalizeHoistDataSource(support.hoistDataSource);
  support.motorNameSource = ResolveHoistFieldDataSource(
      support.motorNameSource, support.hoistDataSource);
  support.motorManufacturerSource = ResolveHoistFieldDataSource(
      support.motorManufacturerSource, support.hoistDataSource);
  support.motorModelSource = ResolveHoistFieldDataSource(
      support.motorModelSource, support.hoistDataSource);
  support.capacitySource = ResolveHoistFieldDataSource(support.capacitySource,
                                                       support.hoistDataSource);
  support.weightSource = ResolveHoistFieldDataSource(support.weightSource,
                                                     support.hoistDataSource);
  support.hoistFunctionSource = ResolveHoistFieldDataSource(
      support.hoistFunctionSource, support.hoistDataSource);
  if (support.function.empty())
    support.function = support.hoistFunction;
}

// Reads legacy per-fixture category metadata from older Perastage MVR exports.
static void ReadFixtureCategoryFromUserData(tinyxml2::XMLElement *fixtureNode,
                                            Fixture &fixture) {
  if (!fixtureNode)
    return;

  tinyxml2::XMLElement *ud = fixtureNode->FirstChildElement("UserData");
  if (!ud)
    return;

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    tinyxml2::XMLElement *info = data->FirstChildElement("FixtureInfo");
    if (!info)
      continue;

    if (tinyxml2::XMLElement *categoryNode =
            info->FirstChildElement("Category")) {
      if (const char *txt = categoryNode->GetText())
        fixture.category = GdtfFixtureCategory::NormalizeCategory(Trim(txt));
    }

    if (tinyxml2::XMLElement *sourceNode =
            info->FirstChildElement("CategorySource")) {
      if (const char *txt = sourceNode->GetText())
        fixture.categorySource = Trim(txt);
    }
    if (tinyxml2::XMLElement *reasonNode =
            info->FirstChildElement("CategoryReason")) {
      if (const char *txt = reasonNode->GetText())
        fixture.categorySourceReason = Trim(txt);
    }

    if (!fixture.category.empty() && fixture.categorySource.empty())
      fixture.categorySource = GdtfFixtureCategory::kManualSource;
    if (fixture.categorySource == GdtfFixtureCategory::kManualSource)
      fixture.categorySourceReason.clear();
    return;
  }
}

struct LegacyFixtureIdentity {
  std::string instanceName;
  std::string stableId;
};

// Reads legacy Perastage fixture identity fields used by older MVR exports.
static LegacyFixtureIdentity
ReadLegacyFixtureIdentityFromUserData(tinyxml2::XMLElement *fixtureNode) {
  LegacyFixtureIdentity identity;
  if (!fixtureNode)
    return identity;

  tinyxml2::XMLElement *ud = fixtureNode->FirstChildElement("UserData");
  if (!ud)
    return identity;

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    tinyxml2::XMLElement *info = data->FirstChildElement("FixtureInfo");
    if (!info)
      continue;

    auto readText = [&](const char *name) -> std::string {
      if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
        if (const char *txt = el->GetText())
          return Trim(txt);
      }
      return {};
    };

    identity.instanceName = readText("InstanceName");
    identity.stableId = readText("StableId");
    return identity;
  }
  return identity;
}

// Reads one Perastage hoist metadata element into a support.
static void ReadSupportHoistInfoElement(tinyxml2::XMLElement *info,
                                        Support &support) {
  if (!info)
    return;

  auto readFloat = [&](const char *name, float &out) {
    if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
      if (const char *txt = el->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(txt, parsed))
          out = parsed;
      }
    }
  };
  auto readText = [&](const char *name) -> std::string {
    if (tinyxml2::XMLElement *el = info->FirstChildElement(name)) {
      if (const char *txt = el->GetText())
        return Trim(txt);
    }
    return {};
  };

  readFloat("Capacity", support.capacityKg);
  readFloat("Weight", support.weightKg);
  if (tinyxml2::XMLElement *load = info->FirstChildElement("Load")) {
    readFloat("Load", support.loadKg);
    support.loadSource = "Manual";
  }

  std::string hoistFunction = readText("RiggingPoint");
  if (hoistFunction.empty())
    hoistFunction = readText("Function");
  if (!hoistFunction.empty())
    support.hoistFunction = NormalizeHoistFunction(hoistFunction);

  const std::string motorName = readText("MotorName");
  if (!motorName.empty())
    support.motorName = motorName;
  const std::string manufacturer = readText("MotorManufacturer");
  if (!manufacturer.empty())
    support.motorManufacturer = manufacturer;
  const std::string model = readText("MotorModel");
  if (!model.empty())
    support.motorModel = model;
  const std::string fixtureUuid = readText("MotorFixtureUuid");
  if (!fixtureUuid.empty())
    support.motorFixtureUuid = fixtureUuid;

  const std::string useDefaults = ToLowerCopy(readText("UseMotorDefaults"));
  if (!useDefaults.empty()) {
    support.useMotorDefaults = !(useDefaults == "false" ||
                                 useDefaults == "0" || useDefaults == "no");
  }

  const std::string dummyPreset = readText("DummyPreset");
  if (!dummyPreset.empty())
    support.dummyPreset = dummyPreset;
  const std::string dummyProfileId = readText("DummyProfileId");
  if (!dummyProfileId.empty())
    support.dummyProfileId = dummyProfileId;

  std::string source = readText("ValueSource");
  if (source.empty())
    source = readText("DataSource");
  if (!source.empty())
    support.hoistDataSource = NormalizeHoistDataSource(source);

  const std::string motorNameSource = readText("MotorNameSource");
  if (!motorNameSource.empty())
    support.motorNameSource = NormalizeHoistDataSource(motorNameSource);

  const std::string motorManufacturerSource =
      readText("MotorManufacturerSource");
  if (!motorManufacturerSource.empty()) {
    support.motorManufacturerSource =
        NormalizeHoistDataSource(motorManufacturerSource);
  }

  const std::string motorModelSource = readText("MotorModelSource");
  if (!motorModelSource.empty())
    support.motorModelSource = NormalizeHoistDataSource(motorModelSource);

  const std::string capacitySource = readText("CapacitySource");
  if (!capacitySource.empty())
    support.capacitySource = NormalizeHoistDataSource(capacitySource);

  const std::string weightSource = readText("WeightSource");
  if (!weightSource.empty())
    support.weightSource = NormalizeHoistDataSource(weightSource);

  std::string hoistFunctionSource = readText("RiggingPointSource");
  if (hoistFunctionSource.empty())
    hoistFunctionSource = readText("FunctionSource");
  if (!hoistFunctionSource.empty()) {
    support.hoistFunctionSource =
        NormalizeHoistDataSource(hoistFunctionSource);
  }
}

// Reads legacy Support/UserData hoist metadata into a support.
static void ReadSupportHoistInfoFromUserData(tinyxml2::XMLElement *supportNode,
                                             Support &support) {
  for (tinyxml2::XMLElement *ud = supportNode->FirstChildElement("UserData");
       ud; ud = ud->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      tinyxml2::XMLElement *info = data->FirstChildElement("HoistInfo");
      if (!info)
        info = data->FirstChildElement("MotorInfo");
      ReadSupportHoistInfoElement(info, support);
    }
  }
}
// Imports an MVR file into the global application scene.
bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 bool promptConflicts, bool applyDictionary,
                                 ProgressCallback progressCallback) {
  MvrImportResult importResult;
  return ImportFromFile(filePath, importResult, MvrImportMode::ReplaceProject,
                        promptConflicts, applyDictionary, progressCallback);
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

  reportProgress("Extracting package resources...");

  std::string tempDir = CreateTemporaryDirectory();
  std::string mvrPath = ToString(path.u8string());
  fs::path tempPath(tempDir);
  if (!ExtractMvrZip(mvrPath, tempDir)) {
    LogMessage("Failed to extract MVR file.");
    return false;
  }

  int extractedGdtfEntryCount = 0;
  std::error_code gdtfCountEc;
  for (const auto &entry : fs::directory_iterator(tempPath, gdtfCountEc)) {
    if (gdtfCountEc)
      break;
    std::error_code regularEc;
    if (entry.is_regular_file(regularEc) && !regularEc &&
        ToLowerAscii(entry.path().extension().string()) == ".gdtf") {
      ++extractedGdtfEntryCount;
    }
  }
  LogMessage(
      Logger::Level::Info,
      "MVR extraction diagnostics: basePath='" + ToString(tempPath.u8string()) +
          "', extractedGdtfEntries=" + std::to_string(extractedGdtfEntryCount));

  fs::path sceneFile = tempPath / "GeneralSceneDescription.xml";
  std::error_code sceneFileEc;
  if (!fs::exists(sceneFile, sceneFileEc) || sceneFileEc) {
    // Some MVR packages may store the file with a different case.
    std::string target = "generalscenedescription.xml";
    sceneFileEc.clear();
    for (const auto &entry : fs::directory_iterator(tempPath, sceneFileEc)) {
      if (sceneFileEc)
        break;
      std::error_code regularFileEc;
      if (entry.is_regular_file(regularFileEc) && !regularFileEc) {
        std::string name = entry.path().filename().string();
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == target) {
          sceneFile = entry.path();
          break;
        }
      }
    }
  }
  sceneFileEc.clear();
  if (!fs::exists(sceneFile, sceneFileEc) || sceneFileEc) {
    LogMessage("Missing GeneralSceneDescription.xml in MVR.");
    return false;
  }

  std::string scenePath = ToString(sceneFile.u8string());
  reportProgress("Parsing scene data...");
  const bool parsed =
      ParseSceneXml(scenePath, importResult, options, progressCallback);
  if (!parsed)
    return false;

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
  return NormalizeArchivePathValue(archivePath);
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

// Creates a temporary extraction directory for the current MVR import.
std::string MvrImporter::CreateTemporaryDirectory() {
  fs::path tempBase = fs::temp_directory_path();
  for (int attempt = 0; attempt < 32; ++attempt) {
    fs::path fullPath = tempBase / ("ps_" + GenerateShortToken());
    std::error_code ec;
    if (fs::create_directory(fullPath, ec) && !ec) {
      // Return the path encoded as UTF-8 so it can safely be converted back
      // using PathUtils::PathFromUtf8 or passed to wxWidgets APIs expecting
      // UTF-8 strings.
      return ToString(fullPath.u8string());
    }
  }

  fs::path fallback =
      tempBase /
      ("ps_" +
       std::to_string(
                                      std::chrono::system_clock::now().time_since_epoch().count()));
  fs::create_directory(fallback);
  return ToString(fallback.u8string());
}

// Extracts an MVR zip archive into the destination directory.
bool MvrImporter::ExtractMvrZip(const std::string &mvrPath,
                                const std::string &destDir) {
  wxFileInputStream input(wxString::FromUTF8(mvrPath.c_str()));
  if (!input.IsOk()) {
    LogMessage("Failed to open MVR file.");
    return false;
  }

  wxZipInputStream zipStream(input);
  std::unique_ptr<wxZipEntry> entry;

  while ((entry.reset(zipStream.GetNextEntry())), entry) {
    // Extract entry names using UTF-8 to preserve special characters
    std::string entryName = entry->GetName().ToUTF8().data();
    const std::string normalizedUnsafeCheck = NormalizeSlashes(entryName);
    const fs::path relativeEntryPath = PathUtils::PathFromUtf8(normalizedUnsafeCheck);
    if (normalizedUnsafeCheck.empty() || relativeEntryPath.is_absolute() ||
        relativeEntryPath.has_root_name() ||
        normalizedUnsafeCheck.find(':') != std::string::npos ||
        std::any_of(relativeEntryPath.begin(), relativeEntryPath.end(),
                    [](const fs::path &part) { return part == ".."; })) {
      LogMessage(Logger::Level::Warn,
                 "Skipping unsafe MVR archive entry: " + entryName);
      char discardBuffer[4096];
      while (true) {
        zipStream.Read(discardBuffer, sizeof(discardBuffer));
        if (zipStream.LastRead() == 0)
          break;
      }
      continue;
    }
    fs::path fullPath =
        PathUtils::PathFromUtf8(destDir) / relativeEntryPath;

    if (entry->IsDir()) {
      std::string dirUtf8 = ToString(fullPath.u8string());
      wxFileName::Mkdir(wxString::FromUTF8(dirUtf8.c_str()), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      continue;
    }

    std::string parentUtf8 = ToString(fullPath.parent_path().u8string());
    wxFileName::Mkdir(wxString::FromUTF8(parentUtf8.c_str()), wxS_DIR_DEFAULT,
                      wxPATH_MKDIR_FULL);

    const std::string normalizedEntryName = NormalizeArchivePath(entryName);
    const size_t fullPathLength = ToString(fullPath.u8string()).size();

    auto tryOpenOutput = [](const fs::path &path) {
      return std::ofstream(path, std::ios::binary);
    };

    std::ofstream output;
    bool remapped = false;
    if (!IsPathLikelyTooLong(fullPath))
      output = tryOpenOutput(fullPath);

    if (!output.is_open()) {
      fs::path longDir = PathUtils::PathFromUtf8(destDir) / "_long";
      std::string extension =
          PathUtils::PathFromUtf8(entryName).extension().string();
      std::string hashBase =
          std::to_string(std::hash<std::string>{}(normalizedEntryName));
      wxFileName::Mkdir(
          wxString::FromUTF8(ToString(longDir.u8string()).c_str()),
                        wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

      for (int suffix = 0; suffix < 64 && !output.is_open(); ++suffix) {
        std::string candidateName = hashBase;
        if (suffix > 0)
          candidateName += "_" + std::to_string(suffix);
        candidateName += extension;
        fs::path candidatePath =
            longDir / PathUtils::PathFromUtf8(candidateName);
        output = tryOpenOutput(candidatePath);
        if (output.is_open()) {
          fullPath = candidatePath;
          pathRemap[normalizedEntryName] = ToString(
              (fs::path("_long") / PathUtils::PathFromUtf8(candidateName))
                                                        .u8string());
          remapped = true;
        }
      }
    }

    if (!output.is_open()) {
      std::ostringstream msg;
      msg << "Cannot create file while extracting MVR entry. entry='"
          << entryName << "', path='" << ToString(fullPath.u8string())
          << "', pathLength=" << fullPathLength;
      const std::string loweredEntry = ToLowerAscii(normalizedEntryName);
      const bool isSceneXml =
          loweredEntry == "generalscenedescription.xml" ||
          fs::path(loweredEntry).filename().generic_string() ==
              "generalscenedescription.xml";
      if (isSceneXml) {
        LogMessage(Logger::Level::Error,
                   msg.str() + " (required scene XML; aborting import)");
        return false;
      }

      LogMessage(Logger::Level::Warn,
                 msg.str() + " (asset entry skipped, continuing import)");
      char discardBuffer[4096];
      while (true) {
        zipStream.Read(discardBuffer, sizeof(discardBuffer));
        if (zipStream.LastRead() == 0)
          break;
      }
      continue;
    }

    if (remapped) {
      const std::string remappedPath = pathRemap[normalizedEntryName];
      std::ostringstream warn;
      warn << "MVR extraction remapped long path entry. entry='" << entryName
           << "', remapped='" << remappedPath
           << "', originalLength=" << fullPathLength;
      LogMessage(Logger::Level::Warn, warn.str());
    }

    char buffer[4096];
    while (true) {
      zipStream.Read(buffer, sizeof(buffer));
      size_t bytes = zipStream.LastRead();
      if (bytes == 0)
        break;
      output.write(buffer, bytes);
    }

    output.close();
  }

  return true;
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

  struct RootFixtureTypeInfo {
    std::string category;
    std::string categorySource;
    std::string visualColorHex;
  };
  std::unordered_map<std::string, RootFixtureTypeInfo>
      rootFixtureTypeInfoByKey;

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
          if (!typeInfo.category.empty() &&
              typeInfo.categorySource.empty())
            typeInfo.categorySource = GdtfFixtureCategory::kManualSource;
          if (tinyxml2::XMLElement *visualColor =
                  info->FirstChildElement("VisualColor")) {
            if (const char *txt = visualColor->GetText()) {
              const std::string value = Trim(txt);
              if (isHexRgb(value))
                typeInfo.visualColorHex = value;
            }
          }
          if (!typeInfo.category.empty() ||
              !typeInfo.visualColorHex.empty())
            rootFixtureTypeInfoByKey[key] = typeInfo;
        }
      }
    }
  };
  parseRootFixtureTypeInfoMap(root->FirstChildElement("UserData"));

  std::unordered_map<std::string, tinyxml2::XMLElement *> rootTrussInfoByUuid;
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
      for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap");
           map; map = map->NextSiblingElement("TrussInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo");
             info; info = info->NextSiblingElement("TrussInfo")) {
          const std::string uuid = CanonicalizeUuid(
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : ""));
          if (!uuid.empty())
            rootTrussInfoByUuid[uuid] = info;
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
      for (tinyxml2::XMLElement *map = data->FirstChildElement("HoistInfoMap");
           map; map = map->NextSiblingElement("HoistInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("HoistInfo");
             info; info = info->NextSiblingElement("HoistInfo")) {
          const std::string rawUuid =
              Trim(info->Attribute("uuid") ? info->Attribute("uuid") : "");
          const std::string canonicalUuid = CanonicalizeUuid(rawUuid);
          if (!rawUuid.empty())
            rootHoistInfoByUuid[rawUuid] = info;
          if (!canonicalUuid.empty())
            rootHoistInfoByUuid[canonicalUuid] = info;
        }
      }
    }
  };
  parseRootHoistInfoMap(root->FirstChildElement("UserData"));

  // ---- Parse AUXData for Symdefs and Positions ----
  std::unordered_map<std::string, std::string> legacyPositionIdToCanonical;
  if (tinyxml2::XMLElement *auxNode = sceneNode->FirstChildElement("AUXData")) {
    for (tinyxml2::XMLElement *pos = auxNode->FirstChildElement("Position");
         pos; pos = pos->NextSiblingElement("Position")) {
      const std::string rawUid =
          Trim(pos->Attribute("uuid") ? pos->Attribute("uuid") : "");
      const char *name = pos->Attribute("name");
      const std::string canonicalUid = CanonicalizeUuid(rawUid);
      if (canonicalUid.empty()) {
        if (rawUid.empty())
          continue;
        std::string seed =
            "mvr:legacy-position:" + rawUid + ":" + (name ? Trim(name) : "");
        const std::string generated = DeriveDeterministicUuid(seed);
        legacyPositionIdToCanonical[rawUid] = generated;
        scene.positions[generated] = name ? name : rawUid;
        LogMessage(Logger::Level::Warn,
                   "MVR import migrated non-canonical Position uuid '" +
                       rawUid + "' -> '" + generated + "'");
      } else {
        if (canonicalUid != rawUid)
          legacyPositionIdToCanonical[rawUid] = canonicalUid;
        scene.positions[canonicalUid] = name ? name : "";
      }
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

  auto normalizeAndResolveGeometryFileName = [&](std::string fileName) {
    std::string normalized = normalizeGeometryFileName(std::move(fileName));
    if (normalized.empty())
      return normalized;
    std::string primitiveToken;
    if (mvr::ResolvePrimitiveTokenFromModelRef(normalized, primitiveToken))
      return primitiveToken;
    std::string remapped = RemapArchivePathIfNeeded(normalized);
    if (mvr::ResolvePrimitiveTokenFromModelRef(remapped, primitiveToken))
      return primitiveToken;
    fs::path remappedPath = PathUtils::PathFromUtf8(remapped);
    fs::path resolved = ResolveSceneRelativePath(scene.basePath, remapped);
    if (!remappedPath.has_extension()) {
      const std::array<std::string, 3> extensions = {".gltf", ".glb", ".3ds"};
      for (const std::string &ext : extensions) {
        fs::path candidate = resolved;
        candidate += ext;
        std::error_code existsEc;
        if (fs::exists(candidate, existsEc) && !existsEc) {
          resolved = candidate;
          break;
        }
      }
      if (!resolved.has_extension())
        resolved += ".3ds";
    }
    return ToSceneRelativePathIfPossible(scene.basePath, resolved);
  };

  std::unordered_map<std::string, std::string> resolvedGdtfPathCache;
  std::unordered_map<std::string, std::vector<std::string>> gdtfModesCache;
  std::unordered_map<std::string, std::unordered_map<std::string, int>>
      gdtfModeChannelCountCache;
  std::unordered_map<std::string, GdtfFixtureCategory::InferenceResult>
      categoryInferenceByResolvedPath;
  const std::string kEmptyResolvedPath;
  auto resolveGdtfPathCached =
      [&](const std::string &spec) -> const std::string & {
    const std::string normalized = NormalizeArchivePathValue(spec);
    if (normalized.empty())
      return kEmptyResolvedPath;

    auto it = resolvedGdtfPathCache.find(normalized);
    if (it != resolvedGdtfPathCache.end())
      return it->second;

    std::string resolved = ResolveGdtfPath(scene.basePath, normalized);
    if (resolved.empty())
      resolved = ToString(
          ResolveSceneRelativePath(scene.basePath, normalized).u8string());
    return resolvedGdtfPathCache.emplace(normalized, std::move(resolved))
        .first->second;
  };

  auto normalizeGdtfSpecForScene = [&](const std::string &spec) {
    const std::string normalized = NormalizeArchivePathValue(spec);
    if (normalized.empty())
      return std::string{};
    return ToSceneRelativePathIfPossible(
        scene.basePath,
        PathUtils::PathFromUtf8(resolveGdtfPathCached(normalized)));
  };

  auto resolvedGdtfFileExists = [](const std::string &gdtfPath) {
    if (gdtfPath.empty())
      return false;
    std::error_code ec;
    return fs::is_regular_file(PathUtils::PathFromUtf8(gdtfPath), ec) && !ec;
  };

  auto getGdtfModesCached =
      [&](const std::string &gdtfPath) -> const std::vector<std::string> & {
    auto cacheIt = gdtfModesCache.find(gdtfPath);
    if (cacheIt != gdtfModesCache.end())
      return cacheIt->second;
    if (!resolvedGdtfFileExists(gdtfPath))
      return gdtfModesCache.emplace(gdtfPath, std::vector<std::string>{})
          .first->second;
    return gdtfModesCache.emplace(gdtfPath, GetGdtfModes(gdtfPath))
        .first->second;
  };

  auto getGdtfModeChannelCountCached = [&](const std::string &gdtfPath,
                                           const std::string &modeName) {
    if (!resolvedGdtfFileExists(gdtfPath))
      return -1;
    auto &channelCountByMode = gdtfModeChannelCountCache[gdtfPath];
    auto countIt = channelCountByMode.find(modeName);
    if (countIt != channelCountByMode.end())
      return countIt->second;
    const int count = GetGdtfModeChannelCount(gdtfPath, modeName);
    channelCountByMode.emplace(modeName, count);
    return count;
  };

  auto resolveExistingGdtfModeCached =
      [&](const std::string &gdtfPath, const std::string &requestedMode,
                                           std::optional<int> channelCountHint) {
    const std::vector<std::string> &modes = getGdtfModesCached(gdtfPath);
    if (modes.empty())
      return requestedMode;

        const std::string normalizedRequested =
            ToLowerAscii(Trim(requestedMode));
    if (!normalizedRequested.empty()) {
      for (const std::string &mode : modes) {
        if (ToLowerAscii(Trim(mode)) == normalizedRequested)
          return mode;
      }
    }

    const std::string requestedDigitSignature =
        ExtractDigitSignature(normalizedRequested);
    if (!requestedDigitSignature.empty()) {
      for (const std::string &mode : modes) {
        const std::string modeDigitSignature =
            ExtractDigitSignature(ToLowerAscii(Trim(mode)));
        if (!modeDigitSignature.empty() &&
            modeDigitSignature == requestedDigitSignature) {
          return mode;
        }
      }
    }

    if (channelCountHint.has_value() && channelCountHint.value() > 0) {
      for (const std::string &mode : modes) {
        const int modeChannelCount =
            getGdtfModeChannelCountCached(gdtfPath, mode);
        if (modeChannelCount == channelCountHint.value())
          return mode;
      }
    }

    for (const std::string &mode : modes) {
      const std::string normalized = ToLowerAscii(Trim(mode));
      if (normalized == "default" || normalized == "standard")
        return mode;
    }

    return modes.front();
  };

  struct GdtfFixtureMetadata {
    std::string fixtureName;
    float weightKg = 0.0f;
    float powerW = 0.0f;
    bool hasProperties = false;
  };
  std::unordered_map<std::string, GdtfFixtureMetadata> gdtfFixtureMetadataCache;
  const GdtfFixtureMetadata kEmptyFixtureMetadata{};
  auto getFixtureMetadata =
      [&](const std::string &resolvedGdtfPath) -> const GdtfFixtureMetadata & {
    if (resolvedGdtfPath.empty() || !resolvedGdtfFileExists(resolvedGdtfPath))
      return kEmptyFixtureMetadata;

    auto it = gdtfFixtureMetadataCache.find(resolvedGdtfPath);
    if (it != gdtfFixtureMetadataCache.end())
      return it->second;

    GdtfFixtureMetadata metadata;
    metadata.fixtureName = Trim(GetGdtfFixtureName(resolvedGdtfPath));
    metadata.hasProperties =
        GetGdtfProperties(resolvedGdtfPath, metadata.weightKg, metadata.powerW);
    return gdtfFixtureMetadataCache
        .emplace(resolvedGdtfPath, std::move(metadata))
        .first->second;
  };

  std::unordered_map<std::string, std::optional<Truss>> trussDefinitionCache;
  auto loadTrussDefinitionCached = [&](const std::string &resolvedGdtfPath,
                                       Truss &out) {
    if (resolvedGdtfPath.empty())
      return false;

    auto it = trussDefinitionCache.find(resolvedGdtfPath);
    if (it == trussDefinitionCache.end()) {
      Truss loaded;
      if (LoadTrussDefinition(resolvedGdtfPath, loaded))
        it = trussDefinitionCache.emplace(resolvedGdtfPath, std::move(loaded))
                 .first;
      else
        it = trussDefinitionCache.emplace(resolvedGdtfPath, std::nullopt).first;
    }

    if (!it->second.has_value())
      return false;
    out = *it->second;
    return true;
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

  auto ensurePositionEntry = [&](const std::string &positionId) -> std::string {
    if (positionId.empty())
      return {};

    auto legacyIt = legacyPositionIdToCanonical.find(positionId);
    const std::string remappedId = legacyIt != legacyPositionIdToCanonical.end()
                                       ? legacyIt->second
                                       : positionId;
    const std::string canonicalId = CanonicalizeUuid(remappedId);
    const std::string normalizedId =
        canonicalId.empty() ? remappedId : canonicalId;

    auto it = scene.positions.find(normalizedId);
    if (it != scene.positions.end())
      return it->second;

    // Create a placeholder entry so the position is preserved on export.
    std::string generated = normalizedId;
    if (generated.empty()) {
      generated = DeriveDeterministicUuid("mvr:position-ref:" + positionId);
      legacyPositionIdToCanonical[positionId] = generated;
      LogMessage(Logger::Level::Warn,
                 "MVR import generated canonical Position uuid '" + generated +
                     "' for unresolved reference '" + positionId + "'");
    }
    scene.positions[generated] = positionId;
    return positionId;
  };

  std::unordered_set<std::string> usedStableUuids;

  struct CachedCategory {
    std::string category;
    std::string source;
    std::string reason;
  };
  std::unordered_map<std::string, CachedCategory> categoryByTypeKey;
  auto buildStableIdSeed = [&](const char *kind, tinyxml2::XMLElement *node,
                               const std::string &layerName,
                               const Matrix &nodeTransform,
                               const std::string &rawUuid) {
    std::ostringstream seed;
    seed << "mvr:" << kind << ':' << layerName << ':';
    if (const char *nameAttr = node->Attribute("name"))
      seed << Trim(nameAttr);
    seed << ':' << MatrixUtils::FormatMatrix(nodeTransform) << ':' << rawUuid;
    return seed.str();
  };

  auto resolveStableUuid = [&](const char *kind, tinyxml2::XMLElement *node,
                               const std::string &layerName,
                               const Matrix &nodeTransform,
                               const std::string &legacyStableId = {}) {
    const char *uuidAttr = node->Attribute("uuid");
    std::string rawUuid = uuidAttr ? Trim(uuidAttr) : std::string{};
    std::string stableUuid = CanonicalizeUuid(rawUuid);
    const std::string seed =
        buildStableIdSeed(kind, node, layerName, nodeTransform, rawUuid);

    if (stableUuid.empty()) {
      const std::string legacyUuid = CanonicalizeUuid(Trim(legacyStableId));
      if (!legacyUuid.empty()) {
        stableUuid = legacyUuid;
      } else if (!rawUuid.empty()) {
        LogMessage(Logger::Level::Warn,
                   wxString::Format("MVR import: %s UUID '%s' is invalid. "
                                    "Applying deterministic fallback.",
                                    kind, rawUuid.c_str())
                       .ToStdString());
      }
      if (stableUuid.empty())
        stableUuid = DeriveDeterministicUuid(seed);
    }

    if (usedStableUuids.contains(stableUuid)) {
      LogMessage(Logger::Level::Warn,
                 wxString::Format("MVR import: UUID collision for %s '%s'. "
                                  "Applying controlled fallback UUID.",
                                  kind, stableUuid.c_str())
                     .ToStdString());
      int suffix = 1;
      std::string candidate;
      do {
        candidate =
            DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
      } while (usedStableUuids.contains(candidate));
      stableUuid = std::move(candidate);
    }

    usedStableUuids.insert(stableUuid);
    return stableUuid;
  };

  auto referenceUuidForNode = [&](const char *kind, tinyxml2::XMLElement *node,
                                  const std::string &layerName,
                                  const Matrix &nodeTransform) {
    const char *uuidAttr = node->Attribute("uuid");
    std::string rawUuid = uuidAttr ? Trim(uuidAttr) : std::string{};
    std::string stableUuid = CanonicalizeUuid(rawUuid);
    if (!stableUuid.empty())
      return stableUuid;
    const std::string seed =
        buildStableIdSeed(kind, node, layerName, nodeTransform, rawUuid);
    return DeriveDeterministicUuid(seed);
  };

  std::unordered_map<std::string, std::optional<GdtfDictionary::Entry>>
      dictionaryEntryByTypeCache;
  auto getDictionaryEntryCached = [&](const std::string &typeName)
      -> const std::optional<GdtfDictionary::Entry> & {
    static const std::optional<GdtfDictionary::Entry> kEmptyEntry =
        std::nullopt;
    if (typeName.empty())
      return kEmptyEntry;
    auto it = dictionaryEntryByTypeCache.find(typeName);
    if (it != dictionaryEntryByTypeCache.end())
      return it->second;
    return dictionaryEntryByTypeCache
        .emplace(typeName, GdtfDictionary::Get(typeName))
        .first->second;
  };

  std::unordered_map<std::string, GdtfConflict> pendingGdtfConflictByType;
  int trussSymbolSymdefPreservedCount = 0;
  std::unordered_map<std::string, int> trussSymbolSymdefPreservedBySymdef;

  // Parses a Fixture XML node into scene data while preserving its original
  // matching identity.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseFixture = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName,
                         const Matrix &nodeTransform,
                         const Matrix &localTransform,
                         const std::string &parentGroupUuid) {
        Fixture fixture;
        const LegacyFixtureIdentity legacyIdentity =
            ReadLegacyFixtureIdentityFromUserData(node);
        const char *rawUuidAttr = node->Attribute("uuid");
        const std::string rawFixtureUuid =
            rawUuidAttr ? Trim(rawUuidAttr) : std::string{};
        fixture.uuid = resolveStableUuid(
            "Fixture", node, layerName, nodeTransform, legacyIdentity.stableId);
        if (!rawFixtureUuid.empty() && rawFixtureUuid != fixture.uuid)
          fixtureUuidRemap[rawFixtureUuid] = fixture.uuid;
        fixture.layer = layerName;
        fixture.transform = nodeTransform;
        fixture.localTransform = localTransform;
        fixture.hasLocalTransform = true;
        fixture.parentGroupUuid = parentGroupUuid;

        const char *nameAttr = node->Attribute("name");
        const std::string rawFixtureNodeName =
            nameAttr ? Trim(nameAttr) : std::string{};
        if (!rawFixtureNodeName.empty())
          fixture.instanceName = rawFixtureNodeName;
        else if (!legacyIdentity.instanceName.empty())
          fixture.instanceName = legacyIdentity.instanceName;

        fixtureIdOf(node, fixture.fixtureIdText, fixture.fixtureIdNumeric);
        fixture.fixtureId = fixture.fixtureIdNumeric;
        intOf(node, "UnitNumber", fixture.unitNumber);
        intOf(node, "CustomId", fixture.customId);
        intOf(node, "CustomIdType", fixture.customIdType);

        fixture.gdtfSpec = textOf(node, "GDTFSpec");
        const std::string rawGdtfSpec = fixture.gdtfSpec;
        fixture.originalMvrGdtfSpec = rawGdtfSpec;
        fixture.gdtfMode = textOf(node, "GDTFMode");
        fixture.requestedFixtureName =
            mvr::gdtf_import_matching::SelectRequestedFixtureName(
                rawFixtureNodeName, rawGdtfSpec);
        fixture.focus = textOf(node, "Focus");
        fixture.function = textOf(node, "Function");
        fixture.position = CanonicalizeUuid(textOf(node, "Position"));
        if (fixture.position.empty())
          fixture.position = textOf(node, "Position");
        fixture.positionName = ensurePositionEntry(fixture.position);
        auto fixturePosIt = legacyPositionIdToCanonical.find(fixture.position);
        if (fixturePosIt != legacyPositionIdToCanonical.end())
          fixture.position = fixturePosIt->second;
        if (tinyxml2::XMLElement *colorNode =
                node->FirstChildElement("Color")) {
          if (const char *txt = colorNode->GetText())
            fixture.mvrFixtureColorHex = CieToHex(txt);
        }
        float legacyPowerConsumptionW = 0.0f;
        bool hasLegacyPowerConsumption = false;
        if (tinyxml2::XMLElement *pcNode =
                node->FirstChildElement("PowerConsumption")) {
          if (const char *txt = pcNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed)) {
              legacyPowerConsumptionW = parsed;
              hasLegacyPowerConsumption = true;
            }
          }
        }
        float legacyWeightKg = 0.0f;
        bool hasLegacyWeight = false;
        if (tinyxml2::XMLElement *wNode = node->FirstChildElement("Weight")) {
          if (const char *txt = wNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed)) {
              legacyWeightKg = parsed;
              hasLegacyWeight = true;
            }
          }
        }
        std::string resolvedGdtfPathForFixture;
        if (!fixture.gdtfSpec.empty()) {
          fixture.gdtfSpec = RemapArchivePathIfNeeded(fixture.gdtfSpec);
          const std::string &resolvedGdtfPath =
              resolveGdtfPathCached(fixture.gdtfSpec);
          resolvedGdtfPathForFixture = resolvedGdtfPath;
          fixture.gdtfSpec = normalizeGdtfSpecForScene(fixture.gdtfSpec);
          if (fixture.gdtfSpec.empty() && !rawGdtfSpec.empty())
            fixture.gdtfSpec = RemapArchivePathIfNeeded(rawGdtfSpec);
          const GdtfFixtureMetadata &metadata =
              getFixtureMetadata(resolvedGdtfPath);
          fixture.typeName = metadata.fixtureName;
          if (fixture.typeName.empty()) {
            fixture.typeName =
                mvr::gdtf_import_matching::SelectFallbackFixtureTypeName(
                    rawFixtureNodeName, rawGdtfSpec);
          }
          if (metadata.hasProperties) {
            if (metadata.weightKg > 0.0f)
              fixture.weightKg = metadata.weightKg;
            if (metadata.powerW > 0.0f)
              fixture.powerConsumptionW = metadata.powerW;
            fixture.physicalPropertiesSource =
                FixturePhysicalPropertiesSource::Gdtf;
            fixture.physicalPropertiesDirty = false;
          }
        }
        if (fixture.weightKg <= 0.0f && hasLegacyWeight) {
          fixture.weightKg = legacyWeightKg;
          fixture.physicalPropertiesSource =
              FixturePhysicalPropertiesSource::LegacyMvrFixtureNode;
        }
        if (fixture.powerConsumptionW <= 0.0f && hasLegacyPowerConsumption) {
          fixture.powerConsumptionW = legacyPowerConsumptionW;
          fixture.physicalPropertiesSource =
              FixturePhysicalPropertiesSource::LegacyMvrFixtureNode;
        }
        fixture.physicalPropertiesDirty = false;

        ReadFixtureCategoryFromUserData(node, fixture);
        const std::string fixtureTypeInfoKey = buildFixtureTypeInfoKey(
            rawGdtfSpec, fixture.gdtfMode, fixture.typeName);
        auto rootTypeInfoIt =
            rootFixtureTypeInfoByKey.find(fixtureTypeInfoKey);
        if (rootTypeInfoIt != rootFixtureTypeInfoByKey.end()) {
          if (!rootTypeInfoIt->second.category.empty()) {
            fixture.category = rootTypeInfoIt->second.category;
            fixture.categorySource = rootTypeInfoIt->second.categorySource;
            fixture.categorySourceReason.clear();
          }
          fixture.visualColorHex = rootTypeInfoIt->second.visualColorHex;
        }
        const std::optional<GdtfDictionary::Entry> &dictionaryEntry =
            getDictionaryEntryCached(fixture.typeName);
        auto posIt = scene.positions.find(fixture.position);
        if (posIt != scene.positions.end())
          fixture.positionName = posIt->second;

        auto boolOf = [&](const char *name, bool &out) {
          tinyxml2::XMLElement *n = node->FirstChildElement(name);
          if (n && n->GetText()) {
            std::string v = n->GetText();
            out = (v == "true" || v == "1");
          }
        };

        boolOf("DMXInvertPan", fixture.dmxInvertPan);
        boolOf("DMXInvertTilt", fixture.dmxInvertTilt);

        if (tinyxml2::XMLElement *addresses =
                node->FirstChildElement("Addresses")) {
          tinyxml2::XMLElement *addr = addresses->FirstChildElement("Address");
          if (addr) {
            const char *breakAttr = addr->Attribute("break");
            int breakNum = breakAttr ? std::atoi(breakAttr) : 0;
            const char *txt = addr->GetText();
            if (txt) {
              std::string t = txt;
              std::string normalized;
              if (t.find('.') == std::string::npos) {
                int value = std::atoi(t.c_str());
                int universe = breakNum + 1;
                if (value > 512) {
                  universe += (value - 1) / 512;
                  value = (value - 1) % 512 + 1;
                }
                normalized =
                    std::to_string(universe) + "." + std::to_string(value);
              } else {
                normalized = t;
              }
              fixture.address = normalized;
            }
          }
        }

        if (tinyxml2::XMLElement *matrix = node->FirstChildElement("Matrix")) {
          if (const char *txt = matrix->GetText())
            fixture.matrixRaw = txt;
        }

        if (options.applyDictionary && dictionaryEntry &&
            !fixture.typeName.empty()) {
          int footprint = 0;
          if (!resolvedGdtfPathForFixture.empty() &&
              !fixture.gdtfMode.empty()) {
            footprint = getGdtfModeChannelCountCached(
                resolvedGdtfPathForFixture, fixture.gdtfMode);
          }
          pendingGdtfConflictByType.try_emplace(
              fixture.typeName,
              GdtfConflict{fixture.typeName, fixture.requestedFixtureName,
                           fixture.gdtfSpec, dictionaryEntry->path, "",
                           fixture.typeName, fixture.gdtfMode, footprint,
                           true});
        }

        scene.fixtures[fixture.uuid] = fixture;
      };

  // Applies Perastage TrussInfo metadata as the effective edited truss state.
  auto applyTrussInfo = [&](tinyxml2::XMLElement *info, Truss &truss,
                              bool hasGdtfMetadataAuthority) {
    if (!info)
      return;
    (void)hasGdtfMetadataAuthority;
    if (tinyxml2::XMLElement *m = info->FirstChildElement("Manufacturer"))
      if (m->GetText())
        truss.manufacturer = Trim(m->GetText());
    if (tinyxml2::XMLElement *mo = info->FirstChildElement("Model"))
      if (mo->GetText())
        truss.model = Trim(mo->GetText());
    if (tinyxml2::XMLElement *len = info->FirstChildElement("Length"))
      if (len->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(len->GetText(), parsed))
          truss.lengthMm = parsed;
      }
    if (tinyxml2::XMLElement *wid = info->FirstChildElement("Width"))
      if (wid->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(wid->GetText(), parsed))
          truss.widthMm = parsed;
        else
          truss.widthMm = 400.0f;
      }
    if (tinyxml2::XMLElement *hei = info->FirstChildElement("Height"))
      if (hei->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(hei->GetText(), parsed))
          truss.heightMm = parsed;
        else
          truss.heightMm = 400.0f;
      }
    if (tinyxml2::XMLElement *wei = info->FirstChildElement("Weight"))
      if (wei->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(wei->GetText(), parsed))
          truss.weightKg = parsed;
      }
    if (tinyxml2::XMLElement *cs = info->FirstChildElement("CrossSection"))
      if (cs->GetText())
        truss.crossSection = Trim(cs->GetText());
    if (tinyxml2::XMLElement *load = info->FirstChildElement("Load"))
      if (load->GetText()) {
        float parsed = 0.0f;
        if (TryParseFloat(Trim(load->GetText()), parsed)) {
          truss.manualLoadKg = parsed;
          truss.hasManualLoadOverride = true;
        }
      }
    if (tinyxml2::XMLElement *mf = info->FirstChildElement("ModelFile"))
      if (mf->GetText())
        truss.modelFile = mf->GetText();
    if (tinyxml2::XMLElement *hp = info->FirstChildElement("PositionName"))
      if (hp->GetText())
        truss.positionName = Trim(hp->GetText());
    if (truss.positionName.empty())
      if (tinyxml2::XMLElement *hp = info->FirstChildElement("HangPos"))
        if (hp->GetText())
          truss.positionName = Trim(hp->GetText());
    if (tinyxml2::XMLElement *rep = info->FirstChildElement("Representation"))
      if (rep->GetText())
        truss.sourceRepresentation = ParseTrussRepresentation(rep->GetText());
    if (tinyxml2::XMLElement *tk = info->FirstChildElement("TypeKey"))
      if (tk->GetText())
        truss.perastageTypeKey = Trim(tk->GetText());
    if (tinyxml2::XMLElement *ag = info->FirstChildElement("AuxGdtf"))
      if (ag->GetText())
        truss.perastageAuxGdtfArchivePath =
            RemapArchivePathIfNeeded(Trim(ag->GetText()));
  };

  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseTruss = [&](tinyxml2::XMLElement *node, const std::string &layerName,
                       const Matrix &nodeTransform,
                       const Matrix &localTransform,
                       const std::string &parentGroupUuid) {
        Truss truss;
        truss.uuid = resolveStableUuid("Truss", node, layerName, nodeTransform);
        truss.layer = layerName;
        truss.transform = nodeTransform;
        truss.localTransform = localTransform;
        truss.hasLocalTransform = true;
        truss.parentGroupUuid = parentGroupUuid;
        truss.sourceSymbolMatrix = MatrixUtils::Identity();
        truss.sourceGeometryMatrix = MatrixUtils::Identity();
        if (const char *nameAttr = node->Attribute("name"))
          truss.name = nameAttr;

        intOf(node, "UnitNumber", truss.unitNumber);
        intOf(node, "CustomId", truss.customId);
        intOf(node, "CustomIdType", truss.customIdType);

        truss.gdtfSpec = textOf(node, "GDTFSpec");
        truss.gdtfMode = textOf(node, "GDTFMode");
        truss.function = textOf(node, "Function");
        truss.position = CanonicalizeUuid(textOf(node, "Position"));
        if (truss.position.empty())
          truss.position = textOf(node, "Position");
        truss.positionName = ensurePositionEntry(truss.position);
        auto trussPosIt = legacyPositionIdToCanonical.find(truss.position);
        if (trussPosIt != legacyPositionIdToCanonical.end())
          truss.position = trussPosIt->second;

        bool gdtfLoadFailed = false;
        if (!truss.gdtfSpec.empty()) {
          truss.sourceRepresentation =
              Truss::GeometryRepresentation::PublicGdtf;
          truss.gdtfSpec = RemapArchivePathIfNeeded(truss.gdtfSpec);
          const std::string trussGdtfPath =
              resolveGdtfPathCached(truss.gdtfSpec);
          truss.gdtfSpec = normalizeGdtfSpecForScene(truss.gdtfSpec);
          Truss gdtfTruss;
          if (loadTrussDefinitionCached(trussGdtfPath, gdtfTruss)) {
            truss.modelFile = gdtfTruss.modelFile;
            if (!gdtfTruss.symbolFile.empty())
              truss.symbolFile = gdtfTruss.symbolFile;
            if (!gdtfTruss.manufacturer.empty())
              truss.manufacturer = gdtfTruss.manufacturer;
            if (!gdtfTruss.model.empty())
              truss.model = gdtfTruss.model;
            if (!gdtfTruss.name.empty() && truss.name.empty())
              truss.name = gdtfTruss.name;
            if (gdtfTruss.lengthMm > 0.0f)
              truss.lengthMm = gdtfTruss.lengthMm;
            if (gdtfTruss.widthMm > 0.0f)
              truss.widthMm = gdtfTruss.widthMm;
            if (gdtfTruss.heightMm > 0.0f)
              truss.heightMm = gdtfTruss.heightMm;
            if (gdtfTruss.weightKg > 0.0f)
              truss.weightKg = gdtfTruss.weightKg;
            if (truss.gdtfMode.empty())
              truss.gdtfMode =
                  gdtfTruss.gdtfMode.empty() ? "Default" : gdtfTruss.gdtfMode;
          } else {
            gdtfLoadFailed = true;
          }
        }

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          if (tinyxml2::XMLElement *g3d =
                  geos->FirstChildElement("Geometry3D")) {
            truss.sourceRepresentation =
                Truss::GeometryRepresentation::Geometry3D;
            const char *file = g3d->Attribute("fileName");
            if (file)
              truss.symbolFile = normalizeAndResolveGeometryFileName(file);
            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "Truss/Geometry3D", geoMatrix,
                                  true);
            truss.sourceGeometryMatrix = geoMatrix;
            if (const char *type = g3d->Attribute("geometryType"))
              truss.sourceGeometryType = Trim(type);
            truss.transform = MatrixUtils::Multiply(nodeTransform, geoMatrix);
          } else if (tinyxml2::XMLElement *sym =
                         geos->FirstChildElement("Symbol")) {
            truss.sourceRepresentation =
                Truss::GeometryRepresentation::SymbolSymdef;
            std::vector<SymdefGeometry> symGeometries;
            std::string symType;
            Matrix symMatrix = MatrixUtils::Identity();
            resolveSymdefReference(sym, symGeometries, symType, symMatrix);
            if (const char *symbolUuid = sym->Attribute("uuid"))
              truss.sourceSymbolUuid = CanonicalizeUuid(Trim(symbolUuid));
            if (const char *symdef = sym->Attribute("symdef"))
              truss.sourceSymdefUuid = Trim(symdef);
            truss.sourceSymbolMatrix = symMatrix;
            truss.sourceGeometryType = symType;
            Matrix symLocal = symMatrix;
            if (!symGeometries.empty()) {
              truss.symbolFile = normalizeAndResolveGeometryFileName(
                  symGeometries.front().file);
              symLocal = MatrixUtils::Multiply(symMatrix,
                                               symGeometries.front().transform);
            }
            truss.transform = MatrixUtils::Multiply(nodeTransform, symLocal);
            ++trussSymbolSymdefPreservedCount;
            const std::string symdefKey = truss.sourceSymdefUuid.empty()
                                              ? std::string{"(empty)"}
                                              : truss.sourceSymdefUuid;
            ++trussSymbolSymdefPreservedBySymdef[symdefKey];
          }
        }

        const bool hasGdtfMetadataAuthority =
            !truss.gdtfSpec.empty() && !gdtfLoadFailed;

        auto rootTrussInfoIt = rootTrussInfoByUuid.find(truss.uuid);
        if (rootTrussInfoIt != rootTrussInfoByUuid.end()) {
          applyTrussInfo(rootTrussInfoIt->second, truss,
                         hasGdtfMetadataAuthority);
        } else if (tinyxml2::XMLElement *ud =
                       node->FirstChildElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            if (tinyxml2::XMLElement *info =
                    data->FirstChildElement("TrussInfo")) {
              applyTrussInfo(info, truss, hasGdtfMetadataAuthority);
              break;
            }
          }
        }

        auto manifestIt = perastageInstanceToTypeKey.find(truss.uuid);
        if (manifestIt != perastageInstanceToTypeKey.end()) {
          truss.perastageTypeKey = manifestIt->second;
          auto typeIt = perastageTypeToGdtfPath.find(truss.perastageTypeKey);
          if (typeIt != perastageTypeToGdtfPath.end()) {
            truss.perastageAuxGdtfArchivePath = typeIt->second;
            fs::path auxPath =
                scene.basePath.empty()
                                   ? PathUtils::PathFromUtf8(typeIt->second)
                    : PathUtils::PathFromUtf8(scene.basePath) /
                          PathUtils::PathFromUtf8(typeIt->second);
            Truss sidecar;
            if (LoadTrussDefinition(ToString(auxPath.u8string()), sidecar)) {
              if (truss.manufacturer.empty())
                truss.manufacturer = sidecar.manufacturer;
              if (truss.model.empty())
                truss.model = sidecar.model;
              if (truss.lengthMm <= 0.0f)
                truss.lengthMm = sidecar.lengthMm;
              if (truss.widthMm <= 0.0f)
                truss.widthMm = sidecar.widthMm;
              if (truss.heightMm <= 0.0f)
                truss.heightMm = sidecar.heightMm;
              if (truss.weightKg <= 0.0f)
                truss.weightKg = sidecar.weightKg;
            }
          }
        }

        const fs::path resolvedSymbolPath =
            ResolveSceneRelativePath(scene.basePath, truss.symbolFile);
        const bool symbolRenderable =
            IsRenderableTrussGeometry(truss.symbolFile);
        std::error_code symbolExistsEc;
        const bool symbolExists =
            symbolRenderable &&
            fs::exists(resolvedSymbolPath, symbolExistsEc) && !symbolExistsEc;
        if (!symbolExists) {
          std::ostringstream reason;
          if (truss.symbolFile.empty()) {
            reason << "symbolFile is empty";
          } else if (!symbolRenderable) {
            reason << "symbolFile extension is not .3ds/.glb";
          } else {
            reason << "symbolFile does not exist on disk (checked path='"
                   << ToString(resolvedSymbolPath.u8string()) << "')";
          }
          if (gdtfLoadFailed)
            reason << "; LoadTrussDefinition(gdtfSpec) returned false";

          std::ostringstream msg;
          msg << "MVR import truss fallback to dummy box: "
              << DescribeTrussForLog(truss) << ". Reason: " << reason.str();
          LogMessage(Logger::Level::Warn, msg.str());
        }

        scene.trusses[truss.uuid] = truss;
      };

  // Parses a Support XML node into scene data while preserving group-local
  // transforms.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseSupport = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName,
                         const Matrix &nodeTransform,
                         const Matrix &localTransform,
          const std::string &parentGroupUuid) {
        Support support;
        support.uuid =
            resolveStableUuid("Support", node, layerName, nodeTransform);
        support.layer = layerName;
        support.transform = nodeTransform;
        support.localTransform = localTransform;
        support.hasLocalTransform = true;
        support.parentGroupUuid = parentGroupUuid;

        if (const char *nameAttr = node->Attribute("name"))
          support.name = nameAttr;

        tinyxml2::XMLElement *childList = node->FirstChildElement("ChildList");
        auto readText = [&](const char *name) -> std::string {
          tinyxml2::XMLElement *parent = childList ? childList : node;
          if (!parent)
            return {};
          if (tinyxml2::XMLElement *el = parent->FirstChildElement(name)) {
            if (const char *txt = el->GetText())
              return Trim(txt);
          }
          return {};
        };

        support.gdtfSpec = RemapArchivePathIfNeeded(readText("GDTFSpec"));
        const std::string resolvedSupportGdtfPath =
            ResolveGdtfPath(scene.basePath, support.gdtfSpec);
        if (!support.gdtfSpec.empty()) {
          const std::string supportGdtfPath = resolvedSupportGdtfPath.empty()
                                                  ? support.gdtfSpec
                                                  : resolvedSupportGdtfPath;
          support.gdtfSpec = ToSceneRelativePathIfPossible(
              scene.basePath, PathUtils::PathFromUtf8(supportGdtfPath));
        }
        support.gdtfMode = readText("GDTFMode");
        support.function = readText("Function");
        support.hoistFunction = NormalizeHoistFunction(support.function);
        std::string chainText = readText("ChainLength");
        if (!chainText.empty()) {
          float parsed = 0.0f;
          if (TryParseFloat(chainText, parsed))
            support.chainLength = parsed;
          else
            support.chainLength = 0.0f;
        }

        support.position = CanonicalizeUuid(readText("Position"));
        if (support.position.empty())
          support.position = readText("Position");
        support.positionName = ensurePositionEntry(support.position);
        auto supportPosIt = legacyPositionIdToCanonical.find(support.position);
        if (supportPosIt != legacyPositionIdToCanonical.end())
          support.position = supportPosIt->second;

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          for (tinyxml2::XMLElement *g3d =
                   geos->FirstChildElement("Geometry3D");
               g3d; g3d = g3d->NextSiblingElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (!file)
              continue;
            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "Support/Geometry3D",
                                  geoMatrix, true);
            appendGeometryInstance(support.geometries, Trim(file), geoMatrix,
                                   BuildSceneObjectGeometryInstanceKey(
                                       support.uuid, "support-geometry3d",
                                       support.geometries.size()));
          }

          size_t symbolIndex = 0;
          for (tinyxml2::XMLElement *sym = geos->FirstChildElement("Symbol");
               sym; sym = sym->NextSiblingElement("Symbol"), ++symbolIndex) {
            std::vector<SymdefGeometry> symGeometries;
            Matrix symMatrix = MatrixUtils::Identity();
            std::string symGeometryType;
            const std::string sourceSymbolUuid =
                Trim(sym->Attribute("uuid") ? sym->Attribute("uuid") : "");
            const std::string sourceSymdefUuid =
                Trim(sym->Attribute("symdef") ? sym->Attribute("symdef") : "");
            resolveSymdefReference(sym, symGeometries, symGeometryType,
                                   symMatrix);
            size_t symGeometryIndex = 0;
            for (const auto &geo : symGeometries) {
              appendGeometryInstance(
                  support.geometries, geo.file,
                  MatrixUtils::Multiply(symMatrix, geo.transform),
                  BuildSceneObjectGeometryInstanceKey(
                      support.uuid, "support-symbol", symbolIndex,
                      sourceSymdefUuid + "/" +
                          std::to_string(symGeometryIndex)),
                  sourceSymbolUuid, sourceSymdefUuid);
              ++symGeometryIndex;
            }
          }
        }
        if (!support.geometries.empty())
          support.modelFile = support.geometries.front().modelFile;

        auto rootHoistInfoIt = rootHoistInfoByUuid.find(support.uuid);
        if (rootHoistInfoIt != rootHoistInfoByUuid.end())
          ReadSupportHoistInfoElement(rootHoistInfoIt->second, support);
        else
          ReadSupportHoistInfoFromUserData(node, support);
        ApplySupportHoistInfoDefaults(support);
        auto posIt = scene.positions.find(support.position);
        if (posIt != scene.positions.end())
          support.positionName = posIt->second;

        scene.supports[support.uuid] = support;
      };

  // Parses a SceneObject XML node into scene data while preserving group-local
  // transforms.
  std::function<void(tinyxml2::XMLElement *, const std::string &,
                     const Matrix &, const Matrix &, const std::string &)>
      parseSceneObj = [&](tinyxml2::XMLElement *node,
                          const std::string &layerName,
                          const Matrix &nodeTransform,
                          const Matrix &localTransform,
                          const std::string &parentGroupUuid) {
        SceneObject obj;
        obj.uuid =
            resolveStableUuid("SceneObject", node, layerName, nodeTransform);
        obj.layer = layerName;
        obj.transform = nodeTransform;
        obj.localTransform = localTransform;
        obj.hasLocalTransform = true;
        obj.parentGroupUuid = parentGroupUuid;
        if (const char *nameAttr = node->Attribute("name"))
          obj.name = nameAttr;
        fixtureIdOf(node, obj.fixtureIdText, obj.fixtureIdNumeric);

        std::string geometryType;
        std::unordered_map<std::string, std::string>
            primitiveModelRefByArchiveFile;

        for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData"); ud;
             ud = ud->NextSiblingElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            const std::string provider = ToLowerCopy(
                Trim(data->Attribute("provider") ? data->Attribute("provider")
                                                 : ""));
            if (provider != "perastage")
              continue;
            if (tinyxml2::XMLElement *map =
                    data->FirstChildElement("PrimitiveGeometryMap")) {
              for (tinyxml2::XMLElement *entry =
                       map->FirstChildElement("Entry");
                   entry; entry = entry->NextSiblingElement("Entry")) {
                const char *fileName = entry->Attribute("fileName");
                const char *modelRef = entry->Attribute("perastageModelRef");
                if (!modelRef)
                  modelRef = entry->Attribute("modelRef");
                if (!fileName || !modelRef)
                  continue;
                primitiveModelRefByArchiveFile[ToLowerCopy(Trim(fileName))] =
                    Trim(modelRef);
              }
            }
          }
        }

        if (const char *typeAttr = node->Attribute("geometryType"))
          geometryType = Trim(typeAttr);

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          for (tinyxml2::XMLElement *g3d =
                   geos->FirstChildElement("Geometry3D");
               g3d; g3d = g3d->NextSiblingElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (!file)
              continue;

            if (const char *type = g3d->Attribute("geometryType"))
              geometryType = Trim(type);

            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "SceneObject/Geometry3D",
                                  geoMatrix, true);
            std::string fileName = Trim(file);
            const std::string rawSceneObjectUuid = Trim(
                node->Attribute("uuid") ? node->Attribute("uuid") : "");
            const std::string normalizedFileName = ToLowerCopy(fileName);
            const std::string rootPrimitiveKey =
                obj.uuid + "|" + normalizedFileName;
            auto rootMappedModelRefIt =
                rootPrimitiveModelRefsBySceneObjectAndFile.find(
                    rootPrimitiveKey);
            if (rootMappedModelRefIt ==
                    rootPrimitiveModelRefsBySceneObjectAndFile.end() &&
                !rawSceneObjectUuid.empty()) {
              rootMappedModelRefIt =
                  rootPrimitiveModelRefsBySceneObjectAndFile.find(
                      rawSceneObjectUuid + "|" + normalizedFileName);
            }
            auto mappedModelRefIt =
                primitiveModelRefByArchiveFile.find(ToLowerCopy(fileName));
            const std::string *mappedModelRef = nullptr;
            if (rootMappedModelRefIt !=
                    rootPrimitiveModelRefsBySceneObjectAndFile.end() &&
                !rootMappedModelRefIt->second.empty()) {
              if (rootMappedModelRefIt->second.size() > 1) {
                LogMessage(Logger::Level::Warn,
                           "MVR import found ambiguous root "
                           "PrimitiveGeometryMap entries for SceneObject " +
                               obj.uuid + " and file " + fileName +
                               "; using the first entry");
              }
              mappedModelRef = &rootMappedModelRefIt->second.front();
            } else if (mappedModelRefIt !=
                       primitiveModelRefByArchiveFile.end()) {
              mappedModelRef = &mappedModelRefIt->second;
            }
            if (mappedModelRef) {
              GeometryInstance instance;
              instance.modelFile = *mappedModelRef;
              instance.instanceKey = BuildSceneObjectGeometryInstanceKey(
                  obj.uuid, "geometry3d", obj.geometries.size());
              instance.localTransform = geoMatrix;
              obj.geometries.push_back(std::move(instance));
            } else {
              const std::string instanceKey =
                  BuildSceneObjectGeometryInstanceKey(obj.uuid, "geometry3d",
                                                      obj.geometries.size());
              appendGeometryInstance(obj.geometries, fileName, geoMatrix,
                                     instanceKey);
            }
          }

          size_t symbolIndex = 0;
          for (tinyxml2::XMLElement *sym = geos->FirstChildElement("Symbol");
               sym; sym = sym->NextSiblingElement("Symbol"), ++symbolIndex) {
            std::vector<SymdefGeometry> symGeometries;
            Matrix symMatrix = MatrixUtils::Identity();
            std::string symGeometryType;
            const std::string sourceSymbolUuid =
                Trim(sym->Attribute("uuid") ? sym->Attribute("uuid") : "");
            const std::string sourceSymdefUuid =
                Trim(sym->Attribute("symdef") ? sym->Attribute("symdef") : "");
            resolveSymdefReference(sym, symGeometries, symGeometryType,
                                   symMatrix);
            if (!symGeometryType.empty())
              geometryType = symGeometryType;

            size_t symGeometryIndex = 0;
            for (const auto &geo : symGeometries) {
              Matrix localTransform =
                  MatrixUtils::Multiply(symMatrix, geo.transform);
              const std::string instanceKey =
                  BuildSceneObjectGeometryInstanceKey(
                  obj.uuid, "symbol", symbolIndex,
                      sourceSymdefUuid + "/" +
                          std::to_string(symGeometryIndex));
              appendGeometryInstance(obj.geometries, geo.file, localTransform,
                                     instanceKey, sourceSymbolUuid,
                                     sourceSymdefUuid);
              std::ostringstream geometryLog;
              geometryLog << "SceneObject geometry resolved: sceneObject='"
                          << obj.name << "' sceneUuid=" << obj.uuid
                          << " symbolUuid=" << sourceSymbolUuid
                          << " symdef=" << sourceSymdefUuid
                          << " file=" << geo.file
                          << " instanceKey=" << instanceKey
                          << " localTransform="
                          << MatrixUtils::FormatMatrix(localTransform);
              LogMessage(Logger::Level::Debug, geometryLog.str());
              if (!geo.geometryType.empty())
                geometryType = geo.geometryType;
              ++symGeometryIndex;
            }
          }
        }

        if (!obj.geometries.empty()) {
          obj.modelFile = obj.geometries.front().modelFile;
          obj.transform = nodeTransform;
        }

        std::ostringstream importedLog;
        importedLog << "Imported SceneObject " << obj.uuid << " with "
                    << obj.geometries.size() << " geometry parts";
        LogMessage(Logger::Level::Debug, importedLog.str());

        auto typeLower = geometryType;
        std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (typeLower == "support") {
          Support support;
          support.uuid = obj.uuid;
          support.name = obj.name;
          support.layer = obj.layer;
          support.transform = obj.transform;
          support.localTransform = obj.localTransform;
          support.hasLocalTransform = obj.hasLocalTransform;
          support.parentGroupUuid = obj.parentGroupUuid;
          support.modelFile = obj.modelFile;
          support.geometries = obj.geometries;
          for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData");
               ud; ud = ud->NextSiblingElement("UserData")) {
            for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data");
                 data; data = data->NextSiblingElement("Data")) {
              const std::string provider = ToLowerCopy(
                  Trim(data->Attribute("provider") ? data->Attribute("provider")
                                                   : ""));
              if (provider != "perastage")
                continue;
              if (tinyxml2::XMLElement *info =
                      data->FirstChildElement("SupportInfo")) {
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("GDTFSpec");
                    n && n->GetText()) {
                  support.gdtfSpec =
                      RemapArchivePathIfNeeded(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("GDTFMode");
                    n && n->GetText()) {
                  support.gdtfMode = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("Function");
                    n && n->GetText()) {
                  support.function = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("HoistFunction");
                    n && n->GetText()) {
                  support.hoistFunction =
                      NormalizeHoistFunction(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("ChainLength");
                    n && n->GetText()) {
                  float parsed = 0.0f;
                  if (TryParseFloat(Trim(n->GetText()), parsed))
                    support.chainLength = parsed;
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("Position");
                    n && n->GetText()) {
                  support.position = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n =
                        info->FirstChildElement("PositionName");
                    n && n->GetText()) {
                  support.positionName = Trim(n->GetText());
                }
                LogMessage(Logger::Level::Info,
                           "MVR import reconstructed Support from SceneObject "
                           "fallback uuid='" +
                               support.uuid + "'");
              }
            }
          }
          if (!support.position.empty()) {
            auto posIt = legacyPositionIdToCanonical.find(support.position);
            if (posIt != legacyPositionIdToCanonical.end())
              support.position = posIt->second;
            support.positionName = ensurePositionEntry(support.position);
          } else if (!support.positionName.empty()) {
            for (const auto &[positionUuid, positionName] : scene.positions) {
              if (positionName == support.positionName) {
                support.position = positionUuid;
                break;
              }
            }
          }
          auto rootHoistInfoIt = rootHoistInfoByUuid.find(support.uuid);
          if (rootHoistInfoIt != rootHoistInfoByUuid.end())
            ReadSupportHoistInfoElement(rootHoistInfoIt->second, support);
          else
            ReadSupportHoistInfoFromUserData(node, support);
          ApplySupportHoistInfoDefaults(support);
          scene.supports[support.uuid] = support;
        } else {
          scene.sceneObjects[obj.uuid] = obj;
        }
      };

  tinyxml2::XMLElement *layersNode = sceneNode->FirstChildElement("Layers");
  if (!layersNode)
    return true;

  std::function<int(tinyxml2::XMLElement *)> countImportSceneNodes =
      [&](tinyxml2::XMLElement *childList) {
        if (!childList)
          return 0;
        int count = 0;
        for (tinyxml2::XMLElement *child = childList->FirstChildElement();
             child; child = child->NextSiblingElement()) {
          const char *name = child->Name();
          if (!name)
            continue;
          const std::string nodeName = name;
          if (nodeName == "Fixture" || nodeName == "Truss" ||
              nodeName == "Support" || nodeName == "SceneObject" ||
              nodeName == "GroupObject") {
            ++count;
          }
          if (tinyxml2::XMLElement *inner =
                  child->FirstChildElement("ChildList"))
            count += countImportSceneNodes(inner);
        }
        return count;
      };

  int totalImportNodes = 0;
  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    totalImportNodes += countImportSceneNodes(cl);
  }
  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    totalImportNodes +=
        countImportSceneNodes(layer->FirstChildElement("ChildList"));
  }

  int importedNodes = 0;
  auto reportNodeProgress = [&](const char *nodeKind) {
    ++importedNodes;
    if (totalImportNodes <= 0)
      return;
    constexpr int kReportEveryNodes = 25;
    if (importedNodes == 1 || importedNodes == totalImportNodes ||
        importedNodes % kReportEveryNodes == 0) {
      reportProgress(std::string("Importing scene objects (") + nodeKind + ")",
                     importedNodes, totalImportNodes);
    }
  };

  parseChildList = [&](tinyxml2::XMLElement *cl, const std::string &layerName,
                       const Matrix &parentTransform,
                       const std::string &parentGroupUuid) {
    for (tinyxml2::XMLElement *child = cl->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const char *name = child->Name();
      if (!name)
        continue;

      Matrix local = MatrixUtils::Identity();
      parseMatrixOrIdentity(child, "Matrix", std::string("Child/") + name,
                            local, true);
      Matrix nodeTransform = MatrixUtils::Multiply(parentTransform, local);

      std::string nodeName = name;
      if (nodeName == "Fixture") {
        parseFixture(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Fixture");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Fixture,
               referenceUuidForNode("Fixture", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "Truss") {
        parseTruss(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Truss");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Truss,
               referenceUuidForNode("Truss", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "Support") {
        parseSupport(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("Support");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Support,
               referenceUuidForNode("Support", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "SceneObject") {
        parseSceneObj(child, layerName, nodeTransform, local, parentGroupUuid);
        reportNodeProgress("SceneObject");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::SceneObject,
               referenceUuidForNode("SceneObject", child, layerName,
                                    nodeTransform)});
        }
      } else if (nodeName == "GroupObject") {
        GroupObject group;
        group.uuid =
            resolveStableUuid("GroupObject", child, layerName, nodeTransform);
        group.layer = layerName;
        group.transform = nodeTransform;
        group.localTransform = local;
        group.parentGroupUuid = parentGroupUuid;
        if (const char *nameAttr = child->Attribute("name"))
          group.name = nameAttr;
        scene.groupObjects[group.uuid] = group;
        reportNodeProgress("GroupObject");
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::GroupObject, group.uuid});
        }
        ++preservedGroupObjectCount;
        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseChildList(inner, layerName, nodeTransform, group.uuid);
        continue;
      }

      if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
        parseChildList(inner, layerName, nodeTransform, parentGroupUuid);
    }
  };
  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    parseChildList(cl, DEFAULT_LAYER_NAME, MatrixUtils::Identity(), "");
  }

  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    const char *layerName = layer->Attribute("name");
    std::string layerStr = layerName ? layerName : "";
    if (!IsValidUtf8(layerStr)) {
      const auto repairedLayerName = RepairWindows1252AsUtf8(layerStr);
      if (repairedLayerName) {
        LogMessage(Logger::Level::Warn,
                   "Repaired legacy Windows-1252 layer name bytes at Layer uuid=" +
                       std::string(layer->Attribute("uuid") ? layer->Attribute("uuid") : "") +
                       " offset=" + std::to_string(ValidateUtf8(layerStr).errorOffset));
        layerStr = *repairedLayerName;
      } else {
        LogMessage(Logger::Level::Error,
                   "Rejected invalid UTF-8 layer name at Layer uuid=" +
                       std::string(layer->Attribute("uuid") ? layer->Attribute("uuid") : "") +
                       " offset=" + std::to_string(ValidateUtf8(layerStr).errorOffset));
        layerStr.clear();
      }
    }
    bool isDefaultLayer = layerStr.empty();

    tinyxml2::XMLElement *childList = layer->FirstChildElement("ChildList");
    if (childList)
      parseChildList(childList, isDefaultLayer ? DEFAULT_LAYER_NAME : layerStr,
                     MatrixUtils::Identity(), "");

    if (!isDefaultLayer) {
      Layer l;
      const char *uuidAttr = layer->Attribute("uuid");
      if (uuidAttr)
        l.uuid = uuidAttr;
      l.name = layerStr;
      auto colorByUuid = layerColorByUuid.find(CanonicalizeUuid(l.uuid));
      if (colorByUuid != layerColorByUuid.end()) {
        l.color = colorByUuid->second;
      } else {
        auto colorByName = layerColorByName.find(l.name);
        if (colorByName != layerColorByName.end()) {
          l.color = colorByName->second;
        } else if (tinyxml2::XMLElement *colorNode =
                       layer->FirstChildElement("Color")) {
          if (const char *txt = colorNode->GetText()) {
            const std::string legacyColor = Trim(txt);
            l.color =
                isHexRgb(legacyColor) ? legacyColor : CieToHex(legacyColor);
          }
        }
      }
      scene.layers[l.uuid] = l;
    }
  }

  const auto reconcileResult = layerdomain::ReconcileLegacyLayers(scene);
  if (reconcileResult.status == layerdomain::LayerStatus::Success) {
    LogMessage(Logger::Level::Warn,
               "Reconciled legacy layer metadata: " + reconcileResult.message);
  }

  if (preservedGroupObjectCount > 0) {
    LogMessage(Logger::Level::Info,
               "MVR import preserved GroupObject count=" +
                   std::to_string(preservedGroupObjectCount));
  }
  const std::size_t repairedGroupLayerCount =
      scene_grouping::SynchronizeGroupObjectLayerOwnership(scene);
  if (repairedGroupLayerCount > 0) {
    LogMessage(Logger::Level::Info,
               "MVR import repaired " +
                   std::to_string(repairedGroupLayerCount) +
                   " GroupObject child layer assignment(s)");
  }

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
            std::optional<CredentialStore::Credentials> activeCredentials =
                CredentialStore::Load();
            auto requestCredentials = [&]() -> bool {
              const std::string initialUser = activeCredentials
                                                  ? activeCredentials->username
                                                  : std::string();
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
              CredentialStore::Save(entered);
              activeCredentials = entered;
              return true;
            };

            wxString cookieFileWx =
                wxFileName::CreateTempFileName("gdtf_mvr_import_");
            if (wxFileExists(cookieFileWx))
              wxRemoveFile(cookieFileWx);
            const std::string cookieFile = cookieFileWx.ToStdString();
            long loginHttpCode = 0;
            bool loginOk = activeCredentials.has_value() &&
                           GdtfLogin(activeCredentials->username,
                                     activeCredentials->password, cookieFile,
                                     loginHttpCode);
            if (!loginOk || loginHttpCode == 401 || loginHttpCode == 403) {
              if (requestCredentials()) {
                loginOk = GdtfLogin(activeCredentials->username,
                                    activeCredentials->password, cookieFile,
                                    loginHttpCode);
              }
            }

            if (loginOk && loginHttpCode == 200) {
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
                  return wxColour(30, 120, 60);
                case DownloadRowState::Fallback:
                  return wxColour(150, 100, 0);
                case DownloadRowState::Canceled:
                  return wxColour(130, 130, 130);
                case DownloadRowState::Downloading:
                  return wxColour(20, 80, 160);
                case DownloadRowState::Pending:
                default:
                  return wxColour(80, 80, 80);
                }
              };

              wxStaticText *summaryText =
                  new wxStaticText(&downloadInfoDialog, wxID_ANY,
                                   _("Selected fixture types for download"));
              wxFont summaryFont = summaryText->GetFont();
              summaryFont.SetWeight(wxFONTWEIGHT_BOLD);
              summaryText->SetFont(summaryFont);
              infoSizer->Add(summaryText, 0, wxLEFT | wxRIGHT | wxTOP, 8);
              wxStaticText *progressPhaseText = new wxStaticText(
                  &downloadInfoDialog, wxID_ANY, _("Preparing download queue..."));
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
              downloadInfoList->InsertColumn(3, _("Progress"), wxLIST_FORMAT_LEFT,
                                             220);
              downloadInfoList->InsertColumn(4, _("Details"), wxLIST_FORMAT_LEFT,
                                             180);
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
                    wxString::Format(_("%d/%zu processed  |  %d downloaded  |  %d fallback  |  %d canceled"),
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
              long listHttpCode = 0;
              reportProgress("Downloading selected GDTFs: loading catalog...");

              GdtfCatalogService catalogService;
              const std::string refreshNowUtc =
                  wxDateTime::UNow().FormatISOCombined(' ').ToStdString();
              const GdtfCatalogRefreshResult catalogResult =
                  catalogService.RefreshCatalogIfStale(
                      [&](std::string &onlineListData) {
                        return GdtfGetList(cookieFile, onlineListData,
                                           &listHttpCode) &&
                               listHttpCode == 200;
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
              std::string catalogFailureReason;
              if (!listPayload.empty()) {
                catalogEntries = ParseGdtfCatalogEntries(listPayload);
              }

              if (catalogEntries.empty()) {
                reportProgress("[INFO] Cached catalog did not provide usable "
                               "entries; forcing online refresh.");
                const GdtfCatalogRefreshResult forcedCatalogResult =
                    catalogService.RefreshCatalogIfStale(
                        [&](std::string &onlineListData) {
                          return GdtfGetList(cookieFile, onlineListData,
                                             &listHttpCode) &&
                                 listHttpCode == 200;
                        },
                        refreshNowUtc, 0);
                if (forcedCatalogResult.snapshot) {
                  listPayload = forcedCatalogResult.snapshot->listData;
                  catalogEntries = ParseGdtfCatalogEntries(listPayload);
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

              if (catalogEntries.empty()) {
                const std::string preview = listPayload.substr(
                    0, std::min<size_t>(listPayload.size(), 180));
                catalogFailureReason =
                    wxString::Format(
                        "Catalog fetch/parsing failed (HTTP %ld, bytes=%zu)",
                                     listHttpCode, listPayload.size())
                        .ToStdString();
                reportProgress("[WARN] " + catalogFailureReason);
                if (!preview.empty()) {
                  reportProgress("[WARN] GDTF catalog payload preview: " +
                                 preview);
                }
              }

              if (!catalogEntries.empty()) {
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
                  const auto bestMatch =
                      gdtf_catalog_matcher::SelectBestDownloadMatch(
                          req.requestedFixtureName, req.type, req.modeName,
                          req.manufacturer, req.footprint, catalogEntries);

                  if (!bestMatch.found || bestMatch.rid.empty()) {
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
                  long downloadHttpCode = 0;
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
                  updateStatusRow(req.type, selectedFixtureName, _("Downloading"),
                                  "0 B / ? B", _("Fetching fixture package"),
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
                  if (GdtfDownload(
                          bestMatch.rid, filePath, cookieFile, downloadHttpCode,
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
                                   [&]() { return cancelRequested.load(); }) &&
                      downloadHttpCode == 200) {
                    selectedPathByType[req.type] = filePath;
                    if (!bestMatch.modeName.empty())
                      selectedModeByType[req.type] = bestMatch.modeName;
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
              wxMessageBox(_("Login failed. Verify credentials in Preferences."),
                           _("GDTF Share login"), wxOK | wxICON_WARNING);
            }
            if (wxFileExists(cookieFileWx))
              wxRemoveFile(cookieFileWx);
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
            f.gdtfSpec = ToSceneRelativePathIfPossible(
                  scene.basePath,
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
            f.gdtfSpec = ToSceneRelativePathIfPossible(
                scene.basePath,
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
        resolvedCategoryPath =
            ResolveScenePathForRead(scene.basePath, resolvedCategoryPath);
      } else {
        resolvedCategoryPath =
            ResolveScenePathForRead(scene.basePath, fixture.gdtfSpec);
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

  std::string summary =
      "Parsed scene: " + std::to_string(scene.fixtures.size()) + " fixtures, " +
      std::to_string(scene.trusses.size()) + " trusses, " +
      std::to_string(scene.supports.size()) + " supports, " +
      std::to_string(scene.sceneObjects.size()) + " objects";
  LogMessage(summary);
  importResult.fixtureUuidRemap = fixtureUuidRemap;
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

// Imports and registers an MVR file with explicit import behavior options.
bool MvrImporter::ImportAndRegister(const std::string &filePath,
                                    const MvrImportOptions &options,
                                    ProgressCallback progressCallback) {
  MvrImporter importer;
  MvrImportResult importResult;
  const bool imported = importer.ImportFromFile(filePath, importResult,
                                                MvrImportMode::ReplaceProject,
                                                options, progressCallback);
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
