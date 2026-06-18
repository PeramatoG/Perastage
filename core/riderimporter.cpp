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
#include "riderimporter.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pdftext.h"

#include "autopatcher.h"
#include "configmanager.h"
#include "fixture.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "hoist_weight_distribution.h"
#include "layer.h"
#include "logger.h"
#include "sceneobject.h"
#include "support.h"
#include "truss.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "units/units.h"
#include "uuidutils.h"
#include <filesystem>

namespace {
constexpr const char *kPrimitiveCylinderToken = "primitive:cylinder";
constexpr const char *kPrimitiveCubeToken = "primitive:cube";

struct RiderImportTimingStats {
  std::chrono::steady_clock::time_point startedAt =
      std::chrono::steady_clock::now();
  std::unordered_map<std::string, double> phaseMs;
  size_t fixtureCount = 0;
  size_t trussCount = 0;
  size_t uniqueFixtureLookups = 0;
  size_t uniqueTrussLookups = 0;
};

// Records elapsed milliseconds for a rider import phase.
void AddRiderImportPhaseTime(RiderImportTimingStats &timing,
                             const std::string &phaseName,
                             std::chrono::steady_clock::time_point startedAt) {
  timing.phaseMs[phaseName] += std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - startedAt)
                                   .count();
}

// Logs one concise timing line for the completed rider import.
void LogRiderImportTiming(const RiderImportTimingStats &timing) {
  const auto totalMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - timing.startedAt)
                           .count();
  auto phase = [&](const std::string &name) {
    const auto it = timing.phaseMs.find(name);
    return it == timing.phaseMs.end() ? 0.0 : it->second;
  };
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << "Rider import timing: total=" << totalMs << "ms"
      << ", filter=" << phase("filter/reduce text") << "ms"
      << ", fixtureDict=" << phase("load fixture dictionary") << "ms"
      << ", trussDict=" << phase("load truss dictionary") << "ms"
      << ", parse=" << phase("parse reduced text") << "ms"
      << ", createFixtures=" << phase("create fixtures") << "ms"
      << ", createTrusses=" << phase("create trusses") << "ms"
      << ", gdtfMetadata=" << phase("resolve GDTF metadata") << "ms"
      << ", trussDefinitions=" << phase("resolve truss definitions") << "ms"
      << ", placeObjects=" << phase("place imported objects") << "ms"
      << ", assignIdentities=" << phase("assign fixture identities") << "ms"
      << ", autoPatch=" << phase("Auto Patch") << "ms"
      << "; fixtures=" << timing.fixtureCount
      << ", uniqueFixtureLookups=" << timing.uniqueFixtureLookups
      << ", trusses=" << timing.trussCount
      << ", uniqueTrussLookups=" << timing.uniqueTrussLookups;
  Logger::Instance().Log(Logger::Level::Debug, oss.str());
}
Matrix BuildCylinderPitchY90LocalTransform() {
  Matrix transform{};
  // Equivalent to applying a +90° pitch around Y after creating the
  // cylinder primitive with its default orientation.
  transform.u = {0.0f, 0.0f, -1.0f};
  transform.v = {0.0f, 1.0f, 0.0f};
  transform.w = {1.0f, 0.0f, 0.0f};
  transform.o = {0.0f, 0.0f, 0.0f};
  return transform;
}

Matrix BuildPipeObjectTransform(float pipeLengthMm, float pipeDiameterMm,
                                float primitiveCylinderHeightMm) {
  Matrix transform{};
  const float radialScale = pipeDiameterMm / primitiveCylinderHeightMm;
  const float lengthScale = pipeLengthMm / primitiveCylinderHeightMm;
  transform.u = {radialScale, 0.0f, 0.0f};
  transform.v = {0.0f, radialScale, 0.0f};
  transform.w = {0.0f, 0.0f, lengthScale};
  transform.o = {0.0f, 0.0f, 0.0f};
  return transform;
}
// Precompiled regexes used by RiderImporter. Keeping them static avoids paying
// the compilation cost on every import call and makes keyword matching cheap
// even when processing large riders.
static const std::regex
    kTrussLineRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:truss|pipe|pipes|vara|varas)"
                 "\\s+([^\\n]*?)\\s+(\\d+(?:\\.\\d+)?)\\s*(?:m|metros?|meters?)"
                 "\\b(?:\\s+(?:(?:para|for)\\s+)?(.+))?\\s*$",
    std::regex::icase);
static const std::regex kTrussLineNoLengthRe(
    "^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:truss|pipe|pipes|vara|varas)\\s+([^\\n]*?)("
    "?:\\s+(?:para|for)\\s+(.+))?\\s*$",
    std::regex::icase);
static const std::regex kTrussRe("(?:truss|pipe|pipes|vara|varas)[^\\n]*?(\\d+("
                                 "?:\\.\\d+)?)\\s*(?:m|metros?|meters?)\\b",
    std::regex::icase);
static const std::regex kPipeKeywordRe("\\b(?:pipe|pipes|vara|varas)\\b",
                                       std::regex::icase);
static const std::regex
    kLengthWithUnitRe("(\\d+(?:\\.\\d+)?)\\s*(?:m|metros?|meters?)\\b",
                      std::regex::icase);
static const std::regex kHoistLineRe(
    "^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:motor(?:es)?|hoist(?:s)?)\\b(.*)$",
    std::regex::icase);
static const std::regex
    kHoistCapacityRe("(\\d+(?:[\\.,]\\d+)?)\\s*(kg|kgs?|kilogramos?|kilos?|t|"
                     "to|tn|ton|tons?|toneladas?)\\b",
    std::regex::icase);
static const std::regex kHoistTargetRe("\\b(?:para|for)\\s+(.+)$",
                                       std::regex::icase);
static const std::regex kFixtureLineRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s+(.+)$",
                                       std::regex::icase);
static const std::regex kQuantityOnlyRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s*$");
static const std::regex kHangLineRe(
    "^\\s*(LX\\d+|lx\\s*sides?|screen|pantalla|proyecci[oó]n|led\\s*screen|"
    "backdrops?|tel[oó]n(?:es)?|puente\\s+de\\s+tel[oó]n(?:es)?|floor|efectos?|"
    "calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?|calle(?:s)?|side(?:s)?)(?:\\s*("
    "?:\\([^\\)]*\\)|\\[[^\\]]*\\]))*\\s*:?\\s*$",
    std::regex::icase);
static const std::regex kHangHeaderWithSuffixRe(
    "^\\s*(LX\\d+|lx\\s*sides?|screen|pantalla|proyecci[oó]n|led\\s*screen|"
    "backdrops?|tel[oó]n(?:es)?|puente\\s+de\\s+tel[oó]n(?:es)?|floor|efectos?|"
    "calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?|calle(?:s)?|side(?:s)?)(?:\\s+["
    "^:]*)?\\s*:\\s*$",
    std::regex::icase);
static const std::regex kHangFindRe(
    "(LX\\d+|lx\\s*sides?|screen|pantalla|proyecci[oó]n|led\\s*screen|"
    "backdrops?|tel[oó]n(?:es)?|puente\\s+de\\s+tel[oó]n(?:es)?|floor|efectos?|"
    "calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?|calle(?:s)?|side(?:s)?)",
                                    std::regex::icase);
static const std::regex kHangOnlyRe(
    "^\\s*(LX\\d+|lx\\s*sides?|screen|pantalla|proyecci[oó]n|led\\s*screen|"
    "backdrops?|tel[oó]n(?:es)?|puente\\s+de\\s+tel[oó]n(?:es)?|floor|efectos?|"
    "calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?|calle(?:s)?|side(?:s)?)\\s*$",
                                    std::regex::icase);
static const std::regex kTrailingHangWithCoordinateRe(
    "(LX\\d+|lx\\s*sides?|screen|pantalla|proyecci[oó]n|led\\s*screen|"
    "backdrops?|tel[oó]n(?:es)?|puente\\s+de\\s+tel[oó]n(?:es)?|floor|efectos?|"
    "calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?|calle(?:s)?|side(?:s)?)(?:\\s*("
    "(?:\\([^\\)]*\\)|\\[[^\\]]*\\])))?\\s*$",
    std::regex::icase);
static const std::regex kParenthesizedCleanupRe("\\([^\\)]*\\)");
static const std::regex kBracketedCleanupRe("\\[[^\\]]*\\]");
static const std::regex kHyphenSpacingCleanupRe("\\s*[-]\\s*");
static const std::regex kWhitespaceCleanupRe("\\s+");
std::string Trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string StripRiderAnnotations(const std::string &line) {
  if (line.empty())
    return {};

  std::string stripped = line;
  stripped =
      std::regex_replace(stripped, std::regex("\\*\\([^\\)]*\\)\\*"), " ");
  return Trim(stripped);
}

enum class RiggingLineKind { Truss, Pipe };

RiggingLineKind DetectRiggingLineKind(const std::string &line) {
  return std::regex_search(line, kPipeKeywordRe) ? RiggingLineKind::Pipe
                                                  : RiggingLineKind::Truss;
}

std::optional<std::string> ExtractParenthesizedToken(const std::string &text) {
  const size_t open = text.find('(');
  if (open == std::string::npos)
    return std::nullopt;
  const size_t close = text.find(')', open + 1);
  if (close == std::string::npos || close <= open)
    return std::nullopt;
  return text.substr(open, close - open + 1);
}

std::optional<std::string> ExtractBracketedToken(const std::string &text) {
  const size_t open = text.find('[');
  if (open == std::string::npos)
    return std::nullopt;
  const size_t close = text.find(']', open + 1);
  if (close == std::string::npos || close <= open)
    return std::nullopt;
  return text.substr(open, close - open + 1);
}

std::string NormalizeHangName(const std::string &raw);

std::string ResolveHangTargetFallback(const std::string &raw) {
  std::string candidate = Trim(raw);
  if (candidate.empty())
    return {};

  std::smatch targetMatch;
  if (std::regex_search(candidate, targetMatch, kHoistTargetRe) &&
      targetMatch.size() > 1) {
    candidate = Trim(targetMatch[1].str());
  }

  // Some riders append accessories before the real target:
  // "... + HUESITOS PARA PUENTES LX". Keep only the effective hang target.
  const size_t plusPos = candidate.find('+');
  if (plusPos != std::string::npos) {
    const std::string tail = Trim(candidate.substr(plusPos + 1));
    if (!tail.empty()) {
      std::smatch plusTargetMatch;
      if (std::regex_search(tail, plusTargetMatch, kHoistTargetRe) &&
          plusTargetMatch.size() > 1) {
        candidate = Trim(plusTargetMatch[1].str());
      }
    }
  }

  return NormalizeHangName(candidate);
}

bool TryExtractTrailingHangWithCoordinate(std::string &text,
                                          std::string &hangOut,
                                          std::string &coordinateSuffixOut) {
  std::smatch trailingHangMatch;
  if (!std::regex_search(text, trailingHangMatch,
                         kTrailingHangWithCoordinateRe) ||
      trailingHangMatch.size() < 2) {
    return false;
  }

  hangOut = Trim(trailingHangMatch[1].str());
  coordinateSuffixOut.clear();
  if (trailingHangMatch.size() > 2 && trailingHangMatch[2].matched)
    coordinateSuffixOut = Trim(trailingHangMatch[2].str());

  text =
      Trim(text.substr(0, static_cast<size_t>(trailingHangMatch.position(0))));
  return !hangOut.empty();
}

std::string ResolveGdtfPath(const MvrScene &scene,
                            const std::string &gdtfSpecPath) {
  if (gdtfSpecPath.empty())
    return {};

  std::filesystem::path specPath = std::filesystem::path(gdtfSpecPath);
  if (specPath.is_absolute() || scene.basePath.empty())
    return gdtfSpecPath;

  return (std::filesystem::path(scene.basePath) / specPath).string();
}

// Returns whether an imported fixture category is authoritative enough to keep.
bool HasAuthoritativeImportCategory(const Fixture &fixture) {
  if (fixture.category.empty())
    return false;
  return fixture.categorySource == GdtfFixtureCategory::kManualSource ||
         fixture.categorySource == GdtfFixtureCategory::kAutoFallbackSource;
}

// Ensures imported fixtures always have a resolved category and source.
void EnsureFixtureCategoryForImport(const MvrScene &scene, Fixture &fixture) {
  if (HasAuthoritativeImportCategory(fixture)) {
    if (fixture.categorySource == GdtfFixtureCategory::kManualSource)
      fixture.categorySourceReason.clear();
    return;
  }

  auto containsWord = [](std::string value, const std::string &needle) {
    std::transform(
        value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value.find(needle) != std::string::npos;
  };
  auto looksLikeWashFromChannels = [&](const std::string &gdtfPath,
                                       const std::string &modeName) {
    struct ModeSignals {
      bool hasChannels = false;
      bool looksWash = false;
    };
    auto evaluateMode = [&](const std::string &mode) -> ModeSignals {
      const std::vector<GdtfChannelInfo> channels =
          GetGdtfModeChannels(gdtfPath, mode);
      bool hasPan = false;
      bool hasTilt = false;
      bool hasGobo = false;
      const bool hasAnyChannel = !channels.empty();
      for (const GdtfChannelInfo &channel : channels) {
        const std::string function = channel.function;
        if (containsWord(function, "pan"))
          hasPan = true;
        if (containsWord(function, "tilt"))
          hasTilt = true;
        if (containsWord(function, "gobo"))
          hasGobo = true;
      }
      return {hasAnyChannel, hasPan && hasTilt && !hasGobo};
    };

    if (!modeName.empty()) {
      const ModeSignals selectedMode = evaluateMode(modeName);
      if (selectedMode.hasChannels)
        return selectedMode.looksWash;
    }

    const std::vector<std::string> modes = GetGdtfModes(gdtfPath);
    for (const std::string &mode : modes) {
      if (evaluateMode(mode).looksWash)
        return true;
    }
    return false;
  };
  auto inferCategoryFromName = [&](const std::string &name) {
    if (name.empty())
      return std::string();
    if (containsWord(name, "blinder") || containsWord(name, "cegadora"))
      return std::string(GdtfFixtureCategory::kBlinder);
    if (containsWord(name, "strobe") || containsWord(name, "estrobo"))
      return std::string(GdtfFixtureCategory::kStrobe);
    if (containsWord(name, "hybrid") || containsWord(name, "hibrido") ||
        containsWord(name, "híbrido"))
      return std::string(GdtfFixtureCategory::kHybrid);
    if (containsWord(name, "beam"))
      return std::string(GdtfFixtureCategory::kBeam);
    if (containsWord(name, "spot") || containsWord(name, "profile"))
      return std::string(GdtfFixtureCategory::kSpot);
    if (containsWord(name, "wash"))
      return std::string(GdtfFixtureCategory::kWash);
    if (containsWord(name, "fresnel") || containsWord(name, "fresnell") ||
        containsWord(name, "pc") || containsWord(name, "par"))
      return std::string(GdtfFixtureCategory::kConventional);
    if (containsWord(name, "haze") || containsWord(name, "hazer") ||
        containsWord(name, "smoke") || containsWord(name, "humo") ||
        containsWord(name, "fog") || containsWord(name, "niebla") ||
        containsWord(name, "fan") || containsWord(name, "turbina") ||
        containsWord(name, "turbine") || containsWord(name, "ventilador"))
      return std::string(GdtfFixtureCategory::kSmoke);
    return std::string();
  };

  if (fixture.category.empty()) {
    fixture.category = inferCategoryFromName(fixture.typeName);
    if (!fixture.category.empty())
      fixture.categorySourceReason = "name hint typeName";
  }

  if (fixture.category.empty() && !fixture.gdtfSpec.empty()) {
    const std::string resolvedGdtfPath =
        ResolveGdtfPath(scene, fixture.gdtfSpec);
    const std::filesystem::path gdtfPath(resolvedGdtfPath);
    const bool washFromChannels =
        std::filesystem::exists(gdtfPath) &&
        looksLikeWashFromChannels(resolvedGdtfPath, fixture.gdtfMode);
    fixture.category = inferCategoryFromName(gdtfPath.stem().string());
    if (!fixture.category.empty())
      fixture.categorySourceReason = "name hint gdtf filename";

    if (fixture.category.empty() && std::filesystem::exists(gdtfPath)) {
      const auto inferred =
          GdtfFixtureCategory::InferFromGdtf(resolvedGdtfPath);
      if (inferred.category != GdtfFixtureCategory::kUnknown) {
        fixture.category = inferred.category;
        fixture.categorySourceReason = inferred.reason;
      }
    }

    if (washFromChannels &&
        (fixture.category.empty() ||
         fixture.category == GdtfFixtureCategory::kHybrid)) {
      const bool overridingHybrid =
          fixture.category == GdtfFixtureCategory::kHybrid;
      fixture.category = GdtfFixtureCategory::kWash;
      fixture.categorySourceReason =
          overridingHybrid
              ? "channel hints override hybrid: pan+tilt without gobo"
              : "channel hints: pan+tilt without gobo";
    }
  }

  if (fixture.category.empty()) {
    fixture.category = GdtfFixtureCategory::kUnknown;
    fixture.categorySourceReason = "no hints";
  }

  if (fixture.categorySource.empty())
    fixture.categorySource = GdtfFixtureCategory::kAutoFallbackSource;
  if (fixture.categorySource == GdtfFixtureCategory::kManualSource)
    fixture.categorySourceReason.clear();
}

// Parses a trimmed float token and accepts only full-token numeric input.
bool TryParseFloat(const std::string &text, float &out) {
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

bool TryParseInt(std::string_view text, int &out) {
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

  int value = 0;
  auto begin = trimmed.data();
  auto end = trimmed.data() + trimmed.size();
  auto result = std::from_chars(begin, end, value);
  if (result.ec == std::errc{} && result.ptr == end) {
    out = value;
    return true;
  }
  return false;
}

struct TrussCoordinateOverride {
  bool hasX = false;
  bool hasY = false;
  bool hasZ = false;
  float xMm = 0.0f;
  float yMm = 0.0f;
  float zMm = 0.0f;
};

std::optional<TrussCoordinateOverride>
ParseTrussCoordinateOverride(std::string &text,
                             Units::DistanceUnitSystem unitSystem) {
  const size_t open = text.find('(');
  if (open == std::string::npos)
    return std::nullopt;

  const size_t close = text.find(')', open + 1);
  if (close == std::string::npos || close <= open + 1)
    return std::nullopt;

  const std::string inside = text.substr(open + 1, close - open - 1);
  static const std::regex kCoordinateNumberRe("[-+]?\\d+(?:[\\.,]\\d+)?");
  std::vector<double> values;
  values.reserve(3);
  for (std::sregex_iterator
           it(inside.begin(), inside.end(), kCoordinateNumberRe),
       end;
       it != end && values.size() < 3; ++it) {
    std::string token = it->str();
    std::replace(token.begin(), token.end(), ',', '.');
    float parsed = 0.0f;
    if (!TryParseFloat(token, parsed))
      continue;
    values.push_back(Units::DistanceDisplayToMillimeters(
        static_cast<double>(parsed), unitSystem));
  }
  if (values.empty())
    return std::nullopt;

  text.erase(open, close - open + 1);
  text = Trim(text);

  TrussCoordinateOverride override;
  if (values.size() >= 3) {
    override.hasX = true;
    override.hasY = true;
    override.hasZ = true;
    override.xMm = static_cast<float>(values[0]);
    override.yMm = static_cast<float>(values[1]);
    override.zMm = static_cast<float>(values[2]);
  } else if (values.size() == 2) {
    override.hasY = true;
    override.hasZ = true;
    override.yMm = static_cast<float>(values[0]);
    override.zMm = static_cast<float>(values[1]);
  } else {
    override.hasY = true;
    override.yMm = static_cast<float>(values[0]);
  }
  return override;
}

std::optional<float>
ParseTrussMarginOverrideMm(std::string &text,
                           Units::DistanceUnitSystem unitSystem) {
  const size_t open = text.find('[');
  if (open == std::string::npos)
    return std::nullopt;

  const size_t close = text.find(']', open + 1);
  if (close == std::string::npos || close <= open + 1)
    return std::nullopt;

  std::string token = text.substr(open + 1, close - open - 1);
  token = Trim(token);
  std::replace(token.begin(), token.end(), ',', '.');

  float parsed = 0.0f;
  if (!TryParseFloat(token, parsed) || parsed < 0.0f)
    return std::nullopt;

  text.erase(open, close - open + 1);
  text = Trim(text);
  return static_cast<float>(Units::DistanceDisplayToMillimeters(
      static_cast<double>(parsed), unitSystem));
}

bool TryParseScreenDimensionsMm(const std::string &text, float &widthMm,
                                float &heightMm) {
  static const std::regex kScreenDimensionRe(
      "(\\d+(?:[\\.,]\\d+)?)\\s*(?:m|metros?)?\\s*[xX]\\s*(\\d+(?:[\\.,]\\d+)?)"
      "\\s*(?:m|metros?)?",
      std::regex::icase);
  std::smatch matches;
  if (!std::regex_search(text, matches, kScreenDimensionRe))
    return false;

  auto parseMetricValue = [](std::string value, float &outMeters) {
    std::replace(value.begin(), value.end(), ',', '.');
    if (!TryParseFloat(value, outMeters))
      return false;
    return outMeters > 0.0f;
  };

  float widthMeters = 0.0f;
  float heightMeters = 0.0f;
  if (!parseMetricValue(matches[1].str(), widthMeters) ||
      !parseMetricValue(matches[2].str(), heightMeters)) {
    return false;
  }

  widthMm = widthMeters * 1000.0f;
  heightMm = heightMeters * 1000.0f;
  return true;
}

int ParseTrailingNumber(const std::string &text) {
  const size_t space = text.find_last_of(' ');
  if (space == std::string::npos || space + 1 >= text.size())
    return 0;
  int parsed = 0;
  return TryParseInt(std::string_view(text).substr(space + 1), parsed) ? parsed
                                                                        : 0;
}

bool IsRenderableTrussGeometry(const std::string &path) {
  if (path.empty())
    return false;
  std::string ext = std::filesystem::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return ext == ".3ds" || ext == ".glb";
}

std::filesystem::path ResolveTrussSymbolPath(const Truss &truss) {
  std::filesystem::path symbolPath = PathUtils::PathFromUtf8(truss.symbolFile);
  if (symbolPath.is_absolute())
    return symbolPath;

  if (!truss.modelFile.empty()) {
    std::filesystem::path modelPath = PathUtils::PathFromUtf8(truss.modelFile);
    if (modelPath.is_absolute())
      return modelPath.parent_path() / symbolPath;
  }

  if (!truss.gdtfSpec.empty()) {
    std::filesystem::path gdtfPath = PathUtils::PathFromUtf8(truss.gdtfSpec);
    if (gdtfPath.is_absolute())
      return gdtfPath.parent_path() / symbolPath;
  }

  return symbolPath;
}

std::string DescribeTrussForLog(const Truss &truss) {
  const std::string displayName = truss.name.empty() ? "(unnamed)" : truss.name;
  std::ostringstream oss;
  oss << "uuid='" << truss.uuid << "', name='" << displayName << "', model='"
      << truss.model << "', modelFile='" << truss.modelFile << "', gdtfSpec='"
      << truss.gdtfSpec << "', symbolFile='" << truss.symbolFile << "'";
  return oss.str();
}

// Returns a trimmed view over ASCII whitespace without copying.
std::string_view TrimView(std::string_view s) {
  const size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos)
    return {};
  const size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Visits each non-empty trimmed plus-separated segment without owning strings.
template <typename Callback>
void ForEachSplitPlusPart(std::string_view s, Callback callback) {
  size_t start = 0;
  while (start <= s.size()) {
    const size_t plus = s.find('+', start);
    const size_t end = plus == std::string_view::npos ? s.size() : plus;
    const std::string_view part = TrimView(s.substr(start, end - start));
    if (!part.empty())
      callback(part);
    if (plus == std::string_view::npos)
      break;
    start = plus + 1;
  }
}

// Splits a plus-separated string into owned, trimmed, non-empty segments.
std::vector<std::string> SplitPlus(const std::string &s) {
  std::vector<std::string> out;
  ForEachSplitPlusPart(s,
                       [&](std::string_view item) { out.emplace_back(item); });
  return out;
}

bool IsFloorAlias(std::string_view value);
bool IsLxSidesAlias(std::string_view value);

std::string NormalizeHangName(const std::string &raw) {
  std::string hang = Trim(raw);
  if (hang.empty())
    return {};
  hang = std::regex_replace(hang, std::regex("\\([^\\)]*\\)"), "");
  hang = std::regex_replace(hang, std::regex("\\[[^\\]]*\\]"), "");
  hang = Trim(hang);
  if (hang.empty())
    return {};
  if (IsFloorAlias(hang))
    return "FLOOR";
  if (IsLxSidesAlias(hang))
    return "LX SIDES";
  std::transform(hang.begin(), hang.end(), hang.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  if (hang.rfind("PUENTES ", 0) == 0)
    hang = Trim(hang.substr(8));
  else if (hang.rfind("PUENTE ", 0) == 0)
    hang = Trim(hang.substr(7));
  if (hang == "PANTALLA" || hang == "PROYECCION" || hang == "PROYECCIÓN")
    return "SCREEN";
  if (hang == "SCREEN" || hang == "LEDSCREEN")
    return "SCREEN";
  if (hang == "BACKDROP" || hang == "BACKDROPS" || hang == "TELON" ||
      hang == "TELONES")
    return "BACKDROP";
  if (hang == "SIDE FILL")
    return "SIDEFILL";
  if (hang == "P.A." || hang == "P.A" || hang == "PA")
    return "PA";
  return hang;
}

float ParseHoistCapacityKg(const std::string &text) {
  std::smatch match;
  if (!std::regex_search(text, match, kHoistCapacityRe))
    return 0.0f;

  std::string number = match[1].str();
  std::replace(number.begin(), number.end(), ',', '.');
  float value = 0.0f;
  if (!TryParseFloat(number, value))
    return 0.0f;

  std::string unit = match[2].str();
  std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  const bool tons = unit == "t" || unit == "to" || unit == "tn" ||
                    unit == "ton" || unit == "tons" || unit == "toneladas";
  return tons ? value * 1000.0f : value;
}

bool IsLxHangName(const std::string &positionName) {
  if (positionName.size() < 3 || positionName.rfind("LX", 0) != 0)
    return false;
  for (size_t i = 2; i < positionName.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(positionName[i])))
      return false;
  }
  return true;
}

bool IsLxSidesHangName(const std::string &positionName) {
  return NormalizeHangName(positionName) == "LX SIDES";
}

std::string BuildIndexedName(const std::string &prefix, int index,
                             bool omitIndexForSingle) {
  if (omitIndexForSingle && index == 1)
    return prefix;
  return prefix + " " + std::to_string(index);
}

void AssignImportedHoistNames(std::vector<Support *> &supports) {
  if (supports.empty())
    return;

  auto sortByX = [](Support *a, Support *b) {
    if (a->transform.o[0] != b->transform.o[0])
      return a->transform.o[0] < b->transform.o[0];
    return a->uuid < b->uuid;
  };

  std::map<std::string, std::vector<Support *>> lxByPosition;
  std::vector<Support *> lxSidesSupports;
  std::vector<Support *> screenSupports;
  std::vector<Support *> backdropSupports;
  std::vector<Support *> sidefillSupports;
  std::vector<Support *> paSupports;
  std::vector<Support *> otherSupports;

  for (Support *support : supports) {
    if (!support)
      continue;
    const std::string position = NormalizeHangName(support->positionName);
    if (IsLxHangName(position)) {
      lxByPosition[position].push_back(support);
    } else if (IsLxSidesHangName(position)) {
      lxSidesSupports.push_back(support);
    } else if (position == "SCREEN") {
      screenSupports.push_back(support);
    } else if (position == "BACKDROP") {
      backdropSupports.push_back(support);
    } else if (position == "SIDEFILL") {
      sidefillSupports.push_back(support);
    } else if (position == "PA" || position == "P.A.") {
      paSupports.push_back(support);
    } else {
      otherSupports.push_back(support);
    }
  }

  for (std::map<std::string, std::vector<Support *> >::iterator it =
           lxByPosition.begin();
       it != lxByPosition.end(); ++it) {
    const std::string &position = it->first;
    std::vector<Support *> &items = it->second;
    std::sort(items.begin(), items.end(), sortByX);
    for (size_t i = 0; i < items.size(); ++i) {
      Support *support = items[i];
      support->name = position + " " + std::to_string(i + 1);
      support->motorName = support->name;
    }
  }

  auto sortByYThenX = [](Support *a, Support *b) {
    if (a->transform.o[1] != b->transform.o[1])
      return a->transform.o[1] < b->transform.o[1];
    if (a->transform.o[0] != b->transform.o[0])
      return a->transform.o[0] < b->transform.o[0];
    return a->uuid < b->uuid;
  };
  std::vector<Support *> sideLeftSupports;
  std::vector<Support *> sideRightSupports;
  for (Support *support : lxSidesSupports) {
    if (!support)
      continue;
    if (support->transform.o[0] <= 0.0f)
      sideLeftSupports.push_back(support);
    else
      sideRightSupports.push_back(support);
  }
  std::sort(sideLeftSupports.begin(), sideLeftSupports.end(), sortByYThenX);
  std::sort(sideRightSupports.begin(), sideRightSupports.end(), sortByYThenX);
  for (size_t i = 0; i < sideLeftSupports.size(); ++i) {
    Support *support = sideLeftSupports[i];
    support->name = "SIDE L " + std::to_string(i + 1);
    support->motorName = support->name;
  }
  for (size_t i = 0; i < sideRightSupports.size(); ++i) {
    Support *support = sideRightSupports[i];
    support->name = "SIDE R " + std::to_string(i + 1);
    support->motorName = support->name;
  }

  std::sort(screenSupports.begin(), screenSupports.end(), sortByX);
  for (size_t i = 0; i < screenSupports.size(); ++i) {
    Support *support = screenSupports[i];
    support->name = "SCR " + std::to_string(i + 1);
    support->motorName = support->name;
  }

  std::sort(backdropSupports.begin(), backdropSupports.end(), sortByX);
  for (size_t i = 0; i < backdropSupports.size(); ++i) {
    Support *support = backdropSupports[i];
    support->name = "BACKDROP " + std::to_string(i + 1);
    support->motorName = support->name;
  }

  auto assignLeftRightNames =
      [&](std::vector<Support *> &items, const std::string &leftPrefix,
          const std::string &rightPrefix, bool omitIndexForSingle) {
    std::vector<Support *> left;
    std::vector<Support *> right;
    for (Support *support : items) {
      if (!support)
        continue;
      if (support->transform.o[0] <= 0.0f)
        left.push_back(support);
      else
        right.push_back(support);
    }
    std::sort(left.begin(), left.end(), sortByX);
    std::sort(right.begin(), right.end(), sortByX);

    for (size_t i = 0; i < left.size(); ++i) {
      Support *support = left[i];
          support->name = BuildIndexedName(leftPrefix, static_cast<int>(i + 1),
                                           omitIndexForSingle);
      support->motorName = support->name;
    }
    for (size_t i = 0; i < right.size(); ++i) {
      Support *support = right[i];
          support->name = BuildIndexedName(rightPrefix, static_cast<int>(i + 1),
                                           omitIndexForSingle);
      support->motorName = support->name;
    }
  };

  assignLeftRightNames(sidefillSupports, "SF L", "SF R", true);
  assignLeftRightNames(paSupports, "PA L", "PA R", false);

  std::sort(otherSupports.begin(), otherSupports.end(), sortByX);
  for (size_t i = 0; i < otherSupports.size(); ++i) {
    Support *support = otherSupports[i];
    support->name = "RP " + std::to_string(i + 1);
    support->motorName = support->name;
  }
}

std::string PickDummyHoistProfileId(float capacityKg) {
  if (capacityKg >= 1500.0f)
    return "dummy_standard_2000kg";
  if (capacityKg >= 750.0f)
    return "dummy_standard_1000kg";
  return "dummy_standard_500kg";
}

struct ParsedHoistLine {
  int quantity = 0;
  float capacityKg = 0.0f;
  std::string target;
};

bool TryParseHoistLine(const std::string &line, const std::string &currentHang,
                       ParsedHoistLine &out) {
  std::smatch match;
  if (!std::regex_match(line, match, kHoistLineRe))
    return false;

  int quantity = 0;
  if (!TryParseInt(match[1].str(), quantity) || quantity <= 0)
    return false;

  const std::string tail = match[2].str();
  const float capacityKg = ParseHoistCapacityKg(tail);
  if (capacityKg <= 0.0f)
    return false;

  std::string hang = currentHang;
  std::smatch targetMatch;
  if (std::regex_search(tail, targetMatch, kHoistTargetRe) &&
      targetMatch.size() > 1) {
    hang = targetMatch[1].str();
  }
  hang = NormalizeHangName(hang);
  if (hang.empty())
    hang = "LX";
  if (hang == "FLOOR")
    return false;

  out.quantity = quantity;
  out.capacityKg = capacityKg;
  out.target = hang;
  return true;
}

std::string ResolveHoistFunctionForTarget(const std::string &target) {
  const std::string normalized = NormalizeHangName(target);
  if (normalized == "PA" || normalized == "P.A." || normalized == "SIDEFILL" ||
      normalized == "OUTFILL")
    return "Audio";
  if (normalized == "SCREEN" || normalized == "LEDSCREEN" ||
      normalized == "VIDEO")
    return "Video";
  return "Lighting";
}

std::string ResolveHoistLayerColor(const std::string &hoistFunctionRaw) {
  const std::string hoistFunction = NormalizeHoistFunction(hoistFunctionRaw);
  if (hoistFunction == "Audio")
    return "#FF0000";
  if (hoistFunction == "Video")
    return "#00FF00";
  if (hoistFunction == "Scenic")
    return "#0000FF";
  if (hoistFunction == "Extra")
    return "#8F00FF";
  if (hoistFunction == "Other")
    return "#C7A3C7";
  return "#FF00FF"; // Lighting (default).
}

// Performs a case-insensitive substring search without lowercasing the entire
// haystack. This keeps the per-line processing in Import() cheap while still
// matching section headers regardless of their capitalization.
bool ContainsCaseInsensitive(std::string_view haystack,
                             std::string_view needle) {
  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(),
                        needle.end(), [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                        });
  return it != haystack.end();
}

bool IsFloorAlias(std::string_view value) {
  const bool effectAlias = ContainsCaseInsensitive(value, "efecto");
  const bool floorAlias = ContainsCaseInsensitive(value, "floor");
  const bool streetToFloorAlias = ContainsCaseInsensitive(value, "calle") &&
                                  ContainsCaseInsensitive(value, "suelo");
  const bool groundLaneAlias = ContainsCaseInsensitive(value, "ground") &&
                               ContainsCaseInsensitive(value, "lane");
  return effectAlias || floorAlias || streetToFloorAlias || groundLaneAlias;
}

bool IsLxSidesAlias(std::string_view value) {
  const bool hasStreetAlias = ContainsCaseInsensitive(value, "calle");
  const bool hasSideAlias = ContainsCaseInsensitive(value, "side");
  const bool hasFloorAlias = ContainsCaseInsensitive(value, "suelo") ||
                             ContainsCaseInsensitive(value, "ground");
  return (hasStreetAlias || hasSideAlias) && !hasFloorAlias;
}

std::string ReadTextFile(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs)
    return {};
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::vector<float> SplitTrussSymmetric(float total) {
  const std::array<float, 4> sizes = {3000.0f, 2000.0f, 1000.0f, 500.0f};
  const std::array<float, 5> centers = {0.0f, 500.0f, 1000.0f, 2000.0f,
                                        3000.0f};

  float discrete = std::floor(total / 500.0f) * 500.0f;
  float leftover = total - discrete;

  std::vector<float> best;
  std::tuple<int, int, float> bestCost{std::numeric_limits<int>::max(),
                                       std::numeric_limits<int>::max(),
                                       std::numeric_limits<float>::max()};

  std::vector<float> current;
  std::vector<std::vector<float>> halfCombs;
  std::function<void(float, size_t)> dfs = [&](float target, size_t idx) {
    if (target < -1e-3f)
      return;
    if (std::abs(target) < 1e-3f) {
      halfCombs.push_back(current);
      return;
    }
    for (size_t i = idx; i < sizes.size(); ++i) {
      current.push_back(sizes[i]);
      dfs(target - sizes[i], i);
      current.pop_back();
    }
  };

  for (float c : centers) {
    if (c > discrete)
      continue;
    float rem = discrete - c;
    if (std::fmod(rem, 1000.0f) != 0.0f)
      continue;
    float half = rem / 2.0f;
    halfCombs.clear();
    current.clear();
    dfs(half, 0);
    for (const auto &left : halfCombs) {
      std::vector<float> pieces;
      pieces.insert(pieces.end(), left.begin(), left.end());
      if (c > 0.0f)
        pieces.push_back(c);
      for (auto it = left.rbegin(); it != left.rend(); ++it)
        pieces.push_back(*it);

      int pieceCount = static_cast<int>(pieces.size());
      std::unordered_set<int> distinct;
      float minPiece = std::numeric_limits<float>::max();
      for (float s : pieces) {
        distinct.insert(static_cast<int>(s));
        if (s < minPiece)
          minPiece = s;
      }
      std::tuple<int, int, float> cost{
          pieceCount, static_cast<int>(distinct.size()), -minPiece};
      if (cost < bestCost) {
        bestCost = cost;
        best = pieces;
      }
    }
  }

  if (best.empty() && discrete > 0.0f)
    best.push_back(discrete);

  if (leftover > 1.0f)
    best.push_back(leftover);

  if (best.empty())
    best.push_back(total);

  return best;
}

std::vector<std::string>
BuildTrussDictionaryLookupKeys(const std::string &modelToken,
                               const std::string &trussName) {
  std::vector<std::string> keys;
  auto pushNormalized = [&](const std::string &key) {
    const std::string normalized = TrussDictionary::NormalizeModelKey(key);
    if (normalized.empty())
      return;
    if (std::find(keys.begin(), keys.end(), normalized) == keys.end())
      keys.push_back(normalized);
  };

  auto simplifyModelToken = [](std::string token) {
    token = TrussDictionary::NormalizeModelKey(token);
    if (token.empty())
      return token;

    static const std::unordered_set<std::string> kFinishWords = {
        "NEGRO", "NEGRA", "NEGRAS", "NEGROS", "BLACK",
        "PLATA", "SILVER", "ALUMINIO", "ALUMINUM"};

    std::istringstream iss(token);
    std::string word;
    std::vector<std::string> kept;
    while (iss >> word) {
      if (kFinishWords.find(word) != kFinishWords.end())
        continue;
      kept.push_back(word);
    }
    if (kept.empty())
      return token;
    std::ostringstream oss;
    for (size_t i = 0; i < kept.size(); ++i) {
      if (i > 0)
        oss << ' ';
      oss << kept[i];
    }
    return oss.str();
  };

  pushNormalized(modelToken);
  if (!modelToken.empty())
    pushNormalized("TRUSS " + modelToken);
  const std::string simplifiedModelToken = simplifyModelToken(modelToken);
  if (!simplifiedModelToken.empty() &&
      simplifiedModelToken != TrussDictionary::NormalizeModelKey(modelToken)) {
    pushNormalized(simplifiedModelToken);
    pushNormalized("TRUSS " + simplifiedModelToken);
  }
  pushNormalized(trussName);

  return keys;
}

// ExtractPdfText moved to pdftext.cpp
} // namespace

// Loads rider text from supported text or PDF file paths.
std::string RiderImporter::LoadText(const std::string &path) {
  std::string ext;
  const size_t dotPos = path.find_last_of('.');
  if (dotPos != std::string::npos)
    ext = path.substr(dotPos);
  for (auto &c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (ext == ".txt")
    return ReadTextFile(path);
  if (ext == ".pdf")
    return ExtractPdfText(path);
  return {};
}

// Imports scene data from a rider file path.
bool RiderImporter::Import(const std::string &path,
                           ProgressCallback progressCallback) {
  std::string text = LoadText(path);
  if (text.empty())
    return false;
  return ImportText(text, std::move(progressCallback));
}

struct ParsedRiderImport {
  std::vector<std::string> fixtureRequests;
  std::vector<std::string> trussRequests;
  std::vector<std::string> pipeRequests;
  std::vector<ParsedHoistLine> hoistRequests;
  std::vector<std::string> screenRequests;
  std::vector<std::string> hangMetadata;
  std::string filteredPreviewText;
};

// Parses raw rider text into filtered import requests and preview metadata.
ParsedRiderImport ParseRiderImport(const std::string &text) {
  ParsedRiderImport parsed;
  if (text.empty())
    return parsed;

  std::istringstream iss(text);
  std::string line;
  bool inFixtures = false;
  bool inRigging = false;
  bool inControl = false;
  bool seenSectionHeader = false;
  bool havePending = false;
  int pendingQuantity = 0;
  std::string currentHang;
  std::vector<std::string> hangOrder;
  std::unordered_map<std::string, std::vector<std::string>> fixturesByHang;
  std::unordered_map<std::string, std::string> hangCoordinateSuffixByHang;
  std::unordered_map<std::string, std::string> hangMarginSuffixByHang;
  std::vector<std::string> riggingLines;
  struct HoistPreviewRequest {
    int quantity = 0;
    float capacityKg = 0.0f;
    std::string target;
  };
  std::vector<HoistPreviewRequest> hoistRequests;
  std::vector<std::string> lxTargetsInRigging;

  auto normalizeFixtureToken = [](const std::string &token) {
    std::string normalized = token;
    normalized = std::regex_replace(normalized, kParenthesizedCleanupRe, "");
    normalized = std::regex_replace(normalized, kHyphenSpacingCleanupRe, "-");
    normalized = std::regex_replace(normalized, kWhitespaceCleanupRe, " ");
    return Trim(normalized);
  };

  auto appendFixtureLines = [&](int baseQuantity, const std::string &descRaw) {
    const std::string hang = currentHang.empty() ? "UNASSIGNED" : currentHang;
    ForEachSplitPlusPart(descRaw, [&](std::string_view partRawView) {
      std::smatch pm;
      std::string partRaw(partRawView);
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, kFixtureLineRe)) {
        if (!TryParseInt(pm[1].str(), quantity))
          quantity = baseQuantity;
        part = Trim(pm[2]);
      }
      part = normalizeFixtureToken(part);
      if (part.empty())
        return;

      auto &bucket = fixturesByHang[hang];
      if (bucket.empty())
        hangOrder.push_back(hang);
      const std::string fixtureRequest = std::to_string(quantity) + " " + part;
      bucket.push_back(fixtureRequest);
      parsed.fixtureRequests.push_back(hang + "\n" + fixtureRequest);
      if (NormalizeHangName(hang) == "SCREEN")
        parsed.screenRequests.push_back(fixtureRequest);
    });
  };

  auto formatLengthM = [](float meters) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << meters;
    std::string length = oss.str();
    length.erase(length.find_last_not_of('0') + 1, std::string::npos);
    if (!length.empty() && length.back() == '.')
      length.pop_back();
    return length;
  };

  while (std::getline(iss, line)) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line = StripRiderAnnotations(line);
    if (line.empty()) {
      havePending = false;
      continue;
    }
    const std::string lowerLine = [&line]() {
      std::string lowered = line;
      std::transform(
          lowered.begin(), lowered.end(), lowered.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return lowered;
    }();

    if (lowerLine.find("sonido") != std::string::npos ||
        lowerLine.find("audio") != std::string::npos ||
        lowerLine.find("control de p.a.") != std::string::npos ||
        lowerLine.find("monitores") != std::string::npos ||
        lowerLine.find("microfon") != std::string::npos ||
        lowerLine.find("video") != std::string::npos ||
        lowerLine.find("realizacion") != std::string::npos ||
        lowerLine.find("control") != std::string::npos) {
      seenSectionHeader = true;
      inFixtures = false;
      inRigging = false;
      inControl = lowerLine.find("control") != std::string::npos;
      havePending = false;
      continue;
    }
    if (lowerLine.find("rigging") != std::string::npos) {
      seenSectionHeader = true;
      inFixtures = false;
      inRigging = true;
      inControl = false;
      havePending = false;
      continue;
    }
    if (!inControl && (lowerLine.find("ilumin") != std::string::npos ||
                       lowerLine.find("robotica") != std::string::npos ||
                       lowerLine.find("convencion") != std::string::npos)) {
      seenSectionHeader = true;
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }

    std::smatch m;
    std::smatch hm;
    if (std::regex_match(line, hm, kHangLineRe) ||
        std::regex_match(line, hm, kHangHeaderWithSuffixRe)) {
      havePending = false;
      std::string captured = hm[1];
      if (IsFloorAlias(captured)) {
        currentHang = "FLOOR";
      } else {
        currentHang = NormalizeHangName(captured);
      }
      if (const auto coordinateSuffix = ExtractParenthesizedToken(line);
          coordinateSuffix.has_value()) {
        hangCoordinateSuffixByHang[currentHang] = *coordinateSuffix;
      }
      if (const auto marginSuffix = ExtractBracketedToken(line);
          marginSuffix.has_value()) {
        hangMarginSuffixByHang[currentHang] = *marginSuffix;
      }
      parsed.hangMetadata.push_back(line);
      if (!inRigging && !inFixtures)
        inFixtures = true;
      continue;
    }

    if (havePending && inFixtures) {
      const std::string desc = Trim(line);
      if (!desc.empty())
        appendFixtureLines(pendingQuantity, desc);
      havePending = false;
      continue;
    }

    if (std::regex_match(line, m, kTrussLineRe)) {
      const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
      const std::string riggingKeyword =
          riggingKind == RiggingLineKind::Pipe ? "PIPE" : "TRUSS";
      int quantity = 0;
      if (!TryParseInt(m[1].str(), quantity) || quantity <= 0)
        continue;
      std::string model = Trim(m[2]);
      float lengthM = 0.0f;
      if (!TryParseFloat(m[3].str(), lengthM))
        continue;
      std::string hang = currentHang;
      std::string trussCoordinateSuffix;
      std::string trussMarginSuffix;
      if (m.size() > 4 && m[4].matched) {
        hang = m[4].str();
        if (const auto coordinateSuffix = ExtractParenthesizedToken(hang);
            coordinateSuffix.has_value()) {
          trussCoordinateSuffix = *coordinateSuffix;
        }
        if (const auto marginSuffix = ExtractBracketedToken(hang);
            marginSuffix.has_value()) {
          trussMarginSuffix = *marginSuffix;
        }
      } else if (std::regex_match(model, kHangOnlyRe)) {
        hang = model;
        model.clear();
      } else {
        std::string modelForHang = model;
        if (const auto coordinateSuffix =
                ExtractParenthesizedToken(modelForHang);
            coordinateSuffix.has_value()) {
          trussCoordinateSuffix = *coordinateSuffix;
        }
        if (const auto marginSuffix = ExtractBracketedToken(modelForHang);
            marginSuffix.has_value()) {
          trussMarginSuffix = *marginSuffix;
        }
        modelForHang =
            std::regex_replace(modelForHang, kParenthesizedCleanupRe, "");
        modelForHang =
            std::regex_replace(modelForHang, kBracketedCleanupRe, "");
        modelForHang = Trim(modelForHang);
        if (std::regex_match(modelForHang, kHangOnlyRe)) {
          hang = modelForHang;
          model.clear();
        }
      }
      const std::string normalizedHang = NormalizeHangName(hang);
      if (trussCoordinateSuffix.empty()) {
        const auto it = hangCoordinateSuffixByHang.find(normalizedHang);
        if (it != hangCoordinateSuffixByHang.end())
          trussCoordinateSuffix = it->second;
      }
      if (trussMarginSuffix.empty()) {
        const auto it = hangMarginSuffixByHang.find(normalizedHang);
        if (it != hangMarginSuffixByHang.end())
          trussMarginSuffix = it->second;
      }
      hang = ResolveHangTargetFallback(hang);
      if (hang.empty())
        hang = NormalizeHangName(currentHang);

      // Do not keep floor trusses in filtered output because those are not
      // useful for the target lighting rigging workflow.
      if (hang == "FLOOR")
        continue;

      const std::string lenText = formatLengthM(lengthM) + "m";
      auto buildLine = [&](const std::string &targetHang) {
        std::string out = "1 " + riggingKeyword;
        if (!model.empty())
          out += " " + model;
        out += " " + lenText;
        if (!targetHang.empty())
          out += " " + targetHang;
        if (!trussCoordinateSuffix.empty())
          out += " " + trussCoordinateSuffix;
        if (!trussMarginSuffix.empty())
          out += " " + trussMarginSuffix;
        riggingLines.push_back(out);
        if (riggingKind == RiggingLineKind::Pipe)
          parsed.pipeRequests.push_back(out);
        else
          parsed.trussRequests.push_back(out);
        if (targetHang.rfind("LX", 0) == 0 &&
            std::find(lxTargetsInRigging.begin(), lxTargetsInRigging.end(),
                      targetHang) == lxTargetsInRigging.end()) {
          lxTargetsInRigging.push_back(targetHang);
        }
      };

      if (hang == "LX") {
        for (int i = 0; i < quantity; ++i)
          buildLine("LX" + std::to_string(i + 1));
      } else {
        for (int i = 0; i < quantity; ++i)
          buildLine(hang);
      }
      continue;
    }

    if (std::regex_match(line, m, kTrussLineNoLengthRe) &&
        !std::regex_search(line, kLengthWithUnitRe)) {
      const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
      const std::string riggingKeyword =
          riggingKind == RiggingLineKind::Pipe ? "PIPE" : "TRUSS";
      int quantity = 0;
      if (!TryParseInt(m[1].str(), quantity) || quantity <= 0)
        continue;
      std::string model = Trim(m[2]);
      std::string hang = currentHang;
      std::string trussCoordinateSuffix;
      std::string trussMarginSuffix;
      if (m.size() > 3 && m[3].matched) {
        hang = m[3].str();
        if (const auto coordinateSuffix = ExtractParenthesizedToken(hang);
            coordinateSuffix.has_value()) {
          trussCoordinateSuffix = *coordinateSuffix;
        }
        if (const auto marginSuffix = ExtractBracketedToken(hang);
            marginSuffix.has_value()) {
          trussMarginSuffix = *marginSuffix;
        }
      } else {
        std::smatch targetMatch;
        if (std::regex_search(model, targetMatch, kHoistTargetRe) &&
            targetMatch.size() > 1) {
          hang = targetMatch[1].str();
          model = Trim(
              model.substr(0, static_cast<size_t>(targetMatch.position(0))));
        } else if (std::string trailingCoordinateSuffix;
                   TryExtractTrailingHangWithCoordinate(
                       model, hang, trailingCoordinateSuffix)) {
          if (trussCoordinateSuffix.empty())
            trussCoordinateSuffix = trailingCoordinateSuffix;
        } else if (std::regex_match(model, kHangOnlyRe)) {
          hang = model;
          model.clear();
        }
      }
      if (trussCoordinateSuffix.empty()) {
        const auto it =
            hangCoordinateSuffixByHang.find(NormalizeHangName(hang));
        if (it != hangCoordinateSuffixByHang.end())
          trussCoordinateSuffix = it->second;
      }
      if (trussMarginSuffix.empty()) {
        const auto it = hangMarginSuffixByHang.find(NormalizeHangName(hang));
        if (it != hangMarginSuffixByHang.end())
          trussMarginSuffix = it->second;
      }
      hang = ResolveHangTargetFallback(hang);
      if (hang.empty())
        hang = NormalizeHangName(currentHang);
      const bool keepLine =
          riggingKind == RiggingLineKind::Pipe || hang == "BACKDROP";
      if (!keepLine)
        continue;

      for (int i = 0; i < quantity; ++i) {
        std::string out = "1 " + riggingKeyword;
        if (!model.empty())
          out += " " + model;
        if (riggingKind == RiggingLineKind::Pipe)
          out += " 14m";
        if (!hang.empty())
          out += " " + hang;
        if (!trussCoordinateSuffix.empty())
          out += " " + trussCoordinateSuffix;
        if (!trussMarginSuffix.empty())
          out += " " + trussMarginSuffix;
        riggingLines.push_back(out);
        if (riggingKind == RiggingLineKind::Pipe)
          parsed.pipeRequests.push_back(out);
        else
          parsed.trussRequests.push_back(out);
      }
      continue;
    }

    if (std::regex_search(line, m, kTrussRe)) {
      const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
      const std::string riggingKeyword =
          riggingKind == RiggingLineKind::Pipe ? "PIPE" : "TRUSS";
      float lengthM = 0.0f;
      if (!TryParseFloat(m[1].str(), lengthM))
        continue;
      std::string hang = NormalizeHangName(currentHang);
      if (std::regex_search(line, hm, kHangFindRe))
        hang = ResolveHangTargetFallback(hm[1].str());
      if (hang.empty())
        hang = NormalizeHangName(currentHang);
      if (hang == "FLOOR")
        continue;
      std::string out =
          "1 " + riggingKeyword + " " + formatLengthM(lengthM) + "m";
      if (!hang.empty())
        out += " " + hang;
      riggingLines.push_back(out);
      if (riggingKind == RiggingLineKind::Pipe)
        parsed.pipeRequests.push_back(out);
      else
        parsed.trussRequests.push_back(out);
      if (hang.rfind("LX", 0) == 0 &&
          std::find(lxTargetsInRigging.begin(), lxTargetsInRigging.end(),
                    hang) == lxTargetsInRigging.end()) {
        lxTargetsInRigging.push_back(hang);
      }
      continue;
    }

    ParsedHoistLine parsedHoist;
    if (TryParseHoistLine(line, currentHang, parsedHoist)) {
      hoistRequests.push_back(
          {parsedHoist.quantity, parsedHoist.capacityKg, parsedHoist.target});
      parsed.hoistRequests.push_back(parsedHoist);
      continue;
    }

    const bool canAssumeHeaderlessFixtureList =
        !seenSectionHeader && !inFixtures && !inControl && !inRigging;
    if ((inFixtures || canAssumeHeaderlessFixtureList) &&
        std::regex_match(line, m, kFixtureLineRe)) {
      if (canAssumeHeaderlessFixtureList)
        inFixtures = true;
      if (currentHang.empty())
        currentHang = "FLOOR";
      int baseQuantity = 0;
      if (!TryParseInt(m[1].str(), baseQuantity))
        continue;
      appendFixtureLines(baseQuantity, Trim(m[2]));
      continue;
    }

    if ((inFixtures || canAssumeHeaderlessFixtureList) &&
        std::regex_match(line, m, kQuantityOnlyRe)) {
      if (canAssumeHeaderlessFixtureList)
        inFixtures = true;
      if (currentHang.empty())
        currentHang = "FLOOR";
      if (!TryParseInt(m[1].str(), pendingQuantity))
        continue;
      havePending = true;
    }
  }

  std::ostringstream preview;
  bool firstSection = true;
  for (const std::string &hang : hangOrder) {
    auto it = fixturesByHang.find(hang);
    if (it == fixturesByHang.end() || it->second.empty())
      continue;
    if (!firstSection)
      preview << "\n\n";
    preview << hang;
    const auto coordinateIt = hangCoordinateSuffixByHang.find(hang);
    if (coordinateIt != hangCoordinateSuffixByHang.end() &&
        !coordinateIt->second.empty())
      preview << " " << coordinateIt->second;
    const auto marginIt = hangMarginSuffixByHang.find(hang);
    if (marginIt != hangMarginSuffixByHang.end() && !marginIt->second.empty())
      preview << " " << marginIt->second;
    for (const std::string &fixtureLine : it->second)
      preview << "\n" << fixtureLine;
    firstSection = false;
  }
  if (!riggingLines.empty() || !hoistRequests.empty()) {
    std::sort(lxTargetsInRigging.begin(), lxTargetsInRigging.end(),
              [](const std::string &a, const std::string &b) {
                return ParseTrailingNumber(a) < ParseTrailingNumber(b);
              });
    if (lxTargetsInRigging.empty())
      lxTargetsInRigging.push_back("LX1");

    std::vector<std::string> hoistLines;
    auto addHoistLine = [&](int quantity, float capacityKg,
                            const std::string &target) {
      if (quantity <= 0)
        return;
      std::ostringstream capText;
      capText << std::fixed << std::setprecision(0) << std::round(capacityKg);
      hoistLines.push_back("MOTOR " + capText.str());
      hoistLines.back() = std::to_string(quantity) + " " + hoistLines.back() +
                          "Kg FOR " + target;
    };
    for (const HoistPreviewRequest &request : hoistRequests) {
      if (request.target == "LX") {
        const int base =
            request.quantity / static_cast<int>(lxTargetsInRigging.size());
        const int remainder =
            request.quantity % static_cast<int>(lxTargetsInRigging.size());
        for (size_t i = 0; i < lxTargetsInRigging.size(); ++i)
          addHoistLine(base + (static_cast<int>(i) < remainder ? 1 : 0),
                       request.capacityKg, lxTargetsInRigging[i]);
      } else if (request.target == "PA") {
        addHoistLine(request.quantity, request.capacityKg, "P.A.");
      } else {
        addHoistLine(request.quantity, request.capacityKg, request.target);
      }
    }

    if (!firstSection)
      preview << "\n\n";
    preview << "RIGGING";
    for (const std::string &hoistLine : hoistLines)
      preview << "\n" << hoistLine;
    for (const std::string &rigLine : riggingLines)
      preview << "\n" << rigLine;
  }

  parsed.filteredPreviewText = preview.str();
  return parsed;
}

// Formats a parsed rider import into the existing filtered preview text.
std::string BuildFilteredPreviewText(const ParsedRiderImport &parsed) {
  return parsed.filteredPreviewText;
}

// Produces scene-import text from parsed rider requests without re-filtering
// raw text.
std::string BuildSceneImportText(const ParsedRiderImport &parsed) {
  return parsed.filteredPreviewText;
}

// Builds a filtered rider preview containing fixtures and rigging context.
std::string RiderImporter::BuildFixtureFilterPreview(const std::string &text) {
  return BuildFilteredPreviewText(ParseRiderImport(text));
}

struct ImportedGdtfMetadata {
  std::string parsedFixtureName;
  bool hasProperties = false;
  float weightKg = 0.0f;
  float powerConsumptionW = 0.0f;
  std::string resolvedCategory;
  std::string categorySource;
  std::string categoryReason;
  bool hasChannelDerivedWashHint = false;
};

struct ImportedGdtfMetadataCache {
  std::unordered_map<std::string, ImportedGdtfMetadata> entries;

  // Returns parsed GDTF metadata cached by resolved path and mode.
  const ImportedGdtfMetadata &Get(const MvrScene &scene,
                                  const std::string &resolvedGdtfPath,
                                  const std::string &modeName) {
    const std::string key = resolvedGdtfPath + "\n" + modeName;
    auto found = entries.find(key);
    if (found != entries.end())
      return found->second;

    ImportedGdtfMetadata metadata;
    metadata.parsedFixtureName = Trim(GetGdtfFixtureName(resolvedGdtfPath));
    metadata.hasProperties = GetGdtfProperties(
        resolvedGdtfPath, metadata.weightKg, metadata.powerConsumptionW);

    Fixture categoryProbe;
    categoryProbe.gdtfSpec = resolvedGdtfPath;
    categoryProbe.gdtfMode = modeName;
    EnsureFixtureCategoryForImport(scene, categoryProbe);
    metadata.resolvedCategory = categoryProbe.category;
    metadata.categorySource = categoryProbe.categorySource;
    metadata.categoryReason = categoryProbe.categorySourceReason;
    metadata.hasChannelDerivedWashHint =
        metadata.categoryReason.find("channel hints") != std::string::npos;

    auto [insertedIt, inserted] = entries.emplace(key, std::move(metadata));
    return insertedIt->second;
  }
};

struct ImportedTrussDefinitionResult {
  bool loaded = false;
  Truss parsed;
};

struct ImportedTrussDefinitionCache {
  std::unordered_map<std::string, ImportedTrussDefinitionResult> entries;

  // Returns a stable cache key for a truss definition path.
  std::string BuildKey(const std::string &path) const {
    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::absolute(path, ec);
    if (ec)
      return path;
    return normalized.lexically_normal().string();
  }

  // Loads and caches a truss definition for the current import.
  const ImportedTrussDefinitionResult &Get(const std::string &path) {
    const std::string key = BuildKey(path);
    auto found = entries.find(key);
    if (found != entries.end())
      return found->second;

    ImportedTrussDefinitionResult result;
    result.loaded = LoadTrussDefinition(path, result.parsed);
    auto [insertedIt, inserted] = entries.emplace(key, std::move(result));
    return insertedIt->second;
  }
};

// Applies cached GDTF metadata without overriding rider or dictionary choices.
void ApplyImportedGdtfMetadata(Fixture &fixture,
                               const ImportedGdtfMetadata &metadata) {
  if (!metadata.parsedFixtureName.empty())
    fixture.typeName = metadata.parsedFixtureName;
  if (metadata.hasProperties) {
    if (fixture.weightKg <= 0.0f)
      fixture.weightKg = metadata.weightKg;
    if (fixture.powerConsumptionW <= 0.0f)
      fixture.powerConsumptionW = metadata.powerConsumptionW;
  }
  if (fixture.category.empty() && !metadata.resolvedCategory.empty()) {
    fixture.category = metadata.resolvedCategory;
    fixture.categorySource = metadata.categorySource;
    fixture.categorySourceReason = metadata.categoryReason;
  }
}

// Imports rider text into the current scene.
bool RiderImporter::ImportText(const std::string &text,
                               ProgressCallback progressCallback,
                               bool skipFixtureFilterPreview) {
  if (text.empty())
    return false;
  RiderImportTimingStats timing;
  auto reportProgress = [&](std::string stage, int completed = 0,
                            int total = 0) {
    if (!progressCallback)
      return;
    progressCallback(ProgressState{std::move(stage), completed, total});
  };

  reportProgress(skipFixtureFilterPreview ? "Using filtered text..."
                                          : "Filtering text...",
                 1, 6);
  // Keep scene creation consistent with the dialog preview flow while allowing
  // callers that already hold preview text to avoid repeating the filter pass.
  auto phaseStartedAt = std::chrono::steady_clock::now();
  const ParsedRiderImport parsedImport =
      skipFixtureFilterPreview ? ParsedRiderImport{} : ParseRiderImport(text);
  const std::string parsedImportText = skipFixtureFilterPreview
                                           ? std::string{}
                                           : BuildSceneImportText(parsedImport);
  AddRiderImportPhaseTime(timing, "filter/reduce text", phaseStartedAt);
  const std::string &textToImport =
      parsedImportText.empty() ? text : parsedImportText;
  phaseStartedAt = std::chrono::steady_clock::now();
  auto fixtureDictionaryOpt = GdtfDictionary::Load();
  AddRiderImportPhaseTime(timing, "load fixture dictionary", phaseStartedAt);
  phaseStartedAt = std::chrono::steady_clock::now();
  auto trussDictionaryOpt = TrussDictionary::Load();
  AddRiderImportPhaseTime(timing, "load truss dictionary", phaseStartedAt);
  const std::unordered_map<std::string, GdtfDictionary::Entry>
      emptyFixtureDictionary;
  const auto &fixtureDictionary =
      fixtureDictionaryOpt ? *fixtureDictionaryOpt : emptyFixtureDictionary;
  const std::unordered_map<std::string, std::string> emptyTrussDictionary;
  const auto &trussDictionary =
      trussDictionaryOpt ? *trussDictionaryOpt : emptyTrussDictionary;
  reportProgress("Parsing lines...", 2, 6);
  const int totalInputLines = [&]() {
    if (textToImport.empty())
      return 0;
    const int newlineCount = static_cast<int>(
        std::count(textToImport.begin(), textToImport.end(), '\n'));
    return newlineCount + 1;
  }();
  const int parsingProgressReportEveryLines = [&]() {
    if (totalInputLines <= 0)
      return 1;
    // Keep parsing feedback visibly active by targeting about 30 updates.
    return std::max(1, totalInputLines / 30);
  }();

  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("import rider");
  auto &scene = cfg.GetScene();
  std::string defaultLayer = cfg.GetCurrentLayer();
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool layerByType = modeVal && *modeVal == "type";
  const Units::DistanceUnitSystem distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  std::optional<float> lastLightingTrussPosY;
  std::optional<float> lastLightingTrussPosZ;
  std::optional<float> lastBackdropReferencePosY;
  std::optional<float> lastBackdropReferencePosZ;
  std::optional<float> lastBackdropReferenceLengthMm;
  std::unordered_map<std::string, float> hangMarginOverridesMm;
  ImportedTrussDefinitionCache trussDefinitionCache;

  auto getHangHeight = [&](const std::string &posName) {
    if (posName.rfind("LX", 0) == 0) {
      int idx = 0;
      if (TryParseInt(std::string_view(posName).substr(2), idx) && idx >= 1 &&
          idx <= 6) {
        return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_height") *
               1000.0f;
      }
    }
    if (posName == "SCREEN") {
      if (lastLightingTrussPosZ)
        return *lastLightingTrussPosZ - 500.0f;
      for (int idx = 6; idx >= 1; --idx) {
        const float configuredHeight =
            cfg.GetFloat("rider_lx" + std::to_string(idx) + "_height");
        if (configuredHeight > 0.0f)
          return configuredHeight * 1000.0f - 500.0f;
      }
    }
    if (posName == "BACKDROP") {
      if (lastBackdropReferencePosZ)
        return *lastBackdropReferencePosZ;
      if (lastLightingTrussPosZ)
        return *lastLightingTrussPosZ;
      for (int idx = 6; idx >= 1; --idx) {
        const float configuredHeight =
            cfg.GetFloat("rider_lx" + std::to_string(idx) + "_height");
        if (configuredHeight > 0.0f)
          return configuredHeight * 1000.0f;
      }
    }
    return 0.0f;
  };

  auto getHangPos = [&](const std::string &posName) {
    if (posName.rfind("LX", 0) == 0) {
      int idx = 0;
      if (TryParseInt(std::string_view(posName).substr(2), idx) && idx >= 1 &&
          idx <= 6) {
        return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_pos") *
               1000.0f;
      }
    }
    if (posName == "SCREEN") {
      if (lastLightingTrussPosY)
        return *lastLightingTrussPosY + 1000.0f;
      for (int idx = 6; idx >= 1; --idx) {
        const float configuredPos =
            cfg.GetFloat("rider_lx" + std::to_string(idx) + "_pos");
        if (configuredPos != 0.0f || idx == 1)
          return configuredPos * 1000.0f + 1000.0f;
      }
    }
    if (posName == "BACKDROP") {
      if (lastBackdropReferencePosY)
        return *lastBackdropReferencePosY + 1000.0f;
      if (lastLightingTrussPosY)
        return *lastLightingTrussPosY + 1000.0f;
      for (int idx = 6; idx >= 1; --idx) {
        const float configuredPos =
            cfg.GetFloat("rider_lx" + std::to_string(idx) + "_pos");
        if (configuredPos != 0.0f || idx == 1)
          return configuredPos * 1000.0f + 1000.0f;
      }
    }
    return 0.0f;
  };

  auto getHangMargin = [&](const std::string &posName) {
    if (const auto it = hangMarginOverridesMm.find(posName);
        it != hangMarginOverridesMm.end()) {
      return it->second;
    }
    if (posName.rfind("LX", 0) == 0) {
      int idx = 0;
      if (TryParseInt(std::string_view(posName).substr(2), idx) && idx >= 1 &&
          idx <= 6) {
        return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_margin") *
               1000.0f;
      }
    }
    return 200.0f;
  };

  std::unordered_map<std::string, Layer *> layerLookup;
  layerLookup.reserve(scene.layers.size() + 4);
  for (auto &[id, layer] : scene.layers)
    layerLookup.emplace(layer.name, &layer);

  auto addToLayer = [&](const std::string &lname, const std::string &uid,
                        const std::string &layerColor = std::string()) {
    std::string name = lname.empty() ? DEFAULT_LAYER_NAME : lname;
    Layer *layerPtr = nullptr;
    auto it = layerLookup.find(name);
    if (it != layerLookup.end()) {
      layerPtr = it->second;
    } else {
      Layer l;
      l.uuid = name == DEFAULT_LAYER_NAME ? "layer_default" : GenerateUuid();
      l.name = name;
      l.visualColorHex = layerColor;
      auto [insertedIt, inserted] = scene.layers.emplace(l.uuid, std::move(l));
      layerPtr = &insertedIt->second;
      layerLookup.emplace(layerPtr->name, layerPtr);
    }
    if (!layerColor.empty() && layerPtr->color.empty()) {
      layerPtr->color = layerColor;
    }
    layerPtr->childUUIDs.push_back(uid);
  };

  std::istringstream iss(textToImport);
  std::string line;
  bool inFixtures = false;
  bool inRigging = false;
  bool inControl = false;
  bool seenSectionHeader = false;
  std::string currentHang;
  std::unordered_map<std::string, int> nameCounters;
  std::vector<std::string> typeOrder;
  typeOrder.reserve(16);
  std::unordered_set<std::string> seenTypes;
  std::vector<std::string> importedTrussUuids;
  std::vector<std::string> importedFixtureUuids;
  std::unordered_map<std::string, size_t> importedFixtureOrderByUuid;
  size_t nextImportedFixtureOrder = 0;
  std::unordered_map<std::string, std::unordered_set<std::string>>
      seenFixtureTypesByHangInListing;
  std::unordered_set<std::string> linearPlacementHangs;
  struct PipeSpanInfo {
    std::string positionName;
    float startX = 0.0f;
    float endX = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  };
  std::vector<PipeSpanInfo> importedPipeSpans;
  std::unordered_set<std::string> pipeHangNames;
  struct ScreenObjectRequest {
    std::string name;
    std::string layer;
    std::string positionName;
    float widthMm = 8000.0f;
    float heightMm = 5000.0f;
  };
  std::vector<ScreenObjectRequest> screenObjectRequests;
  struct HoistRequest {
    int quantity = 0;
    float capacityKg = 0.0f;
    std::string target;
  };
  std::vector<HoistRequest> hoistRequests;
  int pendingQuantity = 0;
  bool havePending = false;
  std::unordered_map<std::string, TrussCoordinateOverride>
      hangCoordinateOverrides;
  ImportedGdtfMetadataCache importedGdtfMetadataCache;
  std::unordered_map<std::string, std::optional<GdtfDictionary::Entry>>
      fixtureDictionaryLookupCache;
  std::unordered_map<std::string, std::optional<std::string>>
      trussDictionaryLookupCache;
  int parsedInputLineCount = 0;

  auto lookupFixtureDictionary = [&](const std::string &typeName)
      -> const std::optional<GdtfDictionary::Entry> & {
    auto found = fixtureDictionaryLookupCache.find(typeName);
    if (found != fixtureDictionaryLookupCache.end())
      return found->second;
    ++timing.uniqueFixtureLookups;
    return fixtureDictionaryLookupCache
        .emplace(typeName, GdtfDictionary::FindInLoadedDictionary(
                               fixtureDictionary, typeName))
        .first->second;
  };
  auto lookupTrussDictionary =
      [&](const std::string &lookupKey) -> const std::optional<std::string> & {
    auto found = trussDictionaryLookupCache.find(lookupKey);
    if (found != trussDictionaryLookupCache.end())
      return found->second;
    ++timing.uniqueTrussLookups;
    return trussDictionaryLookupCache
        .emplace(lookupKey, TrussDictionary::FindInLoadedDictionary(
                                trussDictionary, lookupKey))
        .first->second;
  };

  auto addFixtures = [&](int baseQuantity, const std::string &desc) {
    ForEachSplitPlusPart(desc, [&](std::string_view partRawView) {
      std::smatch pm;
      std::string partRaw(partRawView);
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, kFixtureLineRe)) {
        if (!TryParseInt(pm[1].str(), quantity))
          quantity = baseQuantity;
        part = Trim(pm[2]);
      }
      std::string placementKey = Trim(part);
      std::transform(
          placementKey.begin(), placementKey.end(), placementKey.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      auto &seenTypesForHang = seenFixtureTypesByHangInListing[currentHang];
      if (!placementKey.empty() && seenTypesForHang.count(placementKey) > 0)
        linearPlacementHangs.insert(currentHang);
      else if (!placementKey.empty())
        seenTypesForHang.insert(placementKey);
      const bool isScreenHang = NormalizeHangName(currentHang) == "SCREEN";
      const bool isScreenDescription =
          ContainsCaseInsensitive(part, "pantalla") ||
          ContainsCaseInsensitive(part, "screen");
      if (isScreenHang && isScreenDescription) {
        float screenWidthMm = 8000.0f;
        float screenHeightMm = 5000.0f;
        TryParseScreenDimensionsMm(part, screenWidthMm, screenHeightMm);
        int &counter = nameCounters[part];
        for (int i = 0; i < quantity; ++i) {
          ScreenObjectRequest request;
          request.name = part + " " + std::to_string(++counter);
          request.positionName = currentHang;
          request.widthMm = screenWidthMm;
          request.heightMm = screenHeightMm;
          if (layerByType)
            request.layer = "obj " + currentHang;
          else
            request.layer =
                currentHang.empty() ? defaultLayer : "pos " + currentHang;
          screenObjectRequests.push_back(std::move(request));
        }
        return;
      }
      int &counter = nameCounters[part];
      for (int i = 0; i < quantity; ++i) {
        Fixture f;
        f.uuid = GenerateUuid();
        f.instanceName = part + " " + std::to_string(++counter);
        f.typeName = part;
        if (const auto &dictEntry = lookupFixtureDictionary(f.typeName)) {
          f.gdtfSpec = dictEntry->path;
          f.gdtfMode = dictEntry->mode;
          if (!dictEntry->visualColorHex.empty())
            f.visualColorHex = dictEntry->visualColorHex;
          const std::string dictionaryCategory = Trim(dictEntry->category);
          if (!dictionaryCategory.empty()) {
            f.category = dictionaryCategory;
            f.categorySource = GdtfFixtureCategory::kManualSource;
            f.categorySourceReason.clear();
          }
          const std::string resolvedGdtfPath =
              ResolveGdtfPath(scene, f.gdtfSpec);
          phaseStartedAt = std::chrono::steady_clock::now();
          const ImportedGdtfMetadata &metadata = importedGdtfMetadataCache.Get(
              scene, resolvedGdtfPath, f.gdtfMode);
          AddRiderImportPhaseTime(timing, "resolve GDTF metadata",
                                  phaseStartedAt);
          ApplyImportedGdtfMetadata(f, metadata);
        }
        EnsureFixtureCategoryForImport(scene, f);
        if (!seenTypes.count(f.typeName)) {
          typeOrder.push_back(f.typeName);
          seenTypes.insert(f.typeName);
        }
        std::string fLayer = defaultLayer;
        if (layerByType) {
          if (!f.typeName.empty())
            fLayer = "fix " + f.typeName;
        } else {
          if (!currentHang.empty())
            fLayer = "pos " + currentHang;
        }
        f.layer = fLayer;
        f.positionName = currentHang;
        f.transform.o[1] = getHangPos(currentHang);
        f.transform.o[2] = getHangHeight(currentHang);
        phaseStartedAt = std::chrono::steady_clock::now();
        scene.fixtures[f.uuid] = f;
        AddRiderImportPhaseTime(timing, "create fixtures", phaseStartedAt);
        ++timing.fixtureCount;
        importedFixtureUuids.push_back(f.uuid);
        importedFixtureOrderByUuid[f.uuid] = nextImportedFixtureOrder++;
        addToLayer(f.layer, f.uuid);
      }
    });
  };
  const auto parseStartedAt = std::chrono::steady_clock::now();
  while (std::getline(iss, line)) {
    ++parsedInputLineCount;
    if (totalInputLines > 0 &&
        (parsedInputLineCount == 1 ||
         parsedInputLineCount % parsingProgressReportEveryLines == 0 ||
         parsedInputLineCount == totalInputLines)) {
      reportProgress("Parsing lines... (" +
                         std::to_string(parsedInputLineCount) + "/" +
                         std::to_string(totalInputLines) + ")",
                     2, 6);
    }
    // Remove Windows carriage returns to allow regexes anchored with '$' to
    // match lines extracted from external tools.
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line = StripRiderAnnotations(line);
    if (line.empty()) {
      havePending = false;
      continue;
    }
    if (ContainsCaseInsensitive(line, "sonido") ||
        ContainsCaseInsensitive(line, "audio") ||
        ContainsCaseInsensitive(line, "control de p.a.") ||
        ContainsCaseInsensitive(line, "monitores") ||
        ContainsCaseInsensitive(line, "microfon") ||
        ContainsCaseInsensitive(line, "video") ||
        ContainsCaseInsensitive(line, "realizacion") ||
        ContainsCaseInsensitive(line, "control")) {
      seenSectionHeader = true;
      inFixtures = false;
      inRigging = false;
      inControl = ContainsCaseInsensitive(line, "control");
      havePending = false;
      continue;
    }
    if (ContainsCaseInsensitive(line, "rigging")) {
      seenSectionHeader = true;
      inFixtures = false;
      inRigging = true;
      inControl = false;
      havePending = false;
      continue;
    }
    if (!inControl && (ContainsCaseInsensitive(line, "ilumin") ||
                       ContainsCaseInsensitive(line, "robotica") ||
                       ContainsCaseInsensitive(line, "convencion"))) {
      seenSectionHeader = true;
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }

    std::smatch m;
    std::smatch hm;
    if (std::regex_match(line, hm, kHangLineRe) ||
        std::regex_match(line, hm, kHangHeaderWithSuffixRe)) {
      havePending = false;
      currentHang = NormalizeHangName(hm[1].str());
      std::string hangLineWithOverrides = line;
      if (const auto parsedMarginOverride = ParseTrussMarginOverrideMm(
              hangLineWithOverrides, distanceUnitSystem);
          parsedMarginOverride.has_value()) {
        hangMarginOverridesMm[currentHang] = *parsedMarginOverride;
      }
      if (const auto parsedOverride = ParseTrussCoordinateOverride(
              hangLineWithOverrides, distanceUnitSystem);
          parsedOverride.has_value()) {
        hangCoordinateOverrides[currentHang] = *parsedOverride;
      }
      // If we weren't in any section yet, assume fixtures when a hang position
      // appears
      if (!inRigging && !inFixtures)
        inFixtures = true;
      continue;
    }
    if (havePending && inFixtures) {
      std::string desc = Trim(line);
      if (!desc.empty())
        addFixtures(pendingQuantity, desc);
      havePending = false;
    } else {
      ParsedHoistLine parsedHoist;
      if (TryParseHoistLine(line, currentHang, parsedHoist)) {
        hoistRequests.push_back(
            {parsedHoist.quantity, parsedHoist.capacityKg, parsedHoist.target});
      } else if (std::regex_match(line, m, kTrussLineRe)) {
        const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
        int quantity = 0;
        if (!TryParseInt(m[1].str(), quantity))
          continue;
        std::string model = Trim(m[2]);
        float length = 0.0f;
        if (!TryParseFloat(m[3], length))
          continue;
        length *= 1000.0f;
        float width = 400.0f;
        float height = 400.0f;
        std::smatch dm;
        if (std::regex_search(
                model, dm,
                std::regex(
                    "(\\d+(?:\\.\\d+)?)\\s*[xX]\\s*(\\d+(?:\\.\\d+)?)"))) {
          float parsed = 0.0f;
          if (TryParseFloat(dm[1], parsed))
            width = parsed * 10.0f;
          parsed = 0.0f;
          if (TryParseFloat(dm[2], parsed))
            height = parsed * 10.0f;
        }
        std::string hang = currentHang;
        std::optional<TrussCoordinateOverride> coordinateOverride;
        std::optional<float> marginOverrideMm;
        if (m.size() > 4 && m[4].matched) {
          hang = Trim(m[4]);
          marginOverrideMm =
              ParseTrussMarginOverrideMm(hang, distanceUnitSystem);
          coordinateOverride =
              ParseTrussCoordinateOverride(hang, distanceUnitSystem);
        } else {
          std::string modelForHang = model;
          marginOverrideMm =
              ParseTrussMarginOverrideMm(modelForHang, distanceUnitSystem);
          const auto modelCoordinateOverride =
              ParseTrussCoordinateOverride(modelForHang, distanceUnitSystem);
          if (std::regex_match(modelForHang, kHangOnlyRe)) {
            hang = modelForHang;
            coordinateOverride = modelCoordinateOverride;
            model.clear();
          } else if (modelCoordinateOverride.has_value()) {
            model = modelForHang;
            coordinateOverride = modelCoordinateOverride;
          }
        }
        if (std::regex_match(model, kHangOnlyRe)) {
          hang = model;
          model.clear();
        }
        hang = ResolveHangTargetFallback(hang);
        if (hang.empty())
          hang = NormalizeHangName(currentHang);
        if (hang == "FLOOR")
          continue;
        if (marginOverrideMm.has_value())
          hangMarginOverridesMm[hang] = *marginOverrideMm;

        auto formatLength = [](float mm) {
          std::ostringstream oss;
          oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
          std::string s = oss.str();
          // remove trailing zeros and optional decimal point
          s.erase(s.find_last_not_of('0') + 1, std::string::npos);
          if (!s.empty() && s.back() == '.')
            s.pop_back();
          return s + "M";
        };

        auto getWidestLxSpan = [&]() {
          bool found = false;
          float minX = 0.0f;
          float maxX = 0.0f;
          for (const auto &[existingUuid, existingTruss] : scene.trusses) {
            (void)existingUuid;
            if (!IsLxHangName(existingTruss.positionName))
              continue;
            const float start = existingTruss.transform.o[0];
            const float end = start + existingTruss.lengthMm;
            if (!found) {
              minX = start;
              maxX = end;
              found = true;
            } else {
              minX = std::min(minX, start);
              maxX = std::max(maxX, end);
            }
          }
          if (!found) {
            minX = -3000.0f;
            maxX = 3000.0f;
          }
          return std::pair<float, float>{minX, maxX};
        };

        auto addTrussPieces = [&](const std::string &posName,
                                  const std::optional<TrussCoordinateOverride>
                                      &coordinateOverride) {
          auto pieces = SplitTrussSymmetric(length);
          float total = std::accumulate(pieces.begin(), pieces.end(), 0.0f);
          const bool isLxSides = IsLxSidesHangName(posName);
          float x = coordinateOverride && coordinateOverride->hasX
                        ? coordinateOverride->xMm
                        : -0.5f * total;
          float yStart = coordinateOverride && coordinateOverride->hasY
                             ? coordinateOverride->yMm
                             : -0.5f * total;
          const float hangY = coordinateOverride && coordinateOverride->hasY
                                  ? coordinateOverride->yMm
                                  : getHangPos(posName);
          const float hangZ = coordinateOverride && coordinateOverride->hasZ
                                  ? coordinateOverride->zMm
                                  : getHangHeight(posName);
          for (float s : pieces) {
            Truss t;
            t.uuid = GenerateUuid();
            std::string tLayer = defaultLayer;
            if (layerByType) {
              if (!posName.empty())
                tLayer = "truss " + posName;
            } else {
              if (!posName.empty())
                tLayer = "pos " + posName;
            }
            t.layer = tLayer;
            t.lengthMm = s;
            t.widthMm = width;
            t.heightMm = height;
            t.positionName = posName;
            if (isLxSides) {
              t.transform.u = {0.0f, 1.0f, 0.0f};
              t.transform.v = {-1.0f, 0.0f, 0.0f};
              t.transform.w = {0.0f, 0.0f, 1.0f};
            }
            t.transform.o[0] = x;
            t.transform.o[1] = isLxSides ? yStart : hangY;
            // Position dummy truss so its base sits at the hang height.
            // Real truss models are inserted from their bottom, so using the
            // raw hang height keeps the base aligned when swapping models.
            t.transform.o[2] = hangZ;
            std::string sizeStr = formatLength(s);
            if (model.empty())
              t.name = "TRUSS " + sizeStr;
            else
              t.name = "TRUSS " + model + " " + sizeStr;
            t.model = model.empty() ? TrussDictionary::NormalizeModelKey(t.name)
                                    : TrussDictionary::NormalizeModelKey(model);

            const std::vector<std::string> dictionaryLookupKeys =
                BuildTrussDictionaryLookupKeys(model, t.name);

            std::optional<std::string> dictPath;
            for (const std::string &lookupKey : dictionaryLookupKeys) {
              if (lookupKey.empty())
                continue;
              dictPath = lookupTrussDictionary(lookupKey);
              if (dictPath)
                break;
            }

            if (dictPath) {
              phaseStartedAt = std::chrono::steady_clock::now();
              const ImportedTrussDefinitionResult &definition =
                  trussDefinitionCache.Get(*dictPath);
              AddRiderImportPhaseTime(timing, "resolve truss definitions",
                                      phaseStartedAt);
              if (definition.loaded) {
                const Truss &parsed = definition.parsed;
                if (!parsed.symbolFile.empty())
                  t.symbolFile = parsed.symbolFile;
                t.modelFile =
                    parsed.modelFile.empty() ? *dictPath : parsed.modelFile;
                t.gdtfSpec = parsed.gdtfSpec;
                t.gdtfMode = parsed.gdtfMode;
                if (!parsed.manufacturer.empty())
                  t.manufacturer = parsed.manufacturer;
                if (!parsed.model.empty())
                  t.model = TrussDictionary::NormalizeModelKey(parsed.model);
                if (parsed.lengthMm > 0.0f)
                  t.lengthMm = parsed.lengthMm;
                if (parsed.widthMm > 0.0f)
                  t.widthMm = parsed.widthMm;
                if (parsed.heightMm > 0.0f)
                  t.heightMm = parsed.heightMm;
                if (parsed.weightKg > 0.0f)
                  t.weightKg = parsed.weightKg;
                if (!parsed.crossSection.empty())
                  t.crossSection = parsed.crossSection;
              } else {
                t.modelFile = *dictPath;
              }
            }
            const std::string trussUuid = t.uuid;
            const std::string trussLayer = t.layer;
            if (isLxSides) {
              const auto [lxStartX, lxEndX] = getWidestLxSpan();
              const float sideOffset = 500.0f;
              for (float sideX : {lxStartX - sideOffset, lxEndX + sideOffset}) {
                Truss sideTruss = t;
                sideTruss.uuid = GenerateUuid();
                sideTruss.transform.o[0] = sideX;
                const std::string sideTrussUuid = sideTruss.uuid;
                phaseStartedAt = std::chrono::steady_clock::now();
                scene.trusses.emplace(sideTrussUuid, std::move(sideTruss));
                AddRiderImportPhaseTime(timing, "create trusses",
                                        phaseStartedAt);
                ++timing.trussCount;
                importedTrussUuids.push_back(sideTrussUuid);
                addToLayer(trussLayer, sideTrussUuid);
              }
            } else {
              phaseStartedAt = std::chrono::steady_clock::now();
              scene.trusses.emplace(trussUuid, std::move(t));
              AddRiderImportPhaseTime(timing, "create trusses", phaseStartedAt);
              ++timing.trussCount;
              importedTrussUuids.push_back(trussUuid);
              addToLayer(trussLayer, trussUuid);
            }
            if (IsLxHangName(posName)) {
              lastLightingTrussPosY = hangY;
              lastLightingTrussPosZ = hangZ;
            }
            x += s;
            yStart += s;
          }
          if (IsLxHangName(posName) || posName == "SCREEN") {
            lastBackdropReferencePosY = hangY;
            lastBackdropReferencePosZ = hangZ;
            lastBackdropReferenceLengthMm = total;
          }
        };

        auto addPipeObject = [&](const std::string &posName,
                                 const std::optional<TrussCoordinateOverride>
                                     &coordinateOverride) {
          const float pipeLengthMm = length;
          const float pipeDiameterMm = 50.0f;
          const float primitiveCylinderHeightMm = 1000.0f;
          const float startX = coordinateOverride && coordinateOverride->hasX
                                   ? coordinateOverride->xMm
                                   : -0.5f * pipeLengthMm;
          const float hangY = coordinateOverride && coordinateOverride->hasY
                                  ? coordinateOverride->yMm
                                  : getHangPos(posName);
          const float hangZ = coordinateOverride && coordinateOverride->hasZ
                                  ? coordinateOverride->zMm
                                  : getHangHeight(posName);

          SceneObject pipeObject;
          pipeObject.uuid = GenerateUuid();
          pipeObject.layer =
              layerByType ? ("obj " + posName) : ("pos " + posName);
          pipeObject.name = "PIPE " + posName;
          pipeObject.transform = BuildCylinderPitchY90LocalTransform();
          pipeObject.transform.o[0] = startX + 0.5f * pipeLengthMm;
          pipeObject.transform.o[1] = hangY;
          pipeObject.transform.o[2] = hangZ;
          pipeObject.geometries.push_back(
              {kPrimitiveCylinderToken,
               BuildPipeObjectTransform(pipeLengthMm, pipeDiameterMm,
                                        primitiveCylinderHeightMm)});

          importedPipeSpans.push_back(
              {posName, startX, startX + pipeLengthMm, hangY, hangZ});
          pipeHangNames.insert(posName);
          scene.sceneObjects.emplace(pipeObject.uuid, pipeObject);
          addToLayer(pipeObject.layer, pipeObject.uuid);
        };

        auto resolveCoordinateOverride = [&](const std::string &posName) {
          TrussCoordinateOverride resolved;
          bool hasResolved = false;
          if (const auto hangOverrideIt = hangCoordinateOverrides.find(posName);
              hangOverrideIt != hangCoordinateOverrides.end()) {
            resolved = hangOverrideIt->second;
            hasResolved = true;
          }
          if (coordinateOverride.has_value()) {
            if (coordinateOverride->hasX) {
              resolved.hasX = true;
              resolved.xMm = coordinateOverride->xMm;
            }
            if (coordinateOverride->hasY) {
              resolved.hasY = true;
              resolved.yMm = coordinateOverride->yMm;
            }
            if (coordinateOverride->hasZ) {
              resolved.hasZ = true;
              resolved.zMm = coordinateOverride->zMm;
            }
            hasResolved = true;
          }
          return hasResolved ? std::optional<TrussCoordinateOverride>(resolved)
                             : std::nullopt;
        };

        if (hang == "LX") {
          for (int i = 0; i < quantity; ++i) {
            const std::string posName = "LX" + std::to_string(i + 1);
            if (riggingKind == RiggingLineKind::Pipe) {
              addPipeObject(posName, resolveCoordinateOverride(posName));
            } else {
              addTrussPieces(posName, resolveCoordinateOverride(posName));
            }
          }
        } else {
          for (int i = 0; i < quantity; ++i) {
            if (riggingKind == RiggingLineKind::Pipe) {
              addPipeObject(hang, resolveCoordinateOverride(hang));
            } else {
              addTrussPieces(hang, resolveCoordinateOverride(hang));
            }
          }
        }
      } else if (std::regex_match(line, m, kTrussLineNoLengthRe) &&
                 !std::regex_search(line, kLengthWithUnitRe)) {
        const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
        int quantity = 0;
        if (!TryParseInt(m[1].str(), quantity))
          continue;
        std::string model = Trim(m[2]);
        std::string hang = currentHang;
        std::optional<TrussCoordinateOverride> coordinateOverride;
        std::optional<float> marginOverrideMm;
        if (m.size() > 3 && m[3].matched) {
          hang = Trim(m[3]);
          marginOverrideMm =
              ParseTrussMarginOverrideMm(hang, distanceUnitSystem);
          coordinateOverride =
              ParseTrussCoordinateOverride(hang, distanceUnitSystem);
        } else {
          std::smatch targetMatch;
          if (std::regex_search(model, targetMatch, kHoistTargetRe) &&
              targetMatch.size() > 1) {
            hang = Trim(targetMatch[1].str());
            model = Trim(
                model.substr(0, static_cast<size_t>(targetMatch.position(0))));
            marginOverrideMm =
                ParseTrussMarginOverrideMm(hang, distanceUnitSystem);
            coordinateOverride =
                ParseTrussCoordinateOverride(hang, distanceUnitSystem);
          } else if (std::string trailingCoordinateSuffix;
                     TryExtractTrailingHangWithCoordinate(
                         model, hang, trailingCoordinateSuffix)) {
            if (!trailingCoordinateSuffix.empty())
              hang += " " + trailingCoordinateSuffix;
            marginOverrideMm =
                ParseTrussMarginOverrideMm(hang, distanceUnitSystem);
            coordinateOverride =
                ParseTrussCoordinateOverride(hang, distanceUnitSystem);
          } else if (std::regex_match(model, kHangOnlyRe)) {
            hang = model;
            model.clear();
          }
        }
        hang = ResolveHangTargetFallback(hang);
        if (hang.empty())
          hang = NormalizeHangName(currentHang);
        const bool isPipe = riggingKind == RiggingLineKind::Pipe;
        if (!isPipe && hang != "BACKDROP")
          continue;
        if (marginOverrideMm.has_value())
          hangMarginOverridesMm[hang] = *marginOverrideMm;

        if (isPipe) {
          const float defaultPipeLengthMm = 14000.0f;
          const float pipeDiameterMm = 50.0f;
          const float primitiveCylinderHeightMm = 1000.0f;
          auto addPipeObject = [&](const std::string &posName,
                                   const std::optional<TrussCoordinateOverride>
                                       &resolvedOverride) {
            const float startX = resolvedOverride && resolvedOverride->hasX
                                     ? resolvedOverride->xMm
                                     : -0.5f * defaultPipeLengthMm;
            const float hangY = resolvedOverride && resolvedOverride->hasY
                                    ? resolvedOverride->yMm
                                    : getHangPos(posName);
            const float hangZ = resolvedOverride && resolvedOverride->hasZ
                                    ? resolvedOverride->zMm
                                    : getHangHeight(posName);

            SceneObject pipeObject;
            pipeObject.uuid = GenerateUuid();
            pipeObject.layer =
                layerByType ? ("obj " + posName) : ("pos " + posName);
            pipeObject.name = "PIPE " + posName;
            pipeObject.transform = BuildCylinderPitchY90LocalTransform();
            pipeObject.transform.o[0] = startX + 0.5f * defaultPipeLengthMm;
            pipeObject.transform.o[1] = hangY;
            pipeObject.transform.o[2] = hangZ;
            pipeObject.geometries.push_back(
                {kPrimitiveCylinderToken,
                 BuildPipeObjectTransform(defaultPipeLengthMm, pipeDiameterMm,
                                          primitiveCylinderHeightMm)});
            scene.sceneObjects.emplace(pipeObject.uuid, pipeObject);
            addToLayer(pipeObject.layer, pipeObject.uuid);
            importedPipeSpans.push_back(
                {posName, startX, startX + defaultPipeLengthMm, hangY, hangZ});
            pipeHangNames.insert(posName);
          };

          auto resolveOverride = [&](const std::string &posName) {
            TrussCoordinateOverride resolved;
            bool hasResolved = false;
            if (const auto hangOverrideIt =
                    hangCoordinateOverrides.find(posName);
                hangOverrideIt != hangCoordinateOverrides.end()) {
              resolved = hangOverrideIt->second;
              hasResolved = true;
            }
            if (coordinateOverride.has_value()) {
              if (coordinateOverride->hasX) {
                resolved.hasX = true;
                resolved.xMm = coordinateOverride->xMm;
              }
              if (coordinateOverride->hasY) {
                resolved.hasY = true;
                resolved.yMm = coordinateOverride->yMm;
              }
              if (coordinateOverride->hasZ) {
                resolved.hasZ = true;
                resolved.zMm = coordinateOverride->zMm;
              }
              hasResolved = true;
            }
            return hasResolved
                       ? std::optional<TrussCoordinateOverride>(resolved)
                               : std::nullopt;
          };

          if (hang == "LX") {
            for (int i = 0; i < quantity; ++i) {
              const std::string posName = "LX" + std::to_string(i + 1);
              addPipeObject(posName, resolveOverride(posName));
            }
          } else {
            for (int i = 0; i < quantity; ++i)
              addPipeObject(hang, resolveOverride(hang));
          }
          continue;
        }

        float width = 400.0f;
        float height = 400.0f;
        std::smatch dm;
        if (std::regex_search(
                model, dm,
                std::regex(
                    "(\\d+(?:\\.\\d+)?)\\s*[xX]\\s*(\\d+(?:\\.\\d+)?)"))) {
          float parsed = 0.0f;
          if (TryParseFloat(dm[1], parsed))
            width = parsed * 10.0f;
          parsed = 0.0f;
          if (TryParseFloat(dm[2], parsed))
            height = parsed * 10.0f;
        }

        const float fallbackLengthMm =
            lastBackdropReferenceLengthMm.value_or(12000.0f);
        const float hangY = coordinateOverride && coordinateOverride->hasY
                                ? coordinateOverride->yMm
                                : getHangPos(hang);
        const float hangZ = coordinateOverride && coordinateOverride->hasZ
                                ? coordinateOverride->zMm
                                : getHangHeight(hang);
        for (int q = 0; q < quantity; ++q) {
          auto pieces = SplitTrussSymmetric(fallbackLengthMm);
          float total = std::accumulate(pieces.begin(), pieces.end(), 0.0f);
          float x = coordinateOverride && coordinateOverride->hasX
                        ? coordinateOverride->xMm
                        : -0.5f * total;
          auto formatLength = [](float mm) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
            std::string s = oss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (!s.empty() && s.back() == '.')
              s.pop_back();
            return s + "M";
          };
          for (float s : pieces) {
            Truss t;
            t.uuid = GenerateUuid();
            t.layer = layerByType ? "truss " + hang : "pos " + hang;
            t.lengthMm = s;
            t.widthMm = width;
            t.heightMm = height;
            t.positionName = hang;
            t.transform.o[0] = x;
            t.transform.o[1] = hangY;
            t.transform.o[2] = hangZ;
            const std::string sizeStr = formatLength(s);
            if (model.empty())
              t.name = "TRUSS " + sizeStr;
            else
              t.name = "TRUSS " + model + " " + sizeStr;
            t.model = model.empty() ? TrussDictionary::NormalizeModelKey(t.name)
                                    : TrussDictionary::NormalizeModelKey(model);

            const std::vector<std::string> dictionaryLookupKeys =
                BuildTrussDictionaryLookupKeys(model, t.name);
            std::optional<std::string> dictPath;
            for (const std::string &lookupKey : dictionaryLookupKeys) {
              if (lookupKey.empty())
                continue;
              dictPath = lookupTrussDictionary(lookupKey);
              if (dictPath)
                break;
            }
            if (dictPath) {
              phaseStartedAt = std::chrono::steady_clock::now();
              const ImportedTrussDefinitionResult &definition =
                  trussDefinitionCache.Get(*dictPath);
              AddRiderImportPhaseTime(timing, "resolve truss definitions",
                                      phaseStartedAt);
              if (definition.loaded) {
                const Truss &parsed = definition.parsed;
                if (!parsed.symbolFile.empty())
                  t.symbolFile = parsed.symbolFile;
                t.modelFile =
                    parsed.modelFile.empty() ? *dictPath : parsed.modelFile;
                t.gdtfSpec = parsed.gdtfSpec;
                t.gdtfMode = parsed.gdtfMode;
                if (!parsed.manufacturer.empty())
                  t.manufacturer = parsed.manufacturer;
                if (!parsed.model.empty())
                  t.model = TrussDictionary::NormalizeModelKey(parsed.model);
                if (parsed.lengthMm > 0.0f)
                  t.lengthMm = parsed.lengthMm;
                if (parsed.widthMm > 0.0f)
                  t.widthMm = parsed.widthMm;
                if (parsed.heightMm > 0.0f)
                  t.heightMm = parsed.heightMm;
                if (parsed.weightKg > 0.0f)
                  t.weightKg = parsed.weightKg;
                if (!parsed.crossSection.empty())
                  t.crossSection = parsed.crossSection;
              } else {
                t.modelFile = *dictPath;
              }
            }
            const std::string trussUuid = t.uuid;
            const std::string trussLayer = t.layer;
            phaseStartedAt = std::chrono::steady_clock::now();
            scene.trusses.emplace(trussUuid, std::move(t));
            AddRiderImportPhaseTime(timing, "create trusses", phaseStartedAt);
            ++timing.trussCount;
            importedTrussUuids.push_back(trussUuid);
            addToLayer(trussLayer, trussUuid);
            x += s;
          }
        }
      } else if (std::regex_search(line, m, kTrussRe)) {
        const RiggingLineKind riggingKind = DetectRiggingLineKind(line);
        float length = 0.0f;
        if (!TryParseFloat(m[1], length))
          continue;
        length *= 1000.0f;
        std::string lineWithCoordinateOverride = line;
        const auto lineMarginOverride = ParseTrussMarginOverrideMm(
            lineWithCoordinateOverride, distanceUnitSystem);
        const auto lineCoordinateOverride = ParseTrussCoordinateOverride(
            lineWithCoordinateOverride, distanceUnitSystem);
        std::string hang = currentHang;
        ParseTrussMarginOverrideMm(hang, distanceUnitSystem);
        std::optional<TrussCoordinateOverride> coordinateOverride =
            ParseTrussCoordinateOverride(hang, distanceUnitSystem);
        if (std::regex_search(line, hm, kHangFindRe))
          hang = ResolveHangTargetFallback(hm[1].str());
        if (hang.empty())
          hang = NormalizeHangName(currentHang);
        if (lineMarginOverride.has_value())
          hangMarginOverridesMm[hang] = *lineMarginOverride;
        if (!coordinateOverride.has_value()) {
          const auto hangOverrideIt = hangCoordinateOverrides.find(hang);
          if (hangOverrideIt != hangCoordinateOverrides.end())
            coordinateOverride = hangOverrideIt->second;
        }
        if (lineCoordinateOverride.has_value()) {
          TrussCoordinateOverride merged =
              coordinateOverride.value_or(TrussCoordinateOverride{});
          if (lineCoordinateOverride->hasX) {
            merged.hasX = true;
            merged.xMm = lineCoordinateOverride->xMm;
          }
          if (lineCoordinateOverride->hasY) {
            merged.hasY = true;
            merged.yMm = lineCoordinateOverride->yMm;
          }
          if (lineCoordinateOverride->hasZ) {
            merged.hasZ = true;
            merged.zMm = lineCoordinateOverride->zMm;
          }
          coordinateOverride = merged;
        }
        if (hang == "FLOOR")
          continue;

        if (riggingKind == RiggingLineKind::Pipe) {
          const float pipeLengthMm = length;
          const float pipeDiameterMm = 50.0f;
          const float primitiveCylinderHeightMm = 1000.0f;
          const float startX = coordinateOverride && coordinateOverride->hasX
                                   ? coordinateOverride->xMm
                                   : -0.5f * pipeLengthMm;
          const float hangY = coordinateOverride && coordinateOverride->hasY
                                  ? coordinateOverride->yMm
                                  : getHangPos(hang);
          const float hangZ = coordinateOverride && coordinateOverride->hasZ
                                  ? coordinateOverride->zMm
                                  : getHangHeight(hang);

          SceneObject pipeObject;
          pipeObject.uuid = GenerateUuid();
          pipeObject.layer = layerByType ? ("obj " + hang) : ("pos " + hang);
          pipeObject.name = "PIPE " + hang;
          pipeObject.transform = BuildCylinderPitchY90LocalTransform();
          pipeObject.transform.o[0] = startX + 0.5f * pipeLengthMm;
          pipeObject.transform.o[1] = hangY;
          pipeObject.transform.o[2] = hangZ;
          pipeObject.geometries.push_back(
              {kPrimitiveCylinderToken,
               BuildPipeObjectTransform(pipeLengthMm, pipeDiameterMm,
                                        primitiveCylinderHeightMm)});
          scene.sceneObjects.emplace(pipeObject.uuid, pipeObject);
          addToLayer(pipeObject.layer, pipeObject.uuid);
          importedPipeSpans.push_back(
              {hang, startX, startX + pipeLengthMm, hangY, hangZ});
          pipeHangNames.insert(hang);
          continue;
        }

        auto formatLength = [](float mm) {
          std::ostringstream oss;
          oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
          std::string s = oss.str();
          s.erase(s.find_last_not_of('0') + 1, std::string::npos);
          if (!s.empty() && s.back() == '.')
            s.pop_back();
          return s + "M";
        };

        float width = 400.0f;
        float height = 400.0f;
        auto pieces = SplitTrussSymmetric(length);
        float total = std::accumulate(pieces.begin(), pieces.end(), 0.0f);
        float x = coordinateOverride && coordinateOverride->hasX
                      ? coordinateOverride->xMm
                      : -0.5f * total;
        const float hangY = coordinateOverride && coordinateOverride->hasY
                                ? coordinateOverride->yMm
                                : getHangPos(hang);
        const float hangZ = coordinateOverride && coordinateOverride->hasZ
                                ? coordinateOverride->zMm
                                : getHangHeight(hang);
        for (float s : pieces) {
          Truss t;
          t.uuid = GenerateUuid();
          std::string tLayer = defaultLayer;
          if (layerByType) {
            if (!hang.empty())
              tLayer = "truss " + hang;
          } else {
            if (!hang.empty())
              tLayer = "pos " + hang;
          }
          t.layer = tLayer;
          t.lengthMm = s;
          t.widthMm = width;
          t.heightMm = height;
          t.positionName = hang;
          t.transform.o[0] = x;
          t.transform.o[1] = hangY;
          // Store the hang height directly so the base matches real models
          // that are inserted from the bottom.
          t.transform.o[2] = hangZ;
          std::string sizeStr = formatLength(s);
          t.name = "TRUSS " + sizeStr;
          t.model = TrussDictionary::NormalizeModelKey(t.name);
          const std::vector<std::string> dictionaryLookupKeys =
              BuildTrussDictionaryLookupKeys(t.model, t.name);

          std::optional<std::string> dictPath;
          for (const std::string &lookupKey : dictionaryLookupKeys) {
            dictPath = lookupTrussDictionary(lookupKey);
            if (dictPath)
              break;
          }

          if (dictPath) {
            phaseStartedAt = std::chrono::steady_clock::now();
            const ImportedTrussDefinitionResult &definition =
                trussDefinitionCache.Get(*dictPath);
            AddRiderImportPhaseTime(timing, "resolve truss definitions",
                                    phaseStartedAt);
            if (definition.loaded) {
              const Truss &parsed = definition.parsed;
              if (!parsed.symbolFile.empty())
                t.symbolFile = parsed.symbolFile;
              t.modelFile =
                  parsed.modelFile.empty() ? *dictPath : parsed.modelFile;
              t.gdtfSpec = parsed.gdtfSpec;
              t.gdtfMode = parsed.gdtfMode;
              t.manufacturer = parsed.manufacturer;
              if (parsed.lengthMm > 0.0f)
                t.lengthMm = parsed.lengthMm;
              if (parsed.widthMm > 0.0f)
                t.widthMm = parsed.widthMm;
              if (parsed.heightMm > 0.0f)
                t.heightMm = parsed.heightMm;
              t.weightKg = parsed.weightKg;
              t.crossSection = parsed.crossSection;
            } else {
              t.modelFile = *dictPath;
            }
          }
          const std::string trussUuid = t.uuid;
          const std::string trussLayer = t.layer;
          phaseStartedAt = std::chrono::steady_clock::now();
          scene.trusses.emplace(trussUuid, std::move(t));
          AddRiderImportPhaseTime(timing, "create trusses", phaseStartedAt);
          ++timing.trussCount;
          importedTrussUuids.push_back(trussUuid);
          addToLayer(trussLayer, trussUuid);
          if (IsLxHangName(hang)) {
            lastLightingTrussPosY = hangY;
            lastLightingTrussPosZ = hangZ;
          }
          if (IsLxHangName(hang) || hang == "SCREEN") {
            lastBackdropReferencePosY = hangY;
            lastBackdropReferencePosZ = hangZ;
            lastBackdropReferenceLengthMm = total;
          }
          x += s;
        }
      } else {
        const bool canAssumeHeaderlessFixtureList =
            !seenSectionHeader && !inFixtures && !inControl && !inRigging;
        if ((inFixtures || canAssumeHeaderlessFixtureList) &&
            std::regex_match(line, m, kFixtureLineRe)) {
          if (canAssumeHeaderlessFixtureList)
            inFixtures = true;
          if (currentHang.empty())
            currentHang = "FLOOR";
          int baseQuantity = 0;
          if (!TryParseInt(m[1].str(), baseQuantity))
            continue;
          std::string desc = Trim(m[2]);
          addFixtures(baseQuantity, desc);
        } else if ((inFixtures || canAssumeHeaderlessFixtureList) &&
                   std::regex_match(line, m, kQuantityOnlyRe)) {
          if (canAssumeHeaderlessFixtureList)
            inFixtures = true;
          if (currentHang.empty())
            currentHang = "FLOOR";
          if (!TryParseInt(m[1].str(), pendingQuantity))
            continue;
          havePending = true;
        }
      }
    }
  }

  AddRiderImportPhaseTime(timing, "parse reduced text", parseStartedAt);

  reportProgress("Resolving rigging data...", 3, 6);
  for (const std::string &uuid : importedTrussUuids) {
    auto trussIt = scene.trusses.find(uuid);
    if (trussIt == scene.trusses.end())
      continue;

    Truss &t = trussIt->second;
    const std::filesystem::path resolvedSymbolPath = ResolveTrussSymbolPath(t);
    if (IsRenderableTrussGeometry(t.symbolFile) &&
        std::filesystem::exists(resolvedSymbolPath))
      continue;

    Truss parsed;
    bool resolved = false;
    bool modelDefinitionFailed = false;
    bool gdtfDefinitionFailed = false;
    bool dictionaryDefinitionFailed = false;

    if (!t.modelFile.empty()) {
      phaseStartedAt = std::chrono::steady_clock::now();
      const ImportedTrussDefinitionResult &definition =
          trussDefinitionCache.Get(t.modelFile);
      AddRiderImportPhaseTime(timing, "resolve truss definitions",
                              phaseStartedAt);
      resolved = definition.loaded;
      if (resolved)
        parsed = definition.parsed;
      if (!resolved)
        modelDefinitionFailed = true;
    }
    if (!resolved && !t.gdtfSpec.empty()) {
      phaseStartedAt = std::chrono::steady_clock::now();
      const ImportedTrussDefinitionResult &definition =
          trussDefinitionCache.Get(t.gdtfSpec);
      AddRiderImportPhaseTime(timing, "resolve truss definitions",
                              phaseStartedAt);
      resolved = definition.loaded;
      if (resolved)
        parsed = definition.parsed;
      if (!resolved)
        gdtfDefinitionFailed = true;
    }
    if (!resolved) {
      const std::vector<std::string> dictionaryLookupKeys =
          BuildTrussDictionaryLookupKeys(t.model, t.name);
      for (const std::string &lookupKey : dictionaryLookupKeys) {
        auto dictPath = lookupTrussDictionary(lookupKey);
        if (!dictPath)
          continue;
        phaseStartedAt = std::chrono::steady_clock::now();
        const ImportedTrussDefinitionResult &definition =
            trussDefinitionCache.Get(*dictPath);
        AddRiderImportPhaseTime(timing, "resolve truss definitions",
                                phaseStartedAt);
        resolved = definition.loaded;
        if (resolved)
          parsed = definition.parsed;
        if (!resolved)
          dictionaryDefinitionFailed = true;
        if (resolved && t.modelFile.empty())
          t.modelFile = *dictPath;
        if (resolved)
          break;
      }
    }

    if (!resolved) {
      std::vector<std::string> reasons;
      if (modelDefinitionFailed)
        reasons.emplace_back("LoadTrussDefinition(modelFile) returned false");
      if (gdtfDefinitionFailed)
        reasons.emplace_back("LoadTrussDefinition(gdtfSpec) returned false");
      if (dictionaryDefinitionFailed)
        reasons.emplace_back(
            "LoadTrussDefinition(dictionary model) returned false");
      if (reasons.empty())
        reasons.emplace_back("missing or non-renderable symbolFile");

      std::ostringstream oss;
      oss << "Rider import truss fallback to dummy box: "
          << DescribeTrussForLog(t) << ". Reason: ";
      for (size_t i = 0; i < reasons.size(); ++i) {
        if (i > 0)
          oss << "; ";
        oss << reasons[i];
      }
      Logger::Instance().Log(Logger::Level::Warn, oss.str());
      continue;
    }

    if (!parsed.symbolFile.empty())
      t.symbolFile = parsed.symbolFile;
    if (t.modelFile.empty() && !parsed.modelFile.empty())
      t.modelFile = parsed.modelFile;
    if (t.gdtfSpec.empty() && !parsed.gdtfSpec.empty())
      t.gdtfSpec = parsed.gdtfSpec;
    if (t.gdtfMode.empty() && !parsed.gdtfMode.empty())
      t.gdtfMode = parsed.gdtfMode;
    if (t.manufacturer.empty() && !parsed.manufacturer.empty())
      t.manufacturer = parsed.manufacturer;
    if (!parsed.model.empty())
      t.model = TrussDictionary::NormalizeModelKey(parsed.model);
    if (parsed.lengthMm > 0.0f)
      t.lengthMm = parsed.lengthMm;
    if (parsed.widthMm > 0.0f)
      t.widthMm = parsed.widthMm;
    if (parsed.heightMm > 0.0f)
      t.heightMm = parsed.heightMm;
    if (parsed.weightKg > 0.0f)
      t.weightKg = parsed.weightKg;
    if (!parsed.crossSection.empty())
      t.crossSection = parsed.crossSection;

    const std::filesystem::path resolvedSymbolPathAfterDefinition =
        ResolveTrussSymbolPath(t);
    bool symbolLooksRenderable = IsRenderableTrussGeometry(t.symbolFile);
    bool symbolExists =
        symbolLooksRenderable &&
        std::filesystem::exists(resolvedSymbolPathAfterDefinition);
    if (!symbolExists) {
      std::ostringstream reason;
      if (t.symbolFile.empty()) {
        reason << "symbolFile is empty";
      } else if (!symbolLooksRenderable) {
        reason << "symbolFile extension is not .3ds/.glb";
      } else {
        reason << "symbolFile does not exist on disk (checked path='"
               << resolvedSymbolPathAfterDefinition.string() << "')";
      }
      std::ostringstream oss;
      oss << "Rider import truss fallback to dummy box: "
          << DescribeTrussForLog(t) << ". Reason: " << reason.str();
      Logger::Instance().Log(Logger::Level::Warn, oss.str());
    }
  }

  reportProgress("Placing imported scene objects...", 4, 6);
  const auto placementStartedAt = std::chrono::steady_clock::now();
  // Distribute fixtures along their hang positions using available truss
  // information. Fixtures are arranged symmetrically and alternately by type,
  // leaving a configurable margin at the ends of the truss and placing them on
  // the front-bottom side. When truss data is missing, a default width of 0.4 m
  // is assumed and fixtures are spaced 0.5 m apart around the origin.
  struct TrussInfo {
    float startX = 0.0f;
    float endX = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float width = 400.0f;
    bool found = false;
  };
  std::unordered_map<std::string, TrussInfo> trussInfo;
  struct SideTrussInfo {
    float leftX = -3500.0f;
    float rightX = 3500.0f;
    float startY = -2000.0f;
    float endY = 2000.0f;
    float z = 1000.0f;
    bool found = false;
  };
  SideTrussInfo sideTrussInfo;
  struct SideHoistSpanInfo {
    float x = 0.0f;
    float startY = 0.0f;
    float endY = 0.0f;
    float z = 0.0f;
    bool found = false;
  };
  SideHoistSpanInfo leftSideHoistSpan;
  SideHoistSpanInfo rightSideHoistSpan;
  for (const auto &[uuid, t] : scene.trusses) {
    (void)uuid;
    if (IsLxSidesHangName(t.positionName)) {
      const float sideX = t.transform.o[0];
      const float startY = t.transform.o[1];
      const float endY = startY + t.lengthMm;
      SideHoistSpanInfo &sideSpan =
          sideX <= 0.0f ? leftSideHoistSpan : rightSideHoistSpan;
      if (!sideSpan.found) {
        sideSpan.x = sideX;
        sideSpan.startY = startY;
        sideSpan.endY = endY;
        sideSpan.z = t.transform.o[2];
        sideSpan.found = true;
      } else {
        sideSpan.x = sideX <= 0.0f ? std::min(sideSpan.x, sideX)
                                   : std::max(sideSpan.x, sideX);
        const float currentStart = std::min(sideSpan.startY, sideSpan.endY);
        const float currentEnd = std::max(sideSpan.startY, sideSpan.endY);
        const float candidateStart = std::min(startY, endY);
        const float candidateEnd = std::max(startY, endY);
        sideSpan.startY = std::min(currentStart, candidateStart);
        sideSpan.endY = std::max(currentEnd, candidateEnd);
      }
      if (!sideTrussInfo.found) {
        sideTrussInfo.leftX = sideX;
        sideTrussInfo.rightX = sideX;
        sideTrussInfo.startY = std::min(startY, endY);
        sideTrussInfo.endY = std::max(startY, endY);
        sideTrussInfo.z = t.transform.o[2];
        sideTrussInfo.found = true;
      } else {
        sideTrussInfo.leftX = std::min(sideTrussInfo.leftX, sideX);
        sideTrussInfo.rightX = std::max(sideTrussInfo.rightX, sideX);
        sideTrussInfo.startY =
            std::min(sideTrussInfo.startY, std::min(startY, endY));
        sideTrussInfo.endY =
            std::max(sideTrussInfo.endY, std::max(startY, endY));
      }
      continue;
    }
    auto &info = trussInfo[t.positionName];
    float start = t.transform.o[0];
    float end = start + t.lengthMm;
    if (!info.found) {
      info.startX = start;
      info.endX = end;
      info.y = t.transform.o[1];
      info.z = t.transform.o[2];
      info.width = t.widthMm > 0.0f ? t.widthMm : info.width;
      info.found = true;
    } else {
      info.startX = std::min(info.startX, start);
      info.endX = std::max(info.endX, end);
    }
  }

  for (const PipeSpanInfo &pipeInfo : importedPipeSpans) {
    auto &info = trussInfo[pipeInfo.positionName];
    if (!info.found) {
      info.startX = pipeInfo.startX;
      info.endX = pipeInfo.endX;
      info.y = pipeInfo.y;
      info.z = pipeInfo.z;
      info.width = 100.0f;
      info.found = true;
    } else {
      info.startX = std::min(info.startX, pipeInfo.startX);
      info.endX = std::max(info.endX, pipeInfo.endX);
      info.y = pipeInfo.y;
      info.z = pipeInfo.z;
      info.width = 100.0f;
    }
  }

  if (!sideTrussInfo.found) {
    bool hasLxSpan = false;
    float lxStart = 0.0f;
    float lxEnd = 0.0f;
    for (const auto &[positionName, info] : trussInfo) {
      if (!info.found || !IsLxHangName(positionName))
        continue;
      if (!hasLxSpan) {
        lxStart = info.startX;
        lxEnd = info.endX;
        hasLxSpan = true;
      } else {
        lxStart = std::min(lxStart, info.startX);
        lxEnd = std::max(lxEnd, info.endX);
      }
    }
    if (hasLxSpan) {
      sideTrussInfo.leftX = lxStart - 500.0f;
      sideTrussInfo.rightX = lxEnd + 500.0f;
    }
  }

  if (!layerByType && !sideTrussInfo.found) {
    auto removeFromLayer = [&](const std::string &layerName,
                               const std::string &childUuid) {
      auto layerIt = layerLookup.find(layerName);
      if (layerIt == layerLookup.end() || !layerIt->second)
        return;
      std::vector<std::string> &children = layerIt->second->childUUIDs;
      children.erase(std::remove(children.begin(), children.end(), childUuid),
                     children.end());
    };

    for (const std::string &fixtureUuid : importedFixtureUuids) {
      auto fixtureIt = scene.fixtures.find(fixtureUuid);
      if (fixtureIt == scene.fixtures.end())
        continue;
      Fixture &fixture = fixtureIt->second;
      if (!IsLxSidesHangName(fixture.positionName))
        continue;

      const std::string oldLayer = fixture.layer;
      fixture.layer = "pos SIDES";
      if (oldLayer != fixture.layer) {
        removeFromLayer(oldLayer, fixture.uuid);
        addToLayer(fixture.layer, fixture.uuid);
      }
    }
  }

  constexpr float kScreenThicknessMm = 100.0f;
  for (const ScreenObjectRequest &request : screenObjectRequests) {
    TrussInfo info;
    auto infoIt = trussInfo.find(request.positionName);
    if (infoIt != trussInfo.end())
      info = infoIt->second;

    const float centerX = info.found ? (info.startX + info.endX) * 0.5f : 0.0f;
    const float centerY =
        info.found ? info.y : getHangPos(request.positionName);
    const float trussZ =
        info.found ? info.z : getHangHeight(request.positionName);
    const float centerZ = trussZ - 200.0f - request.heightMm * 0.5f;

    SceneObject screenObject;
    screenObject.uuid = GenerateUuid();
    screenObject.name = request.name;
    screenObject.layer = request.layer;
    screenObject.transform.o[0] = centerX;
    screenObject.transform.o[1] = centerY;
    screenObject.transform.o[2] = centerZ;
    Matrix screenGeometryScale;
    screenGeometryScale.u = {request.widthMm / 1000.0f, 0.0f, 0.0f};
    screenGeometryScale.v = {0.0f, kScreenThicknessMm / 1000.0f, 0.0f};
    screenGeometryScale.w = {0.0f, 0.0f, request.heightMm / 1000.0f};
    screenObject.geometries.push_back(
        {kPrimitiveCubeToken, screenGeometryScale});
    scene.sceneObjects[screenObject.uuid] = screenObject;
    addToLayer(screenObject.layer, screenObject.uuid);
  }

  std::vector<std::string> importedSupportUuids;
  auto placeHoist = [&](float x, float y, float z, float capacityKg,
                        const std::string &positionName,
                        const std::string &hoistFunction) {
    Support support;
    support.uuid = GenerateUuid();
    support.positionName = positionName;
    support.position = positionName;
    support.transform.o[0] = x;
    support.transform.o[1] = y;
    support.transform.o[2] = z;
    support.capacityKg = capacityKg;
    support.hoistFunction = NormalizeHoistFunction(hoistFunction);
    support.function = support.hoistFunction;
    support.hoistDataSource = "Manual";
    support.capacitySource = "Manual";
    support.hoistFunctionSource = "Manual";
    support.name = "HOIST";
    support.dummyProfileId = PickDummyHoistProfileId(capacityKg);
    support.motorName = support.name;
    support.motorNameSource = "Manual";
    support.motorManufacturerSource = "Manual";
    support.motorModelSource = "Manual";
    support.weightSource = "Manual";

    const std::string normalizedFunction =
        NormalizeHoistFunction(hoistFunction);
    std::string layerName = "rig " + normalizedFunction;
    support.layer = layerName;

    const std::string supportUuid = support.uuid;
    scene.supports[supportUuid] = support;
    importedSupportUuids.push_back(supportUuid);
    addToLayer(layerName, supportUuid,
               ResolveHoistLayerColor(normalizedFunction));
  };

  auto distributeAcrossTruss = [&](const std::string &positionName,
                                   int quantity, float capacityKg,
                                   float marginMm,
                                   const std::string &hoistFunction) {
    if (quantity <= 0)
      return;
    TrussInfo info;
    auto infoIt = trussInfo.find(positionName);
    if (infoIt != trussInfo.end())
      info = infoIt->second;
    float startX =
        info.found ? info.startX + marginMm : -500.0f * (quantity - 1);
    float endX = info.found ? info.endX - marginMm : 500.0f * (quantity - 1);
    if (endX < startX)
      std::swap(startX, endX);
    const float step = quantity > 1
                           ? (endX - startX) / static_cast<float>(quantity - 1)
                           : 0.0f;
    const float y = info.found ? info.y : getHangPos(positionName);
    const float z = info.found ? info.z : getHangHeight(positionName);
    for (int i = 0; i < quantity; ++i)
      placeHoist(startX + step * i, y, z, capacityKg, positionName,
                 hoistFunction);
  };

  auto distributeAcrossSides = [&](int quantity, float capacityKg,
                                   const std::string &hoistFunction) {
    if (quantity <= 0)
      return;

    const int leftCount = quantity / 2 + quantity % 2;
    const int rightCount = quantity / 2;
    const float sideMarginMm = 1000.0f;

    auto placeOnSide = [&](const SideHoistSpanInfo &sideInfo, int count,
                           float fallbackX) {
      if (count <= 0)
        return;
      const float sideX = sideInfo.found ? sideInfo.x : fallbackX;
      float startY = sideInfo.found ? sideInfo.startY : sideTrussInfo.startY;
      float endY = sideInfo.found ? sideInfo.endY : sideTrussInfo.endY;
      const float z = sideInfo.found
                          ? sideInfo.z
                                     : (sideTrussInfo.found ? sideTrussInfo.z
                                                            : getHangHeight("LX SIDES"));
      const float direction = endY >= startY ? 1.0f : -1.0f;
      startY += direction * sideMarginMm;
      endY -= direction * sideMarginMm;
      if ((direction > 0.0f && endY < startY) ||
          (direction < 0.0f && endY > startY)) {
        const float center = sideInfo.found
                                 ? (sideInfo.startY + sideInfo.endY) * 0.5f
                                            : (startY + endY) * 0.5f;
        startY = center;
        endY = center;
      }
      const float step =
          count > 1 ? (endY - startY) / static_cast<float>(count - 1) : 0.0f;
      for (int i = 0; i < count; ++i) {
        const float y = startY + step * static_cast<float>(i);
        placeHoist(sideX, y, z, capacityKg, "LX SIDES", hoistFunction);
      }
    };

    placeOnSide(leftSideHoistSpan, leftCount, sideTrussInfo.leftX);
    placeOnSide(rightSideHoistSpan, rightCount, sideTrussInfo.rightX);
  };

  auto distributePaOrSidefill = [&](const std::string &positionName,
                                    int quantity, float capacityKg,
                                    bool sidefill,
                                     const std::string &hoistFunction) {
    if (quantity <= 0)
      return;
    const TrussInfo lxInfo =
        trussInfo.count("LX1") ? trussInfo["LX1"] : TrussInfo{};
    const float baseY = lxInfo.found ? lxInfo.y : getHangPos("LX1");
    const float z = lxInfo.found ? lxInfo.z : getHangHeight("LX1");
    const float spanLeft = lxInfo.found ? lxInfo.startX : -3000.0f;
    const float spanRight = lxInfo.found ? lxInfo.endX : 3000.0f;
    const float offsetX = sidefill ? 1000.0f : 1000.0f;
    const float y = sidefill ? baseY + 2000.0f : baseY;

    const int leftCount = quantity / 2 + quantity % 2;
    const int rightCount = quantity / 2;
    auto placeGroup = [&](int count, float anchorX, float xDirection) {
      if (count <= 0)
        return;
      const int rows =
          count <= 2 ? count : static_cast<int>(std::ceil(std::sqrt(count)));
      const int cols =
          static_cast<int>(std::ceil(static_cast<float>(count) / rows));
      int placed = 0;
      for (int col = 0; col < cols && placed < count; ++col) {
        for (int row = 0; row < rows && placed < count; ++row) {
          float x = anchorX + xDirection * col * 1000.0f;
          float yPlaced =
              y + (rows == 1 ? 0.0f : (row - (rows - 1) * 0.5f) * 1000.0f);
          placeHoist(x, yPlaced, z, capacityKg, positionName, hoistFunction);
          ++placed;
        }
      }
    };

    placeGroup(leftCount, spanLeft - offsetX, -1.0f);
    placeGroup(rightCount, spanRight + offsetX, 1.0f);
  };

  for (const HoistRequest &request : hoistRequests) {
    const std::string hoistFunction =
        ResolveHoistFunctionForTarget(request.target);
    if (request.target == "LX") {
      std::vector<std::string> lxNames;
      for (const auto &[name, info] : trussInfo) {
        if (info.found && name.rfind("LX", 0) == 0)
          lxNames.push_back(name);
      }
      std::sort(lxNames.begin(), lxNames.end(),
                [](const std::string &a, const std::string &b) {
        return ParseTrailingNumber(a) < ParseTrailingNumber(b);
      });
      if (lxNames.empty())
        lxNames.push_back("LX1");
      int base = request.quantity / static_cast<int>(lxNames.size());
      int rem = request.quantity % static_cast<int>(lxNames.size());
      for (size_t i = 0; i < lxNames.size(); ++i) {
        int qty = base + (static_cast<int>(i) < rem ? 1 : 0);
        distributeAcrossTruss(lxNames[i], qty, request.capacityKg, 1000.0f,
                              hoistFunction);
      }
    } else if (request.target == "PA") {
      distributePaOrSidefill("P.A.", request.quantity, request.capacityKg,
                             false, hoistFunction);
    } else if (request.target == "SIDEFILL") {
      distributePaOrSidefill("SIDEFILL", request.quantity, request.capacityKg,
                             true, hoistFunction);
    } else if (request.target == "SCREEN") {
      if (request.quantity <= 0)
        continue;
      TrussInfo info;
      auto infoIt = trussInfo.find("SCREEN");
      if (infoIt != trussInfo.end())
        info = infoIt->second;
      const float y = info.found ? info.y : getHangPos("SCREEN");
      const float z = info.found ? info.z : getHangHeight("SCREEN");
      const float span =
          info.found ? (info.endX - info.startX) : (1000.0f * request.quantity);
      const float part = span / static_cast<float>(request.quantity + 1);
      const float startX = info.found ? info.startX : (-0.5f * span);
      for (int i = 0; i < request.quantity; ++i) {
        const float x = startX + part * static_cast<float>(i + 1);
        placeHoist(x, y, z, request.capacityKg, "SCREEN", hoistFunction);
      }
    } else {
      if (IsLxSidesHangName(request.target)) {
        distributeAcrossSides(request.quantity, request.capacityKg,
                              hoistFunction);
        continue;
      }
      const float marginMm = IsLxHangName(request.target) ? 1000.0f : 2000.0f;
      distributeAcrossTruss(request.target, request.quantity,
                            request.capacityKg, marginMm, hoistFunction);
    }
  }

  reportProgress("Assigning fixture identities...", 5, 6);
  std::vector<Support *> importedSupports;
  importedSupports.reserve(importedSupportUuids.size());
  for (const std::string &uuid : importedSupportUuids) {
    auto it = scene.supports.find(uuid);
    if (it == scene.supports.end())
      continue;
    importedSupports.push_back(&it->second);
  }
  AssignImportedHoistNames(importedSupports);
  const auto roundedRiggingTotalsByPosition =
      HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(scene);
  HoistWeightDistribution::ApplyForImportedSupports(
      scene, importedSupportUuids, roundedRiggingTotalsByPosition);

  // Fill any missing fixture categories before placement so downstream
  // distribution can rely on them.
  for (const std::string &uuid : importedFixtureUuids) {
    auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;
    EnsureFixtureCategoryForImport(scene, fixtureIt->second);
  }

  std::unordered_map<std::string, std::vector<Fixture *>> fixturesByPos;
  fixturesByPos.reserve(importedFixtureUuids.size());
  for (const std::string &uuid : importedFixtureUuids) {
    auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;
    fixturesByPos[fixtureIt->second.positionName].push_back(&fixtureIt->second);
  }

  auto buildSymmetricOrder = [](const std::vector<Fixture *> &fixturesVec) {
    std::vector<Fixture *> ordered;
    if (fixturesVec.empty())
      return ordered;

    std::unordered_map<std::string, int> counts;
    std::vector<std::string> types;
    for (Fixture *f : fixturesVec) {
      if (!counts.count(f->typeName))
        types.push_back(f->typeName);
      counts[f->typeName]++;
    }

    int total = static_cast<int>(fixturesVec.size());
    std::vector<std::string> center;
    for (const std::string &t : types) {
      if (counts[t] % 2 == 1) {
        center.push_back(t);
        counts[t]--;
      }
    }

    int pairsPerSide = (total - static_cast<int>(center.size())) / 2;
    std::vector<std::string> left;
    size_t idx = 0;
    while (static_cast<int>(left.size()) < pairsPerSide) {
      const std::string &t = types[idx % types.size()];
      if (counts[t] > 0) {
        left.push_back(t);
        counts[t] -= 2;
      }
      ++idx;
    }

    std::vector<std::string> order = left;
    order.insert(order.end(), center.begin(), center.end());
    std::vector<std::string> right = left;
    std::reverse(right.begin(), right.end());
    order.insert(order.end(), right.begin(), right.end());

    std::unordered_map<std::string, std::vector<Fixture *>> byType;
    for (Fixture *f : fixturesVec)
      byType[f->typeName].push_back(f);
    for (auto &[type, vec] : byType)
      std::reverse(vec.begin(), vec.end());

    ordered.reserve(total);
    for (const std::string &t : order) {
      auto &vec = byType[t];
      if (vec.empty())
        continue;
      ordered.push_back(vec.back());
      vec.pop_back();
    }
    return ordered;
  };

  auto buildLinearOrder = [&](const std::vector<Fixture *> &fixturesVec) {
    std::vector<Fixture *> ordered = fixturesVec;
    std::sort(ordered.begin(), ordered.end(), [&](Fixture *a, Fixture *b) {
      const size_t orderA = importedFixtureOrderByUuid.count(a->uuid) > 0
                                ? importedFixtureOrderByUuid.at(a->uuid)
                                : std::numeric_limits<size_t>::max();
      const size_t orderB = importedFixtureOrderByUuid.count(b->uuid) > 0
                                ? importedFixtureOrderByUuid.at(b->uuid)
                                : std::numeric_limits<size_t>::max();
      if (orderA != orderB)
        return orderA < orderB;
      return a->uuid < b->uuid;
    });
    return ordered;
  };

  auto placeFixtureGroup =
      [&](const std::string &pos, const std::vector<Fixture *> &fixturesVec,
          const std::function<void(Fixture &, float, float, float, float)>
              &applyPlacement) {
    if (fixturesVec.empty())
      return;

    const bool useLinearPlacement =
        linearPlacementHangs.count(pos) > 0 && !fixturesVec.empty();
    const std::vector<Fixture *> ordered =
        useLinearPlacement ? buildLinearOrder(fixturesVec)
                           : buildSymmetricOrder(fixturesVec);
    const int total = static_cast<int>(ordered.size());
    if (total <= 0)
      return;

    TrussInfo info;
    auto it = trussInfo.find(pos);
    if (it != trussInfo.end())
      info = it->second;
    float margin = getHangMargin(pos);
    float startX =
        info.found ? info.startX + margin : -0.5f * ((total - 1) * 500.0f);
    float endX =
        info.found ? info.endX - margin : 0.5f * ((total - 1) * 500.0f);
    float baseY = info.found ? info.y : getHangPos(pos);
    float baseZ = info.found ? info.z : getHangHeight(pos);
    float width = info.found ? info.width : 400.0f;
    if (total == 1) {
      Fixture *fixture = ordered.front();
      if (fixture) {
            const float centeredX = info.found ? 0.5f * (startX + endX) : 0.0f;
        applyPlacement(*fixture, centeredX, baseY, baseZ, width);
      }
      return;
    }

    float step = (endX - startX) / static_cast<float>(total - 1);
    for (int i = 0; i < total; ++i) {
      Fixture *f = ordered[static_cast<size_t>(i)];
      if (!f)
        continue;
      applyPlacement(*f, startX + i * step, baseY, baseZ, width);
    }
  };

  auto normalizeCategory = [](std::string category) {
    std::transform(
        category.begin(), category.end(), category.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return category;
  };

  const std::string blinderCategory =
      normalizeCategory(GdtfFixtureCategory::kBlinder);
  const std::string strobeCategory =
      normalizeCategory(GdtfFixtureCategory::kStrobe);
  const std::string washCategory =
      normalizeCategory(GdtfFixtureCategory::kWash);
  const std::string smokeCategory =
      normalizeCategory(GdtfFixtureCategory::kSmoke);

  auto isTopFrontCategory = [&](const Fixture &fixture) {
    const std::string normalized = normalizeCategory(fixture.category);
    return normalized == blinderCategory || normalized == strobeCategory;
  };

  auto isBackBottomCategory = [&](const Fixture &fixture) {
    return normalizeCategory(fixture.category) == washCategory;
  };
  auto isSmokeCategory = [&](const Fixture &fixture) {
    return normalizeCategory(fixture.category) == smokeCategory;
  };

  auto placeFixturesAsSides = [&](const std::vector<Fixture *> &ordered,
                                  float leftX, float rightX, float startY,
                                  float endY, bool hasSideSpan, float z) {
    if (ordered.empty())
      return;
    const int leftCount = static_cast<int>(ordered.size()) / 2 +
                          static_cast<int>(ordered.size()) % 2;
    const int rightCount = static_cast<int>(ordered.size()) / 2;
    auto placeSideGroup =
        [&](int count, float sideX,
                              const std::function<Fixture *(int)> &pickFixture) {
      if (count <= 0)
        return;
      const float effectiveEndY =
              hasSideSpan ? endY
                          : startY + static_cast<float>(count - 1) * 500.0f;
          const float step = count > 1 ? (effectiveEndY - startY) /
                                             static_cast<float>(count - 1)
                                   : 0.0f;
      for (int i = 0; i < count; ++i) {
        Fixture *fixture = pickFixture(i);
        if (!fixture)
          continue;
        fixture->transform.o[0] = sideX;
        fixture->transform.o[1] = startY + step * static_cast<float>(i);
        fixture->transform.o[2] = z;
      }
    };
    placeSideGroup(leftCount, leftX,
                   [&](int i) { return ordered[static_cast<size_t>(i)]; });
    placeSideGroup(rightCount, rightX, [&](int i) {
      return ordered[ordered.size() - 1U - static_cast<size_t>(i)];
    });
  };

  float smokeLeftX = sideTrussInfo.leftX;
  float smokeRightX = sideTrussInfo.rightX;
  if (!sideTrussInfo.found) {
    bool hasLxSpan = false;
    float lxStart = 0.0f;
    float lxEnd = 0.0f;
    for (const auto &[positionName, info] : trussInfo) {
      if (!info.found || !IsLxHangName(positionName))
        continue;
      if (!hasLxSpan) {
        lxStart = info.startX;
        lxEnd = info.endX;
        hasLxSpan = true;
      } else {
        lxStart = std::min(lxStart, info.startX);
        lxEnd = std::max(lxEnd, info.endX);
      }
    }
    if (hasLxSpan) {
      smokeLeftX = lxStart + 500.0f;
      smokeRightX = lxEnd - 500.0f;
      if (smokeLeftX > smokeRightX) {
        const float center = (lxStart + lxEnd) * 0.5f;
        smokeLeftX = center;
        smokeRightX = center;
      }
    }
  }
  const float smokeStartY = sideTrussInfo.found
                                ? sideTrussInfo.startY + getHangMargin("LX1")
                                : 1000.0f;
  const float smokeEndY = sideTrussInfo.found
                              ? sideTrussInfo.endY - getHangMargin("LX1")
                              : smokeStartY;

  for (auto &[pos, fixturesVec] : fixturesByPos) {
    if (fixturesVec.empty())
      continue;
    if (IsLxSidesHangName(pos)) {
      const std::vector<Fixture *> ordered = buildSymmetricOrder(fixturesVec);
      const float startY = sideTrussInfo.found
                               ? sideTrussInfo.startY + getHangMargin("LX1")
                               : 1000.0f;
      const float endY = sideTrussInfo.found
                             ? sideTrussInfo.endY - getHangMargin("LX1")
                                             : startY;
      placeFixturesAsSides(ordered, sideTrussInfo.leftX, sideTrussInfo.rightX,
                           startY, endY, sideTrussInfo.found,
                           sideTrussInfo.found ? sideTrussInfo.z : 1000.0f);
      continue;
    }
    if (pipeHangNames.count(pos) > 0) {
      placeFixtureGroup(pos, fixturesVec,
                        [&](Fixture &fixture, float x, float baseY, float baseZ,
                            float /*width*/) {
                          fixture.transform.o[0] = x;
                          fixture.transform.o[1] = baseY;
                          fixture.transform.o[2] = baseZ;
                        });
      continue;
    }

    std::vector<Fixture *> bottomFixtures;
    std::vector<Fixture *> topFrontFixtures;
    std::vector<Fixture *> smokeFixtures;
    bottomFixtures.reserve(fixturesVec.size());
    topFrontFixtures.reserve(fixturesVec.size());
    smokeFixtures.reserve(fixturesVec.size());
    for (Fixture *f : fixturesVec) {
      if (!f)
        continue;
      if (isSmokeCategory(*f)) {
        smokeFixtures.push_back(f);
        continue;
      }
      if (isTopFrontCategory(*f))
        topFrontFixtures.push_back(f);
      else
        bottomFixtures.push_back(f);
    }

    const std::vector<Fixture *> orderedSmoke =
        buildSymmetricOrder(smokeFixtures);
    placeFixturesAsSides(orderedSmoke, smokeLeftX, smokeRightX, smokeStartY,
                         smokeEndY, sideTrussInfo.found, 0.0f);

    placeFixtureGroup(
        pos, bottomFixtures,
        [&](Fixture &fixture, float x, float baseY, float baseZ, float width) {
                        fixture.transform.o[0] = x;
          fixture.transform.o[1] = isBackBottomCategory(fixture)
                                       ? baseY + width * 0.5f
                                                           : baseY - width * 0.5f;
                        fixture.transform.o[2] = baseZ;
                      });

    placeFixtureGroup(
        pos, topFrontFixtures,
        [&](Fixture &fixture, float x, float baseY, float baseZ, float width) {
                        fixture.transform.o[0] = x;
                        fixture.transform.o[1] = baseY - width * 0.5f;
                        fixture.transform.o[2] = baseZ + width * 0.5f;
                      });
  }

  // Assign fixture IDs and instance names to imported fixtures only.
  // Existing fixtures keep their IDs and transforms untouched.
  std::unordered_map<std::string, std::vector<Fixture *>> fixturesByType;
  fixturesByType.reserve(typeOrder.size());
  for (const std::string &uuid : importedFixtureUuids) {
    auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;
    fixturesByType[fixtureIt->second.typeName].push_back(&fixtureIt->second);
  }

  AddRiderImportPhaseTime(timing, "place imported objects", placementStartedAt);
  const auto identityStartedAt = std::chrono::steady_clock::now();
  std::unordered_set<std::string> importedFixtureIdSet(
      importedFixtureUuids.begin(), importedFixtureUuids.end());
  int highestExistingFixtureId = 100;
  std::unordered_map<std::string, int> nextUnitByType;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (importedFixtureIdSet.count(uuid) != 0)
      continue;
    highestExistingFixtureId =
        std::max(highestExistingFixtureId, fixture.fixtureId);

    int nextUnit = fixture.unitNumber;
    if (nextUnit <= 0)
      nextUnit = ParseTrailingNumber(fixture.instanceName);
    if (nextUnit > 0) {
      int &tracked = nextUnitByType[fixture.typeName];
      tracked = std::max(tracked, nextUnit);
    }
  }

  auto baseName = [](const std::string &name) {
    size_t space = name.find_last_of(' ');
    if (space == std::string::npos)
      return name;
    bool numeric = true;
    for (size_t i = space + 1; i < name.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
        numeric = false;
        break;
      }
    }
    return numeric ? name.substr(0, space) : name;
  };

  int baseId = ((highestExistingFixtureId + 99) / 100) * 100 + 1;
  for (const std::string &type : typeOrder) {
    auto it = fixturesByType.find(type);
    if (it == fixturesByType.end())
      continue;
    auto &vec = it->second;
    std::sort(vec.begin(), vec.end(), [](Fixture *a, Fixture *b) {
      if (std::abs(a->transform.o[1] - b->transform.o[1]) < 1e-3f)
        return a->transform.o[0] < b->transform.o[0];
      return a->transform.o[1] < b->transform.o[1];
    });
    std::string prefix =
        vec.empty() ? type : baseName(vec.front()->instanceName);
    int nextUnitNumber = nextUnitByType[type] + 1;
    for (size_t i = 0; i < vec.size(); ++i) {
      vec[i]->fixtureId = baseId + static_cast<int>(i);
      vec[i]->unitNumber = nextUnitNumber + static_cast<int>(i);
      vec[i]->instanceName =
          prefix + " " + std::to_string(nextUnitNumber + static_cast<int>(i));
    }
    baseId = ((baseId - 1 + static_cast<int>(vec.size()) + 99) / 100) * 100 + 1;
  }

  AddRiderImportPhaseTime(timing, "assign fixture identities",
                          identityStartedAt);

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

  auto autoPref = cfg.GetValue("rider_autopatch");
  if (!autoPref || *autoPref != "0") {
    phaseStartedAt = std::chrono::steady_clock::now();
    AutoPatcher::AutoPatch(scene);
    AddRiderImportPhaseTime(timing, "Auto Patch", phaseStartedAt);
  }
  reportProgress("Completed.", 6, 6);
  LogRiderImportTiming(timing);
  return true;
}
