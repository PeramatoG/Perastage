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
#include "configmanager.h"
#include "dummyprofilelibrary.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "gdtf_fixture_category.h"
#include "matrixutils.h"
#include "sceneobject.h"
#include "support.h"
#include "groupobject.h"
#include "uuidutils.h"
#include "trussloader.h"

#include "consolepanel.h"
#include "logger.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream> // Required for std::ofstream
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
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
#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

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


static std::string ToLowerCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

  fs::path path = fs::u8path(normalized);
  std::string stem = Trim(path.stem().string());
  std::string ext = ToLowerAscii(path.extension().string());
  if (ext.empty())
    ext = ".gdtf";
  return ToLowerAscii(stem + ext);
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

static std::string ResolveExistingGdtfMode(const std::string &gdtfPath,
                                           const std::string &requestedMode,
                                           std::optional<int> channelCountHint) {
  const std::vector<std::string> modes = GetGdtfModes(gdtfPath);
  if (modes.empty())
    return requestedMode;

  const std::string normalizedRequested = ToLowerAscii(Trim(requestedMode));
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
      const int modeChannelCount = GetGdtfModeChannelCount(gdtfPath, mode);
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
}

static bool IsNearlyEqualRelative(float a, float b, float relEps) {
  if (!std::isfinite(a) || !std::isfinite(b))
    return false;
  const float scale = std::max(std::max(std::fabs(a), std::fabs(b)), 1.0e-6f);
  return std::fabs(a - b) <= relEps * scale;
}

static bool IsGeometryMatrixContext(const std::string &contextTag) {
  if (contextTag == "SceneObject/Geometry3D" || contextTag == "Truss/Geometry3D" ||
      contextTag == "Symbol") {
    return true;
  }
  return contextTag.find("Geometry3D") != std::string::npos;
}

static std::string JoinMatrixContextCounts(const std::unordered_map<std::string, size_t> &counts) {
  if (counts.empty())
    return "none";

  std::vector<std::pair<std::string, size_t>> sorted(counts.begin(), counts.end());
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

static bool TryParseFloat(const std::string &text, float &out) {
  if (text.empty())
    return false;

  const auto first =
      std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
      });
  if (first == text.end())
    return false;
  const auto last =
      std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

  float value = 0.0f;
  auto begin = trimmed.data();
  auto end = trimmed.data() + trimmed.size();
  auto result = std::from_chars(begin, end, value);
  if (result.ec == std::errc{} && result.ptr == end) {
    out = value;
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
  fs::path path = fs::u8path(pathText);
  if (path.is_absolute() || basePath.empty())
    return path;
  return fs::u8path(basePath) / path;
}

static std::string ToSceneRelativePathIfPossible(const std::string &basePath,
                                                 const fs::path &candidatePath) {
  if (candidatePath.empty())
    return {};

  if (basePath.empty() || !candidatePath.is_absolute())
    return ToString(candidatePath.u8string());

  std::error_code ec;
  const fs::path base = fs::weakly_canonical(fs::u8path(basePath), ec);
  if (ec)
    return ToString(candidatePath.u8string());
  const fs::path canonicalCandidate = fs::weakly_canonical(candidatePath, ec);
  if (ec)
    return ToString(candidatePath.u8string());

  fs::path relative = fs::relative(canonicalCandidate, base, ec);
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
// so we progressively try exact, extension-appended and case-insensitive matches.
static std::string ResolveGdtfPath(const std::string &baseDir,
                                   const std::string &spec) {
  const std::string normalizedSpec = NormalizeArchivePathValue(spec);
  if (normalizedSpec.empty())
    return {};

  fs::path candidate = baseDir.empty()
                           ? fs::u8path(normalizedSpec)
                           : fs::u8path(baseDir) / fs::u8path(normalizedSpec);

  const std::string candidateExt = ToLowerAscii(candidate.extension().string());
  if (candidateExt == ".gdtf" && fs::exists(candidate))
    return ToString(candidate.u8string());

  if (!candidate.has_extension()) {
    fs::path withExtension = candidate;
    withExtension += ".gdtf";
    if (fs::exists(withExtension))
      return ToString(withExtension.u8string());
  }

  std::error_code ec;
  fs::path lookupDir = baseDir.empty() ? fs::current_path(ec) : fs::u8path(baseDir);
  if (ec || !fs::exists(lookupDir))
    return ToString(candidate.u8string());

  const std::string expectedStem =
      ToLowerAscii(Trim(fs::u8path(normalizedSpec).filename().stem().string()));
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
        NormalizeGdtfLookupKey(entryPath.filename().generic_string()) == normalizedSpecKey) {
      return ToString(entryPath.u8string());
    }
  }

  return ToString(candidate.u8string());
}

static std::string DescribeTrussForLog(const Truss &truss) {
  const std::string displayName = truss.name.empty() ? "(unnamed)" : truss.name;
  std::ostringstream oss;
  oss << "uuid='" << truss.uuid << "', name='" << displayName << "', model='"
      << truss.model << "', modelFile='" << truss.modelFile << "', gdtfSpec='"
      << truss.gdtfSpec << "', symbolFile='" << truss.symbolFile << "'";
  return oss.str();
}

static Truss::GeometryRepresentation ParseTrussRepresentation(
    const std::string &value) {
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
    return c <= 0.0031308 ? 12.92 * c
                          : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
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
  os << '#' << std::uppercase << std::hex << std::setfill('0')
     << std::setw(2) << R << std::setw(2) << G << std::setw(2) << B;
  return os.str();
}

// Helper to log errors both to stderr and the application's console panel.
// Log a message to both the log file and the application's console panel.
// Console updates are queued to the GUI thread to avoid blocking.
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
      size_t keepLength =
          kMaxConsoleMessageLength > suffix.size()
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
  std::string mvrPath;
  std::string appPath;
};

static std::unordered_map<std::string, std::string>
PromptGdtfConflicts(const std::vector<GdtfConflict> &conflicts) {
  std::unordered_map<std::string, std::string> chosen;
  if (conflicts.empty())
    return chosen;

  auto selectAll = [](const std::vector<wxRadioButton *> &buttons) {
    for (wxRadioButton *button : buttons) {
      if (button)
        button->SetValue(true);
    }
  };

  wxDialog dlg(nullptr, wxID_ANY, "GDTF conflicts");
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *grid = new wxFlexGridSizer(3, 5, 5);
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "Type"));
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "MVR"));
  grid->Add(new wxStaticText(&dlg, wxID_ANY, "App"));

  std::vector<wxRadioButton *> mvrBtns;
  std::vector<wxRadioButton *> appBtns;
  for (const auto &c : conflicts) {
    grid->Add(
        new wxStaticText(&dlg, wxID_ANY, wxString::FromUTF8(c.type.c_str())));
    wxRadioButton *mvr = new wxRadioButton(
        &dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    wxRadioButton *app = new wxRadioButton(&dlg, wxID_ANY, "");
    app->SetValue(true);
    grid->Add(mvr, 0, wxALIGN_CENTER);
    grid->Add(app, 0, wxALIGN_CENTER);
    mvrBtns.push_back(mvr);
    appBtns.push_back(app);
  }

  wxBoxSizer *batchSelectionSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *selectAllMvrButton = new wxButton(&dlg, wxID_ANY, "Select all MVR");
  wxButton *selectAllAppButton = new wxButton(&dlg, wxID_ANY, "Select all App");
  selectAllAppButton->Bind(wxEVT_BUTTON,
                           [&appBtns, &selectAll](wxCommandEvent &) {
                             selectAll(appBtns);
                           });
  selectAllMvrButton->Bind(wxEVT_BUTTON,
                           [&mvrBtns, &selectAll](wxCommandEvent &) {
                             selectAll(mvrBtns);
                           });
  batchSelectionSizer->Add(selectAllMvrButton, 0, wxRIGHT, 5);
  batchSelectionSizer->Add(selectAllAppButton, 0);

  topSizer->Add(batchSelectionSizer, 0, wxLEFT | wxRIGHT | wxTOP, 10);
  topSizer->Add(grid, 1, wxALL, 10);
  topSizer->Add(dlg.CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxEXPAND | wxALL, 10);
  dlg.SetSizerAndFit(topSizer);
  dlg.CentreOnScreen();

  if (dlg.ShowModal() != wxID_OK)
    return chosen;

  for (size_t i = 0; i < conflicts.size(); ++i) {
    const auto &c = conflicts[i];
    chosen[c.type] = mvrBtns[i]->GetValue() ? c.mvrPath : c.appPath;
  }
  return chosen;
}



static void ApplySupportHoistInfoDefaults(Support &support) {
  if (support.dummyProfileId.empty() && !support.dummyPreset.empty()) {
    const auto profile = DummyProfileLibrary::FindByDisplayName(support.dummyPreset);
    if (profile.has_value())
      support.dummyProfileId = profile->id;
  }

  support.hoistFunction = NormalizeHoistFunction(
      support.hoistFunction.empty() ? support.function : support.hoistFunction);
  support.hoistDataSource = NormalizeHoistDataSource(support.hoistDataSource);
  support.motorNameSource =
      ResolveHoistFieldDataSource(support.motorNameSource, support.hoistDataSource);
  support.motorManufacturerSource = ResolveHoistFieldDataSource(
      support.motorManufacturerSource, support.hoistDataSource);
  support.motorModelSource =
      ResolveHoistFieldDataSource(support.motorModelSource, support.hoistDataSource);
  support.capacitySource =
      ResolveHoistFieldDataSource(support.capacitySource, support.hoistDataSource);
  support.weightSource =
      ResolveHoistFieldDataSource(support.weightSource, support.hoistDataSource);
  support.hoistFunctionSource = ResolveHoistFieldDataSource(
      support.hoistFunctionSource, support.hoistDataSource);
  if (support.function.empty())
    support.function = support.hoistFunction;
}

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

    if (tinyxml2::XMLElement *categoryNode = info->FirstChildElement("Category")) {
      if (const char *txt = categoryNode->GetText())
        fixture.category = GdtfFixtureCategory::NormalizeCategory(Trim(txt));
    }

    if (tinyxml2::XMLElement *sourceNode = info->FirstChildElement("CategorySource")) {
      if (const char *txt = sourceNode->GetText())
        fixture.categorySource = Trim(txt);
    }
    if (tinyxml2::XMLElement *reasonNode = info->FirstChildElement("CategoryReason")) {
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

static void ReadSupportHoistInfoFromUserData(tinyxml2::XMLElement *supportNode,
                                             Support &support) {
  for (tinyxml2::XMLElement *ud = supportNode->FirstChildElement("UserData"); ud;
       ud = ud->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      tinyxml2::XMLElement *info = data->FirstChildElement("HoistInfo");
      if (!info)
        info = data->FirstChildElement("MotorInfo"); // Legacy block name.
      if (!info)
        continue;

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
      readFloat("Load", support.loadKg);

      std::string hoistFunction = readText("RiggingPoint"); // Canonical in new schema.
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
        support.useMotorDefaults =
            !(useDefaults == "false" || useDefaults == "0" || useDefaults == "no");
      }

      const std::string dummyPreset = readText("DummyPreset");
      if (!dummyPreset.empty())
        support.dummyPreset = dummyPreset;
      const std::string dummyProfileId = readText("DummyProfileId");
      if (!dummyProfileId.empty())
        support.dummyProfileId = dummyProfileId;

      std::string source = readText("ValueSource");
      if (source.empty())
        source = readText("DataSource"); // Legacy key.
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
  }
}
bool MvrImporter::ImportFromFile(const std::string &filePath,
                                 bool promptConflicts,
                                 bool applyDictionary,
                                 ProgressCallback progressCallback) {
  pathRemap.clear();
  // Treat the incoming path as UTF-8 to preserve any non-ASCII characters
  fs::path path = fs::u8path(filePath);

  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (progressCallback)
    progressCallback("Preparing import...");

  if (!fs::exists(path)) {
    LogMessage("MVR file does not exist: " + filePath);
    return false;
  }
  if (ext != ".mvr") {
    LogMessage("MVR file has invalid extension: " + ext);
    return false;
  }

  if (progressCallback)
    progressCallback("Extracting package resources...");

  std::string tempDir = CreateTemporaryDirectory();
  std::string mvrPath = ToString(path.u8string());
  fs::path tempPath(tempDir);
  if (!ExtractMvrZip(mvrPath, tempDir)) {
    LogMessage("Failed to extract MVR file.");
    return false;
  }

  fs::path sceneFile = tempPath / "GeneralSceneDescription.xml";
  if (!fs::exists(sceneFile)) {
    // Some MVR packages may store the file with a different case.
    std::string target = "generalscenedescription.xml";
    for (const auto &entry : fs::directory_iterator(tempPath)) {
      if (entry.is_regular_file()) {
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
  if (!fs::exists(sceneFile)) {
    LogMessage("Missing GeneralSceneDescription.xml in MVR.");
    return false;
  }

  std::string scenePath = ToString(sceneFile.u8string());
  if (progressCallback)
    progressCallback("Parsing scene data...");
  return ParseSceneXml(scenePath, promptConflicts, applyDictionary, progressCallback);
}

std::string MvrImporter::NormalizeArchivePath(const std::string &archivePath) const {
  return NormalizeArchivePathValue(archivePath);
}

std::string MvrImporter::RemapArchivePathIfNeeded(const std::string &archivePath) const {
  const std::string normalized = NormalizeArchivePath(archivePath);
  auto it = pathRemap.find(normalized);
  if (it != pathRemap.end())
    return it->second;
  return archivePath;
}

std::string MvrImporter::CreateTemporaryDirectory() {
  fs::path tempBase = fs::temp_directory_path();
  for (int attempt = 0; attempt < 32; ++attempt) {
    fs::path fullPath = tempBase / ("ps_" + GenerateShortToken());
    std::error_code ec;
    if (fs::create_directory(fullPath, ec) && !ec) {
      // Return the path encoded as UTF-8 so it can safely be converted back
      // using fs::u8path or passed to wxWidgets APIs expecting UTF-8 strings.
      return ToString(fullPath.u8string());
    }
  }

  fs::path fallback = tempBase / ("ps_" + std::to_string(
                                      std::chrono::system_clock::now().time_since_epoch().count()));
  fs::create_directory(fallback);
  return ToString(fallback.u8string());
}

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
    fs::path fullPath = fs::u8path(destDir) / fs::u8path(entryName);

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
      fs::path longDir = fs::u8path(destDir) / "_long";
      std::string extension = fs::u8path(entryName).extension().string();
      std::string hashBase = std::to_string(std::hash<std::string>{}(normalizedEntryName));
      wxFileName::Mkdir(wxString::FromUTF8(ToString(longDir.u8string()).c_str()),
                        wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

      for (int suffix = 0; suffix < 64 && !output.is_open(); ++suffix) {
        std::string candidateName = hashBase;
        if (suffix > 0)
          candidateName += "_" + std::to_string(suffix);
        candidateName += extension;
        fs::path candidatePath = longDir / fs::u8path(candidateName);
        output = tryOpenOutput(candidatePath);
        if (output.is_open()) {
          fullPath = candidatePath;
          pathRemap[normalizedEntryName] = ToString((fs::path("_long") /
                                                     fs::u8path(candidateName))
                                                        .u8string());
          remapped = true;
        }
      }
    }

    if (!output.is_open()) {
      std::ostringstream msg;
      msg << "Cannot create file while extracting MVR entry. entry='" << entryName
          << "', path='" << ToString(fullPath.u8string())
          << "', pathLength=" << fullPathLength;
      const std::string loweredEntry = ToLowerAscii(normalizedEntryName);
      const bool isSceneXml =
          loweredEntry == "generalscenedescription.xml" ||
          fs::path(loweredEntry).filename().generic_string() ==
              "generalscenedescription.xml";
      if (isSceneXml) {
        LogMessage(Logger::Level::Error, msg.str() +
                                           " (required scene XML; aborting import)");
        return false;
      }

      LogMessage(Logger::Level::Warn, msg.str() +
                                       " (asset entry skipped, continuing import)");
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

// Parses GeneralSceneDescription.xml and populates fixtures and trusses into
// the scene model
bool MvrImporter::ParseSceneXml(const std::string &sceneXmlPath,
                                bool promptConflicts,
                                bool applyDictionary,
                                ProgressCallback progressCallback) {
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

  ConfigManager::Get().Reset();
  auto &scene = ConfigManager::Get().GetScene();
  scene.basePath = ToString(fs::u8path(sceneXmlPath).parent_path().u8string());

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

  auto fixtureIdOf = [&](tinyxml2::XMLElement *parent,
                         std::string &textOut,
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
    for (tinyxml2::XMLElement *data = userDataNode->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerCopy(
          Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      if (tinyxml2::XMLElement *manifest = data->FirstChildElement("TrussSidecarManifest")) {
        found = true;
        for (tinyxml2::XMLElement *type = manifest->FirstChildElement("Type"); type;
             type = type->NextSiblingElement("Type")) {
          const char *key = type->Attribute("key");
          const char *path = type->Attribute("gdtf");
          if (key && path)
            perastageTypeToGdtfPath[Trim(key)] = RemapArchivePathIfNeeded(path);
        }
        for (tinyxml2::XMLElement *inst = manifest->FirstChildElement("Instance"); inst;
             inst = inst->NextSiblingElement("Instance")) {
          const char *uuid = inst->Attribute("uuid");
          const char *key = inst->Attribute("typeKey");
          if (uuid && key)
            perastageInstanceToTypeKey[CanonicalizeUuid(Trim(uuid))] = Trim(key);
        }
      }
    }
    if (found) {
      LogMessage(Logger::Level::Info,
                 std::string("MVR import loaded Perastage sidecar manifest from ") +
                     originLabel);
    }
    return found;
  };

  const bool hasRootManifest =
      parsePerastageManifest(root->FirstChildElement("UserData"), "GeneralSceneDescription/UserData");
  if (!hasRootManifest &&
      parsePerastageManifest(sceneNode->FirstChildElement("UserData"), "legacy Scene/UserData")) {
    LogMessage(Logger::Level::Warn,
               "MVR import used legacy Scene/UserData fallback for Perastage sidecar manifest");
  }

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
        std::string seed = "mvr:legacy-position:" + rawUid + ":" + (name ? Trim(name) : "");
        const std::string generated = DeriveDeterministicUuid(seed);
        legacyPositionIdToCanonical[rawUid] = generated;
        scene.positions[generated] = name ? name : rawUid;
        LogMessage(Logger::Level::Warn,
                   "MVR import migrated non-canonical Position uuid '" + rawUid + "' -> '" +
                       generated + "'");
      } else {
        if (canonicalUid != rawUid)
          legacyPositionIdToCanonical[rawUid] = canonicalUid;
        scene.positions[canonicalUid] = name ? name : "";
      }
    }

    std::function<void(tinyxml2::XMLElement *, const Matrix &, std::vector<SymdefGeometry> &)> parseSymdefChildList;
    parseSymdefChildList = [&](tinyxml2::XMLElement *childList,
                               const Matrix &parent,
                               std::vector<SymdefGeometry> &geometries) {
      for (tinyxml2::XMLElement *child = childList ? childList->FirstChildElement() : nullptr;
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
                                   const std::string &contextTag,
                                   Matrix &out,
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

        const bool finiteNorms = std::isfinite(nu) && std::isfinite(nv) && std::isfinite(nw);
        const bool strictlyPositiveNorms = minNorm > 0.0f;
        const bool isUniformScale = IsNearlyEqualRelative(nu, nv, kUniformScaleRelativeTolerance) &&
                                    IsNearlyEqualRelative(nu, nw, kUniformScaleRelativeTolerance);
        const bool isTinyUniformGeometryScale =
            finiteNorms && strictlyPositiveNorms && maxNorm <= kTinyScaleMaxNorm &&
            isUniformScale && IsGeometryMatrixContext(contextTag);
        if (isTinyUniformGeometryScale) {
          ++matrixScaleAggregation.acceptedTinyUniformScaleCount;
          ++matrixScaleAggregation.acceptedByContext[contextTag];
          return;
        }

        const bool hasInvalidNorm = !finiteNorms || !strictlyPositiveNorms;
        const bool hasOutlierNorm = minNorm < kMinOutlierNorm || maxNorm > kMaxOutlierNorm;
        if (!hasInvalidNorm && !hasOutlierNorm)
          return;

        ++matrixScaleAggregation.suspiciousMatrixCount;
        ++matrixScaleAggregation.suspiciousByContext[contextTag];
        if (matrixScaleAggregation.suspiciousExamples.size() < kMaxSuspiciousExamples) {
          std::ostringstream oss;
          oss << contextTag << " (|u|=" << nu << ", |v|=" << nv << ", |w|=" << nw << ")";
          matrixScaleAggregation.suspiciousExamples.push_back(oss.str());
        }
      }
    }
  };

  auto normalizeGeometryFileName = [](std::string fileName) {
    fileName = Trim(fileName);
    if (fileName.empty())
      return fileName;
    return ToString(fs::u8path(fileName).u8string());
  };

  auto normalizeAndResolveGeometryFileName = [&](std::string fileName) {
    std::string normalized = normalizeGeometryFileName(std::move(fileName));
    if (normalized.empty())
      return normalized;
    std::string remapped = RemapArchivePathIfNeeded(normalized);
    fs::path remappedPath = fs::u8path(remapped);
    fs::path resolved = ResolveSceneRelativePath(scene.basePath, remapped);
    if (!remappedPath.has_extension()) {
      const std::array<std::string, 3> extensions = {".gltf", ".glb", ".3ds"};
      for (const std::string &ext : extensions) {
        fs::path candidate = resolved;
        candidate += ext;
        if (fs::exists(candidate)) {
          resolved = candidate;
          break;
        }
      }
      if (!resolved.has_extension())
        resolved += ".3ds";
    }
    return ToSceneRelativePathIfPossible(scene.basePath, resolved);
  };

  auto appendGeometryInstance = [&](std::vector<GeometryInstance> &instances,
                                    const std::string &fileName,
                                    const Matrix &localTransform) {
    std::string normalized = normalizeAndResolveGeometryFileName(fileName);
    if (normalized.empty())
      return;
    GeometryInstance instance;
    instance.modelFile = normalized;
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
  std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &, const std::string &)>
      parseChildList;

  auto ensurePositionEntry = [&](const std::string &positionId)
      -> std::string {
    if (positionId.empty())
      return {};

    auto legacyIt = legacyPositionIdToCanonical.find(positionId);
    const std::string remappedId =
        legacyIt != legacyPositionIdToCanonical.end() ? legacyIt->second : positionId;
    const std::string canonicalId = CanonicalizeUuid(remappedId);
    const std::string normalizedId = canonicalId.empty() ? remappedId : canonicalId;

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
                               const Matrix &nodeTransform) {
    const char *uuidAttr = node->Attribute("uuid");
    std::string rawUuid = uuidAttr ? Trim(uuidAttr) : std::string{};
    std::string stableUuid = CanonicalizeUuid(rawUuid);
    const std::string seed =
        buildStableIdSeed(kind, node, layerName, nodeTransform, rawUuid);

    if (stableUuid.empty()) {
      if (!rawUuid.empty()) {
        LogMessage(Logger::Level::Warn,
                   wxString::Format("MVR import: %s UUID '%s' is invalid. Applying deterministic fallback.",
                                    kind, rawUuid.c_str())
                       .ToStdString());
      }
      stableUuid = DeriveDeterministicUuid(seed);
    }

    if (usedStableUuids.contains(stableUuid)) {
      LogMessage(Logger::Level::Warn,
                 wxString::Format("MVR import: UUID collision for %s '%s'. Applying controlled fallback UUID.",
                                  kind, stableUuid.c_str())
                     .ToStdString());
      int suffix = 1;
      std::string candidate;
      do {
        candidate = DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
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

  std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &)>
      parseFixture = [&](tinyxml2::XMLElement *node,
                         const std::string &layerName,
                         const Matrix &nodeTransform) {
        Fixture fixture;
        fixture.uuid =
            resolveStableUuid("Fixture", node, layerName, nodeTransform);
        fixture.layer = layerName;
        fixture.transform = nodeTransform;

        if (const char *nameAttr = node->Attribute("name"))
          fixture.instanceName = nameAttr;

        fixtureIdOf(node, fixture.fixtureIdText, fixture.fixtureIdNumeric);
        fixture.fixtureId = fixture.fixtureIdNumeric;
        intOf(node, "UnitNumber", fixture.unitNumber);
        intOf(node, "CustomId", fixture.customId);
        intOf(node, "CustomIdType", fixture.customIdType);

        fixture.gdtfSpec = textOf(node, "GDTFSpec");
        fixture.gdtfMode = textOf(node, "GDTFMode");
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
            fixture.color = CieToHex(txt);
        }
        if (tinyxml2::XMLElement *pcNode =
                node->FirstChildElement("PowerConsumption")) {
          if (const char *txt = pcNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed))
              fixture.powerConsumptionW = parsed;
          }
        }
        if (tinyxml2::XMLElement *wNode =
                node->FirstChildElement("Weight")) {
          if (const char *txt = wNode->GetText()) {
            float parsed = 0.0f;
            if (TryParseFloat(txt, parsed))
              fixture.weightKg = parsed;
          }
        }
        if (!fixture.gdtfSpec.empty()) {
          fixture.gdtfSpec = RemapArchivePathIfNeeded(fixture.gdtfSpec);
          const std::string resolvedGdtfPath =
              ResolveGdtfPath(scene.basePath, fixture.gdtfSpec);
          const std::string gdtfPath = resolvedGdtfPath.empty()
                                           ? fixture.gdtfSpec
                                           : resolvedGdtfPath;
          fixture.gdtfSpec = ToSceneRelativePathIfPossible(
              scene.basePath, fs::u8path(gdtfPath));
          fixture.typeName = Trim(GetGdtfFixtureName(gdtfPath));
          float gw = 0.0f, gp = 0.0f;
          if (GetGdtfProperties(gdtfPath, gw, gp)) {
            if (fixture.weightKg == 0.0f)
              fixture.weightKg = gw;
            if (fixture.powerConsumptionW == 0.0f)
              fixture.powerConsumptionW = gp;
          }
        }

        ReadFixtureCategoryFromUserData(node, fixture);
        const std::string categoryKey =
            !fixture.typeName.empty() ? fixture.typeName : fixture.gdtfSpec;

        if (fixture.category.empty() && !fixture.typeName.empty()) {
          if (auto entry = GdtfDictionary::Get(fixture.typeName)) {
            fixture.category = GdtfFixtureCategory::NormalizeCategory(entry->category);
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
          const auto inferred = GdtfFixtureCategory::InferFromGdtf(
              ResolveScenePathForRead(scene.basePath, fixture.gdtfSpec));
          fixture.category = GdtfFixtureCategory::NormalizeCategory(inferred.category);
          if (fixture.category.empty())
            fixture.category = GdtfFixtureCategory::kUnknown;
          fixture.categorySource = GdtfFixtureCategory::kAutoFallbackSource;
          fixture.categorySourceReason = inferred.reason;
          if (!categoryKey.empty()) {
            categoryByTypeKey[categoryKey] =
                {fixture.category, fixture.categorySource, inferred.reason};
          }
          LogMessage(Logger::Level::Info,
                     "Auto category fallback: " + fixture.instanceName + " -> " +
                         fixture.category + " [" + inferred.reason + "]");
        } else if (!fixture.category.empty() && !categoryKey.empty()) {
          categoryByTypeKey[categoryKey] =
              {fixture.category, fixture.categorySource.empty()
                                     ? GdtfFixtureCategory::kManualSource
                                     : fixture.categorySource,
               fixture.categorySourceReason.empty() ? "cached"
                                                   : fixture.categorySourceReason};
        }

        if (!fixture.category.empty() && !fixture.typeName.empty() &&
            fixture.categorySource == GdtfFixtureCategory::kManualSource) {
          GdtfDictionary::UpdateCategory(fixture.typeName, fixture.category);
        }
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

        scene.fixtures[fixture.uuid] = fixture;
      };

  std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &, const Matrix &, const std::string &)> parseTruss =
      [&](tinyxml2::XMLElement *node, const std::string &layerName,
          const Matrix &nodeTransform, const Matrix &localTransform, const std::string &parentGroupUuid) {
        Truss truss;
        truss.uuid =
            resolveStableUuid("Truss", node, layerName, nodeTransform);
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
          truss.sourceRepresentation = Truss::GeometryRepresentation::PublicGdtf;
          truss.gdtfSpec = RemapArchivePathIfNeeded(truss.gdtfSpec);
          const std::string resolvedGdtfPath =
              ResolveGdtfPath(scene.basePath, truss.gdtfSpec);
          const std::string trussGdtfPath = resolvedGdtfPath.empty()
                                                ? truss.gdtfSpec
                                                : resolvedGdtfPath;
          truss.gdtfSpec = ToSceneRelativePathIfPossible(
              scene.basePath, fs::u8path(trussGdtfPath));
          Truss gdtfTruss;
          if (LoadTrussDefinition(trussGdtfPath, gdtfTruss)) {
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
              truss.gdtfMode = gdtfTruss.gdtfMode.empty() ? "Default" : gdtfTruss.gdtfMode;
          } else {
            gdtfLoadFailed = true;
          }
        }

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          if (tinyxml2::XMLElement *g3d =
                  geos->FirstChildElement("Geometry3D")) {
            truss.sourceRepresentation = Truss::GeometryRepresentation::Geometry3D;
            const char *file = g3d->Attribute("fileName");
            if (file)
              truss.symbolFile = normalizeAndResolveGeometryFileName(file);
            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "Truss/Geometry3D", geoMatrix, true);
            truss.sourceGeometryMatrix = geoMatrix;
            if (const char *type = g3d->Attribute("geometryType"))
              truss.sourceGeometryType = Trim(type);
            truss.transform = MatrixUtils::Multiply(nodeTransform, geoMatrix);
          } else if (tinyxml2::XMLElement *sym =
                         geos->FirstChildElement("Symbol")) {
            truss.sourceRepresentation = Truss::GeometryRepresentation::SymbolSymdef;
            std::vector<SymdefGeometry> symGeometries;
            std::string symType;
            Matrix symMatrix = MatrixUtils::Identity();
            resolveSymdefReference(sym, symGeometries, symType, symMatrix);
            if (const char *symdef = sym->Attribute("symdef"))
              truss.sourceSymdefUuid = Trim(symdef);
            truss.sourceSymbolMatrix = symMatrix;
            truss.sourceGeometryType = symType;
            Matrix symLocal = symMatrix;
            if (!symGeometries.empty()) {
              truss.symbolFile =
                  normalizeAndResolveGeometryFileName(symGeometries.front().file);
              symLocal = MatrixUtils::Multiply(symMatrix,
                                               symGeometries.front().transform);
            }
            truss.transform = MatrixUtils::Multiply(nodeTransform, symLocal);
            LogMessage(Logger::Level::Info,
                       "MVR import truss preserves Symbol/Symdef representation for uuid='" +
                           truss.uuid + "', symdef='" + truss.sourceSymdefUuid + "'");
          }
        }


        const bool hasGdtfMetadataAuthority = !truss.gdtfSpec.empty() && !gdtfLoadFailed;

        if (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData")) {
          for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
               data = data->NextSiblingElement("Data")) {
            if (tinyxml2::XMLElement *info =
                    data->FirstChildElement("TrussInfo")) {
              // GDTF values are authoritative when available. TrussInfo is used
              // only as fallback metadata if GDTF could not be loaded.
              if (!hasGdtfMetadataAuthority) {
                if (tinyxml2::XMLElement *m =
                        info->FirstChildElement("Manufacturer"))
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
                if (tinyxml2::XMLElement *cs =
                        info->FirstChildElement("CrossSection"))
                  if (cs->GetText())
                    truss.crossSection = Trim(cs->GetText());
              }
              if (tinyxml2::XMLElement *mf =
                      info->FirstChildElement("ModelFile"))
                if (mf->GetText())
                  truss.modelFile = mf->GetText();
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
                  truss.perastageAuxGdtfArchivePath = RemapArchivePathIfNeeded(Trim(ag->GetText()));
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
            fs::path auxPath = scene.basePath.empty()
                                   ? fs::u8path(typeIt->second)
                                   : fs::u8path(scene.basePath) / fs::u8path(typeIt->second);
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
        const bool symbolRenderable = IsRenderableTrussGeometry(truss.symbolFile);
        const bool symbolExists = symbolRenderable && fs::exists(resolvedSymbolPath);
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

  std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &)> parseSupport =
      [&](tinyxml2::XMLElement *node, const std::string &layerName,
          const Matrix &nodeTransform) {
        Support support;
        support.uuid =
            resolveStableUuid("Support", node, layerName, nodeTransform);
        support.layer = layerName;
        support.transform = nodeTransform;

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
              scene.basePath, fs::u8path(supportGdtfPath));
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

        ReadSupportHoistInfoFromUserData(node, support);
        ApplySupportHoistInfoDefaults(support);
        auto posIt = scene.positions.find(support.position);
        if (posIt != scene.positions.end())
          support.positionName = posIt->second;

        scene.supports[support.uuid] = support;
      };

  std::function<void(tinyxml2::XMLElement *, const std::string &, const Matrix &, const Matrix &, const std::string &)>
      parseSceneObj = [&](tinyxml2::XMLElement *node,
                          const std::string &layerName,
                          const Matrix &nodeTransform, const Matrix &localTransform, const std::string &parentGroupUuid) {
        SceneObject obj;
        obj.uuid =
            resolveStableUuid("SceneObject", node, layerName, nodeTransform);
        obj.layer = layerName;
        obj.transform = nodeTransform;
        if (const char *nameAttr = node->Attribute("name"))
          obj.name = nameAttr;

        std::string geometryType;

        if (const char *typeAttr = node->Attribute("geometryType"))
          geometryType = Trim(typeAttr);

        if (tinyxml2::XMLElement *geos =
                node->FirstChildElement("Geometries")) {
          for (tinyxml2::XMLElement *g3d = geos->FirstChildElement("Geometry3D"); g3d;
               g3d = g3d->NextSiblingElement("Geometry3D")) {
            const char *file = g3d->Attribute("fileName");
            if (!file)
              continue;

            if (const char *type = g3d->Attribute("geometryType"))
              geometryType = Trim(type);

            Matrix geoMatrix = MatrixUtils::Identity();
            parseMatrixOrIdentity(g3d, "Matrix", "SceneObject/Geometry3D", geoMatrix, true);
            appendGeometryInstance(obj.geometries, file, geoMatrix);
          }

          for (tinyxml2::XMLElement *sym = geos->FirstChildElement("Symbol"); sym;
               sym = sym->NextSiblingElement("Symbol")) {
            std::vector<SymdefGeometry> symGeometries;
            Matrix symMatrix = MatrixUtils::Identity();
            std::string symGeometryType;
            resolveSymdefReference(sym, symGeometries, symGeometryType, symMatrix);
            if (!symGeometryType.empty())
              geometryType = symGeometryType;

            for (const auto &geo : symGeometries) {
              Matrix localTransform = MatrixUtils::Multiply(symMatrix, geo.transform);
              appendGeometryInstance(obj.geometries, geo.file, localTransform);
              if (!geo.geometryType.empty())
                geometryType = geo.geometryType;
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
          for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData"); ud;
               ud = ud->NextSiblingElement("UserData")) {
            for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
                 data = data->NextSiblingElement("Data")) {
              const std::string provider = ToLowerCopy(
                  Trim(data->Attribute("provider") ? data->Attribute("provider") : ""));
              if (provider != "perastage")
                continue;
              if (tinyxml2::XMLElement *info = data->FirstChildElement("SupportInfo")) {
                if (tinyxml2::XMLElement *n = info->FirstChildElement("GDTFSpec");
                    n && n->GetText()) {
                  support.gdtfSpec = RemapArchivePathIfNeeded(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("GDTFMode");
                    n && n->GetText()) {
                  support.gdtfMode = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("Function");
                    n && n->GetText()) {
                  support.function = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("HoistFunction");
                    n && n->GetText()) {
                  support.hoistFunction = NormalizeHoistFunction(Trim(n->GetText()));
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("ChainLength");
                    n && n->GetText()) {
                  float parsed = 0.0f;
                  if (TryParseFloat(Trim(n->GetText()), parsed))
                    support.chainLength = parsed;
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("Position");
                    n && n->GetText()) {
                  support.position = Trim(n->GetText());
                }
                if (tinyxml2::XMLElement *n = info->FirstChildElement("PositionName");
                    n && n->GetText()) {
                  support.positionName = Trim(n->GetText());
                }
                LogMessage(Logger::Level::Info,
                           "MVR import reconstructed Support from SceneObject fallback uuid='" +
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
          ReadSupportHoistInfoFromUserData(node, support);
          ApplySupportHoistInfoDefaults(support);
          scene.supports[support.uuid] = support;
        } else {
          scene.sceneObjects[obj.uuid] = obj;
        }
      };

  parseChildList = [&](tinyxml2::XMLElement *cl, const std::string &layerName,
                       const Matrix &parentTransform, const std::string &parentGroupUuid) {
    for (tinyxml2::XMLElement *child = cl->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const char *name = child->Name();
      if (!name)
        continue;

      Matrix local = MatrixUtils::Identity();
      parseMatrixOrIdentity(child, "Matrix", std::string("Child/") + name, local, true);
      Matrix nodeTransform = MatrixUtils::Multiply(parentTransform, local);

      std::string nodeName = name;
      if (nodeName == "Fixture") {
        parseFixture(child, layerName, nodeTransform);
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Fixture, referenceUuidForNode("Fixture", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "Truss") {
        parseTruss(child, layerName, nodeTransform, local, parentGroupUuid);
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Truss, referenceUuidForNode("Truss", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "Support") {
        parseSupport(child, layerName, nodeTransform);
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::Support, referenceUuidForNode("Support", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "SceneObject") {
        parseSceneObj(child, layerName, nodeTransform, local, parentGroupUuid);
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::SceneObject,
               referenceUuidForNode("SceneObject", child, layerName, nodeTransform)});
        }
      } else if (nodeName == "GroupObject") {
        GroupObject group;
        group.uuid = resolveStableUuid("GroupObject", child, layerName, nodeTransform);
        group.layer = layerName;
        group.transform = nodeTransform;
        group.localTransform = local;
        group.parentGroupUuid = parentGroupUuid;
        if (const char *nameAttr = child->Attribute("name"))
          group.name = nameAttr;
        scene.groupObjects[group.uuid] = group;
        if (!parentGroupUuid.empty()) {
          scene.groupObjects[parentGroupUuid].children.push_back(
              {MvrNodeType::GroupObject, group.uuid});
        }
        LogMessage(Logger::Level::Info,
                   "MVR import preserved GroupObject uuid='" + group.uuid + "'");
        if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
          parseChildList(inner, layerName, nodeTransform, group.uuid);
        continue;
      }

      if (tinyxml2::XMLElement *inner = child->FirstChildElement("ChildList"))
        parseChildList(inner, layerName, nodeTransform, parentGroupUuid);
    }
  };
  tinyxml2::XMLElement *layersNode = sceneNode->FirstChildElement("Layers");
  if (!layersNode)
    return true;

  for (tinyxml2::XMLElement *cl = layersNode->FirstChildElement("ChildList");
       cl; cl = cl->NextSiblingElement("ChildList")) {
    parseChildList(cl, DEFAULT_LAYER_NAME, MatrixUtils::Identity(), "");
  }

  for (tinyxml2::XMLElement *layer = layersNode->FirstChildElement("Layer");
       layer; layer = layer->NextSiblingElement("Layer")) {
    const char *layerName = layer->Attribute("name");
    std::string layerStr = layerName ? layerName : "";
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
      if (tinyxml2::XMLElement *colorNode =
              layer->FirstChildElement("Color")) {
        if (const char *txt = colorNode->GetText())
          l.color = CieToHex(txt);
      }
      scene.layers[l.uuid] = l;
    }
  }

  auto resolveFixtureGdtfPathForRead = [&](const std::string &spec) {
    if (spec.empty())
      return std::string{};
    std::string remapped = RemapArchivePathIfNeeded(spec);
    std::string resolved = ResolveGdtfPath(scene.basePath, remapped);
    if (!resolved.empty())
      return resolved;
    return ToString(ResolveSceneRelativePath(scene.basePath, remapped).u8string());
  };

  // After parsing the entire scene, resolve any GDTF conflicts using the
  // dictionary only if requested. This occurs before rendering so user choices
  // are applied to the final scene data.
  if (applyDictionary) {
    std::vector<GdtfConflict> gdtfConflicts;
    std::unordered_set<std::string> conflictTypes;
    for (const auto &[uid, f] : scene.fixtures) {
      if (auto dictEntry = GdtfDictionary::Get(f.typeName)) {
        if (conflictTypes.insert(f.typeName).second) {
          gdtfConflicts.push_back({f.typeName, f.gdtfSpec, dictEntry->path});
        }
      }
    }
    if (!gdtfConflicts.empty()) {
      if (promptConflicts) {
        if (progressCallback)
          progressCallback("Conflict dialog:show");
        auto choices = PromptGdtfConflicts(gdtfConflicts);
        if (progressCallback)
          progressCallback("Conflict dialog:hide");
        if (progressCallback)
          progressCallback("Applying GDTF conflict selection...");
        for (auto &[uid, f] : scene.fixtures) {
          auto typeKey = f.typeName;
          auto it = choices.find(typeKey);
          if (it != choices.end()) {
            const int previousChannelCount =
                (!f.gdtfSpec.empty() && !f.gdtfMode.empty())
                    ? GetGdtfModeChannelCount(resolveFixtureGdtfPathForRead(f.gdtfSpec),
                                              f.gdtfMode)
                    : -1;
            f.gdtfSpec = it->second;
            f.gdtfSpec = ToSceneRelativePathIfPossible(
                scene.basePath, fs::u8path(resolveFixtureGdtfPathForRead(f.gdtfSpec)));
            std::string parsed =
                Trim(GetGdtfFixtureName(resolveFixtureGdtfPathForRead(f.gdtfSpec)));
            if (!parsed.empty())
              f.typeName = parsed;
            if (auto dictEntry = GdtfDictionary::Get(typeKey)) {
              if (f.gdtfMode.empty())
                f.gdtfMode = dictEntry->mode;
            }
            f.gdtfMode = ResolveExistingGdtfMode(
                resolveFixtureGdtfPathForRead(f.gdtfSpec), f.gdtfMode,
                previousChannelCount > 0 ? std::optional<int>(previousChannelCount)
                                         : std::nullopt);
          }
        }
      } else {
        for (auto &[uid, f] : scene.fixtures) {
          if (auto dictEntry = GdtfDictionary::Get(f.typeName)) {
            const int previousChannelCount =
                (!f.gdtfSpec.empty() && !f.gdtfMode.empty())
                    ? GetGdtfModeChannelCount(resolveFixtureGdtfPathForRead(f.gdtfSpec),
                                              f.gdtfMode)
                    : -1;
            f.gdtfSpec = dictEntry->path;
            f.gdtfSpec = ToSceneRelativePathIfPossible(
                scene.basePath, fs::u8path(resolveFixtureGdtfPathForRead(f.gdtfSpec)));
            if (f.gdtfMode.empty())
              f.gdtfMode = dictEntry->mode;
            f.gdtfMode = ResolveExistingGdtfMode(
                resolveFixtureGdtfPathForRead(f.gdtfSpec), f.gdtfMode,
                previousChannelCount > 0 ? std::optional<int>(previousChannelCount)
                                         : std::nullopt);
            std::string parsed =
                Trim(GetGdtfFixtureName(resolveFixtureGdtfPathForRead(f.gdtfSpec)));
            if (!parsed.empty())
              f.typeName = parsed;
          }
        }
      }
    }
  }

  if (progressCallback)
    progressCallback("Building fixtures, trusses, and objects...");

  for (auto &[uid, fixture] : scene.fixtures) {
    if (fixture.gdtfSpec.empty())
      continue;
    const int currentChannelCount =
        (!fixture.gdtfMode.empty())
            ? GetGdtfModeChannelCount(resolveFixtureGdtfPathForRead(fixture.gdtfSpec),
                                      fixture.gdtfMode)
            : -1;
    fixture.gdtfMode = ResolveExistingGdtfMode(
        resolveFixtureGdtfPathForRead(fixture.gdtfSpec), fixture.gdtfMode,
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
    oss << "MVR import matrix anomalies: " << matrixScaleAggregation.suspiciousMatrixCount
        << " suspicious matrices detected. Contexts: "
        << JoinMatrixContextCounts(matrixScaleAggregation.suspiciousByContext);
    LogMessage(Logger::Level::Warn, oss.str());

    for (const std::string &example : matrixScaleAggregation.suspiciousExamples)
      LogMessage(Logger::Level::Warn, "MVR import suspicious matrix example: " + example);
  }

  std::string summary =
      "Parsed scene: " + std::to_string(scene.fixtures.size()) + " fixtures, " +
      std::to_string(scene.trusses.size()) + " trusses, " +
      std::to_string(scene.supports.size()) + " supports, " +
      std::to_string(scene.sceneObjects.size()) + " objects";
  LogMessage(summary);
  return true;
}

bool MvrImporter::ImportAndRegister(const std::string &filePath,
                                    bool promptConflicts,
                                    bool applyDictionary,
                                    ProgressCallback progressCallback) {
  MvrImporter importer;
  return importer.ImportFromFile(filePath, promptConflicts, applyDictionary,
                                 progressCallback);
}
