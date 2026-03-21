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

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pdftext.h"

#include "autopatcher.h"
#include "configmanager.h"
#include "fixture.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "layer.h"
#include "logger.h"
#include "support.h"
#include "truss.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "uuidutils.h"
#include <filesystem>

namespace {
// Precompiled regexes used by RiderImporter. Keeping them static avoids paying
// the compilation cost on every import call and makes keyword matching cheap
// even when processing large riders.
static const std::regex kTrussLineRe(
    "^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:truss)\\s+([^\\n]*?)\\s+(\\d+(?:\\.\\d+)?)\\s*(?:m|metros?|meters?)\\b(?:\\s+para\\s+(.+))?",
    std::regex::icase);
static const std::regex kTrussRe(
    "(?:truss)[^\\n]*?(\\d+(?:\\.\\d+)?)\\s*(?:m|metros?|meters?)\\b",
    std::regex::icase);
static const std::regex kHoistLineRe(
    "^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:motor(?:es)?|hoist(?:s)?)\\b(.*)$",
    std::regex::icase);
static const std::regex kHoistCapacityRe(
    "(\\d+(?:[\\.,]\\d+)?)\\s*(kg|kgs?|kilogramos?|kilos?|t|to|tn|ton|tons?|toneladas?)\\b",
    std::regex::icase);
static const std::regex kHoistTargetRe("\\bpara\\s+(.+)$", std::regex::icase);
static const std::regex kFixtureLineRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s+(.+)$",
                                       std::regex::icase);
static const std::regex kQuantityOnlyRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s*$");
static const std::regex kHangLineRe(
    "^\\s*(LX\\d+|screen|pantalla|led\\s*screen|floor|efectos?|calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?)\\s*:?\\s*$",
    std::regex::icase);
static const std::regex kHangFindRe(
    "(LX\\d+|screen|pantalla|led\\s*screen|floor|efectos?|calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?)",
                                    std::regex::icase);
static const std::regex kHangOnlyRe(
    "^\\s*(LX\\d+|screen|pantalla|led\\s*screen|floor|efectos?|calle(?:s)?\\s+a\\s+suelo|ground\\s+lanes?)\\s*$",
                                    std::regex::icase);
std::string Trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
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

void ApplyFixturePhysicalPropertiesFromGdtf(const MvrScene &scene,
                                            Fixture &fixture) {
  if (fixture.gdtfSpec.empty())
    return;

  const std::string gdtfPath = ResolveGdtfPath(scene, fixture.gdtfSpec);
  if (gdtfPath.empty())
    return;

  float gdtfWeightKg = 0.0f;
  float gdtfPowerW = 0.0f;
  if (!GetGdtfProperties(gdtfPath, gdtfWeightKg, gdtfPowerW))
    return;

  if (fixture.weightKg <= 0.0f)
    fixture.weightKg = gdtfWeightKg;
  if (fixture.powerConsumptionW <= 0.0f)
    fixture.powerConsumptionW = gdtfPowerW;
}

bool TryParseFloat(const std::string &text, float &out) {
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

bool TryParseInt(std::string_view text, int &out) {
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
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return ext == ".3ds" || ext == ".glb";
}

std::filesystem::path ResolveTrussSymbolPath(const Truss &truss) {
  std::filesystem::path symbolPath = std::filesystem::u8path(truss.symbolFile);
  if (symbolPath.is_absolute())
    return symbolPath;

  if (!truss.modelFile.empty()) {
    std::filesystem::path modelPath = std::filesystem::u8path(truss.modelFile);
    if (modelPath.is_absolute())
      return modelPath.parent_path() / symbolPath;
  }

  if (!truss.gdtfSpec.empty()) {
    std::filesystem::path gdtfPath = std::filesystem::u8path(truss.gdtfSpec);
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

std::vector<std::string> SplitPlus(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream ss(s);
  std::string item;
  while (std::getline(ss, item, '+')) {
    item = Trim(item);
    if (!item.empty())
      out.push_back(item);
  }
  return out;
}

bool IsFloorAlias(std::string_view value);

std::string NormalizeHangName(const std::string &raw) {
  std::string hang = Trim(raw);
  if (hang.empty())
    return {};
  if (IsFloorAlias(hang))
    return "FLOOR";
  std::transform(hang.begin(), hang.end(), hang.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  if (hang.rfind("PUENTES ", 0) == 0)
    hang = Trim(hang.substr(8));
  else if (hang.rfind("PUENTE ", 0) == 0)
    hang = Trim(hang.substr(7));
  if (hang == "PANTALLA")
    return "SCREEN";
  if (hang == "SCREEN" || hang == "LEDSCREEN")
    return "SCREEN";
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

std::string BuildHoistName(float capacityKg, const std::string &positionName) {
  int rounded = static_cast<int>(std::round(capacityKg));
  std::ostringstream oss;
  oss << "Motor " << rounded << " Kg";
  if (!positionName.empty())
    oss << " " << positionName;
  return oss.str();
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
  if (normalized == "PA" || normalized == "P.A." ||
      normalized == "SIDEFILL" || normalized == "OUTFILL")
    return "Audio";
  if (normalized == "SCREEN" || normalized == "LEDSCREEN" ||
      normalized == "VIDEO")
    return "Video";
  return "Lighting";
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

  pushNormalized(modelToken);
  if (!modelToken.empty())
    pushNormalized("TRUSS " + modelToken);
  pushNormalized(trussName);

  return keys;
}

// ExtractPdfText moved to pdftext.cpp
} // namespace

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

bool RiderImporter::Import(const std::string &path) {
  std::string text = LoadText(path);
  if (text.empty())
    return false;
  return ImportText(text);
}

std::string RiderImporter::BuildFixtureFilterPreview(const std::string &text) {
  if (text.empty())
    return {};

  std::istringstream iss(text);
  std::string line;
  bool inFixtures = false;
  bool inRigging = false;
  bool inControl = false;
  bool havePending = false;
  int pendingQuantity = 0;
  std::string currentHang;
  std::vector<std::string> hangOrder;
  std::unordered_map<std::string, std::vector<std::string>> fixturesByHang;
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
    normalized = std::regex_replace(normalized, std::regex("\\([^\\)]*\\)"), "");
    normalized = std::regex_replace(normalized, std::regex("\\s*[-]\\s*"), "-");
    normalized = std::regex_replace(normalized, std::regex("\\s+"), " ");
    return Trim(normalized);
  };

  auto appendFixtureLines = [&](int baseQuantity, const std::string &descRaw) {
    const std::string hang = currentHang.empty() ? "UNASSIGNED" : currentHang;
    auto parts = SplitPlus(descRaw);
    for (const auto &partRaw : parts) {
      std::smatch pm;
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, kFixtureLineRe)) {
        if (!TryParseInt(pm[1].str(), quantity))
          quantity = baseQuantity;
        part = Trim(pm[2]);
      }
      part = normalizeFixtureToken(part);
      if (part.empty())
        continue;

      auto &bucket = fixturesByHang[hang];
      if (bucket.empty())
        hangOrder.push_back(hang);
      bucket.push_back(std::to_string(quantity) + " " + part);
    }
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
    if (ContainsCaseInsensitive(line, "sonido") ||
        ContainsCaseInsensitive(line, "audio") ||
        ContainsCaseInsensitive(line, "control de p.a.") ||
        ContainsCaseInsensitive(line, "monitores") ||
        ContainsCaseInsensitive(line, "microfon") ||
        ContainsCaseInsensitive(line, "video") ||
        ContainsCaseInsensitive(line, "realizacion") ||
        ContainsCaseInsensitive(line, "control")) {
      inFixtures = false;
      inRigging = false;
      inControl = ContainsCaseInsensitive(line, "control");
      havePending = false;
      continue;
    }
    if (ContainsCaseInsensitive(line, "rigging")) {
      inFixtures = false;
      inRigging = true;
      inControl = false;
      havePending = false;
      continue;
    }
    if (!inControl && (ContainsCaseInsensitive(line, "ilumin") ||
                       ContainsCaseInsensitive(line, "robotica") ||
                       ContainsCaseInsensitive(line, "convencion"))) {
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }

    std::smatch m;
    std::smatch hm;
    if (std::regex_match(line, hm, kHangLineRe)) {
      havePending = false;
      std::string captured = hm[1];
      if (IsFloorAlias(captured)) {
        currentHang = "FLOOR";
      } else {
        currentHang = NormalizeHangName(captured);
      }
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
      int quantity = 0;
      if (!TryParseInt(m[1].str(), quantity) || quantity <= 0)
        continue;
      std::string model = Trim(m[2]);
      float lengthM = 0.0f;
      if (!TryParseFloat(m[3].str(), lengthM))
        continue;
      std::string hang = currentHang;
      if (m.size() > 4 && m[4].matched) {
        hang = m[4].str();
      } else if (std::regex_match(model, kHangOnlyRe)) {
        hang = model;
        model.clear();
      }
      hang = NormalizeHangName(hang);

      // Do not keep floor trusses in filtered output because those are not
      // useful for the target lighting rigging workflow.
      if (hang == "FLOOR")
        continue;

      const std::string lenText = formatLengthM(lengthM) + "m";
      auto buildLine = [&](const std::string &targetHang) {
        std::string out = "1 TRUSS";
        if (!model.empty())
          out += " " + model;
        out += " " + lenText;
        if (!targetHang.empty())
          out += " " + targetHang;
        riggingLines.push_back(out);
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

    if (std::regex_search(line, m, kTrussRe)) {
      float lengthM = 0.0f;
      if (!TryParseFloat(m[1].str(), lengthM))
        continue;
      std::string hang = NormalizeHangName(currentHang);
      if (std::regex_search(line, hm, kHangFindRe))
        hang = NormalizeHangName(hm[1].str());
      if (hang == "FLOOR")
        continue;
      std::string out = "1 TRUSS " + formatLengthM(lengthM) + "m";
      if (!hang.empty())
        out += " " + hang;
      riggingLines.push_back(out);
      if (hang.rfind("LX", 0) == 0 &&
          std::find(lxTargetsInRigging.begin(), lxTargetsInRigging.end(), hang) ==
              lxTargetsInRigging.end()) {
        lxTargetsInRigging.push_back(hang);
      }
      continue;
    }

    ParsedHoistLine parsedHoist;
    if (TryParseHoistLine(line, currentHang, parsedHoist)) {
      hoistRequests.push_back(
          {parsedHoist.quantity, parsedHoist.capacityKg, parsedHoist.target});
      continue;
    }

    if (inFixtures && std::regex_match(line, m, kFixtureLineRe)) {
      int baseQuantity = 0;
      if (!TryParseInt(m[1].str(), baseQuantity))
        continue;
      appendFixtureLines(baseQuantity, Trim(m[2]));
      continue;
    }

    if (inFixtures && std::regex_match(line, m, kQuantityOnlyRe)) {
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
      hoistLines.back() =
          std::to_string(quantity) + " " + hoistLines.back() + "Kg PARA " + target;
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

    if (!preview.str().empty())
      preview << "\n\n";
    preview << "RIGGING";
    for (const std::string &hoistLine : hoistLines)
      preview << "\n" << hoistLine;
    for (const std::string &rigLine : riggingLines)
      preview << "\n" << rigLine;
  }

  return preview.str();
}

bool RiderImporter::ImportText(const std::string &text) {
  if (text.empty())
    return false;

  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("import rider");
  auto &scene = cfg.GetScene();
  std::string defaultLayer = cfg.GetCurrentLayer();
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool layerByType = modeVal && *modeVal == "type";
  std::optional<float> lastLightingTrussPosY;
  std::optional<float> lastLightingTrussPosZ;

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
    return 0.0f;
  };

  auto getHangMargin = [&](const std::string &posName) {
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

  auto addToLayer = [&](const std::string &lname, const std::string &uid) {
    std::string name = lname.empty() ? DEFAULT_LAYER_NAME : lname;
    Layer *layerPtr = nullptr;
    auto it = layerLookup.find(name);
    if (it != layerLookup.end()) {
      layerPtr = it->second;
    } else {
      Layer l;
      l.uuid = name == DEFAULT_LAYER_NAME ? "layer_default" : GenerateUuid();
      l.name = name;
      auto [insertedIt, inserted] =
          scene.layers.emplace(l.uuid, std::move(l));
      layerPtr = &insertedIt->second;
      layerLookup.emplace(layerPtr->name, layerPtr);
    }
    layerPtr->childUUIDs.push_back(uid);
  };

  std::istringstream iss(text);
  std::string line;
  bool inFixtures = false;
  bool inRigging = false;
  bool inControl = false;
  std::string currentHang;
  std::unordered_map<std::string, int> nameCounters;
  std::vector<std::string> typeOrder;
  typeOrder.reserve(16);
  std::unordered_set<std::string> seenTypes;
  std::vector<std::string> importedTrussUuids;
  std::vector<std::string> importedFixtureUuids;
  struct HoistRequest {
    int quantity = 0;
    float capacityKg = 0.0f;
    std::string target;
  };
  std::vector<HoistRequest> hoistRequests;
  int pendingQuantity = 0;
  bool havePending = false;

  auto addFixtures = [&](int baseQuantity, const std::string &desc) {
    auto parts = SplitPlus(desc);
    for (const auto &partRaw : parts) {
      std::smatch pm;
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, kFixtureLineRe)) {
        if (!TryParseInt(pm[1].str(), quantity))
          quantity = baseQuantity;
        part = Trim(pm[2]);
      }
      int &counter = nameCounters[part];
      for (int i = 0; i < quantity; ++i) {
        Fixture f;
        f.uuid = GenerateUuid();
        f.instanceName = part + " " + std::to_string(++counter);
        f.typeName = part;
        if (auto dictEntry = GdtfDictionary::Get(f.typeName)) {
          f.gdtfSpec = dictEntry->path;
          f.gdtfMode = dictEntry->mode;
          const std::string resolvedGdtfPath = ResolveGdtfPath(scene, f.gdtfSpec);
          std::string parsed = Trim(GetGdtfFixtureName(resolvedGdtfPath));
          if (!parsed.empty())
            f.typeName = parsed;
          ApplyFixturePhysicalPropertiesFromGdtf(scene, f);
        }
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
        scene.fixtures[f.uuid] = f;
        importedFixtureUuids.push_back(f.uuid);
        addToLayer(f.layer, f.uuid);
      }
    }
  };
  while (std::getline(iss, line)) {
    // Remove Windows carriage returns to allow regexes anchored with '$' to
    // match lines extracted from external tools.
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    if (ContainsCaseInsensitive(line, "sonido") ||
        ContainsCaseInsensitive(line, "audio") ||
        ContainsCaseInsensitive(line, "control de p.a.") ||
        ContainsCaseInsensitive(line, "monitores") ||
        ContainsCaseInsensitive(line, "microfon") ||
        ContainsCaseInsensitive(line, "video") ||
        ContainsCaseInsensitive(line, "realizacion") ||
        ContainsCaseInsensitive(line, "control")) {
      inFixtures = false;
      inRigging = false;
      inControl = ContainsCaseInsensitive(line, "control");
      havePending = false;
      continue;
    }
    if (ContainsCaseInsensitive(line, "rigging")) {
      inFixtures = false;
      inRigging = true;
      inControl = false;
      havePending = false;
      continue;
    }
    if (!inControl && (ContainsCaseInsensitive(line, "ilumin") ||
                       ContainsCaseInsensitive(line, "robotica") ||
                       ContainsCaseInsensitive(line, "convencion"))) {
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }

    std::smatch m;
    std::smatch hm;
    if (std::regex_match(line, hm, kHangLineRe)) {
      havePending = false;
      std::string captured = hm[1];
      if (IsFloorAlias(captured)) {
        currentHang = "FLOOR";
      } else {
        currentHang = captured;
        std::transform(
            currentHang.begin(), currentHang.end(), currentHang.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
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
                std::regex("(\\d+(?:\\.\\d+)?)\\s*[xX]\\s*(\\d+(?:\\.\\d+)?)"))) {
          float parsed = 0.0f;
          if (TryParseFloat(dm[1], parsed))
            width = parsed * 10.0f;
          parsed = 0.0f;
          if (TryParseFloat(dm[2], parsed))
            height = parsed * 10.0f;
        }
        std::string hang = currentHang;
        if (m.size() > 4 && m[4].matched) {
          hang = Trim(m[4]);
        } else if (std::regex_match(model, kHangOnlyRe)) {
          hang = model;
          model.clear();
        }
        hang = NormalizeHangName(hang);
        if (hang == "FLOOR")
          continue;

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

        auto addTrussPieces = [&](const std::string &posName) {
          auto pieces = SplitTrussSymmetric(length);
          float total = std::accumulate(pieces.begin(), pieces.end(), 0.0f);
          float x = -0.5f * total;
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
            t.transform.o[0] = x;
            t.transform.o[1] = getHangPos(posName);
            // Position dummy truss so its base sits at the hang height.
            // Real truss models are inserted from their bottom, so using the
            // raw hang height keeps the base aligned when swapping models.
            t.transform.o[2] = getHangHeight(posName);
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
              dictPath = TrussDictionary::Get(lookupKey);
              if (dictPath)
                break;
            }

            if (dictPath) {
              Truss parsed;
              if (LoadTrussDefinition(*dictPath, parsed)) {
                if (!parsed.symbolFile.empty())
                  t.symbolFile = parsed.symbolFile;
                t.modelFile = parsed.modelFile.empty() ? *dictPath : parsed.modelFile;
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
            scene.trusses.emplace(trussUuid, std::move(t));
            importedTrussUuids.push_back(trussUuid);
            addToLayer(trussLayer, trussUuid);
            if (posName.rfind("LX", 0) == 0) {
              lastLightingTrussPosY = getHangPos(posName);
              lastLightingTrussPosZ = getHangHeight(posName);
            }
            x += s;
          }
        };

        if (hang == "LX") {
          for (int i = 0; i < quantity; ++i)
            addTrussPieces("LX" + std::to_string(i + 1));
        } else {
          for (int i = 0; i < quantity; ++i)
            addTrussPieces(hang);
        }
      } else if (std::regex_search(line, m, kTrussRe)) {
        float length = 0.0f;
        if (!TryParseFloat(m[1], length))
          continue;
        length *= 1000.0f;
        std::string hang = currentHang;
        if (std::regex_search(line, hm, kHangFindRe)) {
          hang = hm[1];
          hang = NormalizeHangName(hang);
        }
        if (hang == "FLOOR")
          continue;

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
        float x = -0.5f * total;
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
          t.transform.o[1] = getHangPos(hang);
          // Store the hang height directly so the base matches real models
          // that are inserted from the bottom.
          t.transform.o[2] = getHangHeight(hang);
          std::string sizeStr = formatLength(s);
          t.name = "TRUSS " + sizeStr;
          t.model = TrussDictionary::NormalizeModelKey(t.name);
          const std::vector<std::string> dictionaryLookupKeys =
              BuildTrussDictionaryLookupKeys(t.model, t.name);

          std::optional<std::string> dictPath;
          for (const std::string &lookupKey : dictionaryLookupKeys) {
            dictPath = TrussDictionary::Get(lookupKey);
            if (dictPath)
              break;
          }

          if (dictPath) {
            Truss parsed;
            if (LoadTrussDefinition(*dictPath, parsed)) {
              if (!parsed.symbolFile.empty())
                t.symbolFile = parsed.symbolFile;
              t.modelFile = parsed.modelFile.empty() ? *dictPath : parsed.modelFile;
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
          scene.trusses.emplace(trussUuid, std::move(t));
          importedTrussUuids.push_back(trussUuid);
          addToLayer(trussLayer, trussUuid);
          if (hang.rfind("LX", 0) == 0) {
            lastLightingTrussPosY = getHangPos(hang);
            lastLightingTrussPosZ = getHangHeight(hang);
          }
          x += s;
        }
      } else if (inFixtures && std::regex_match(line, m, kFixtureLineRe)) {
        int baseQuantity = 0;
        if (!TryParseInt(m[1].str(), baseQuantity))
          continue;
        std::string desc = Trim(m[2]);
        addFixtures(baseQuantity, desc);
      } else if (inFixtures && std::regex_match(line, m, kQuantityOnlyRe)) {
        if (!TryParseInt(m[1].str(), pendingQuantity))
          continue;
        havePending = true;
      }
    }
  }

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
      resolved = LoadTrussDefinition(t.modelFile, parsed);
      if (!resolved)
        modelDefinitionFailed = true;
    }
    if (!resolved && !t.gdtfSpec.empty()) {
      resolved = LoadTrussDefinition(t.gdtfSpec, parsed);
      if (!resolved)
        gdtfDefinitionFailed = true;
    }
    if (!resolved) {
      const std::vector<std::string> dictionaryLookupKeys =
          BuildTrussDictionaryLookupKeys(t.model, t.name);
      for (const std::string &lookupKey : dictionaryLookupKeys) {
        auto dictPath = TrussDictionary::Get(lookupKey);
        if (!dictPath)
          continue;
        resolved = LoadTrussDefinition(*dictPath, parsed);
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
        reasons.emplace_back("LoadTrussDefinition(dictionary model) returned false");
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
        symbolLooksRenderable && std::filesystem::exists(resolvedSymbolPathAfterDefinition);
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
  for (const auto &[uuid, t] : scene.trusses) {
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
    support.name = BuildHoistName(capacityKg, positionName);
    support.dummyProfileId = PickDummyHoistProfileId(capacityKg);
    support.motorName = support.name;
    support.motorNameSource = "Manual";
    support.motorManufacturerSource = "Manual";
    support.motorModelSource = "Manual";
    support.weightSource = "Manual";

    std::string layerName = defaultLayer;
    if (layerByType && !positionName.empty())
      layerName = "hoist " + positionName;
    else if (!layerByType && !positionName.empty())
      layerName = "pos " + positionName;
    support.layer = layerName;

    const std::string supportUuid = support.uuid;
    scene.supports[supportUuid] = support;
    addToLayer(layerName, supportUuid);
  };

  auto distributeAcrossTruss = [&](const std::string &positionName, int quantity,
                                   float capacityKg, float marginMm,
                                   const std::string &hoistFunction) {
    if (quantity <= 0)
      return;
    TrussInfo info;
    auto infoIt = trussInfo.find(positionName);
    if (infoIt != trussInfo.end())
      info = infoIt->second;
    float startX = info.found ? info.startX + marginMm : -500.0f * (quantity - 1);
    float endX = info.found ? info.endX - marginMm : 500.0f * (quantity - 1);
    if (endX < startX)
      std::swap(startX, endX);
    const float step = quantity > 1 ? (endX - startX) / static_cast<float>(quantity - 1) : 0.0f;
    const float y = info.found ? info.y : getHangPos(positionName);
    const float z = info.found ? info.z : getHangHeight(positionName);
    for (int i = 0; i < quantity; ++i)
      placeHoist(startX + step * i, y, z, capacityKg, positionName,
                 hoistFunction);
  };

  auto distributePaOrSidefill = [&](const std::string &positionName, int quantity,
                                     float capacityKg, bool sidefill,
                                     const std::string &hoistFunction) {
    if (quantity <= 0)
      return;
    const TrussInfo lxInfo = trussInfo.count("LX1") ? trussInfo["LX1"] : TrussInfo{};
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
      const int rows = count <= 2 ? count : static_cast<int>(std::ceil(std::sqrt(count)));
      const int cols = static_cast<int>(std::ceil(static_cast<float>(count) / rows));
      int placed = 0;
      for (int col = 0; col < cols && placed < count; ++col) {
        for (int row = 0; row < rows && placed < count; ++row) {
          float x = anchorX + xDirection * col * 1000.0f;
          float yPlaced = y + (rows == 1 ? 0.0f : (row - (rows - 1) * 0.5f) * 1000.0f);
          placeHoist(x, yPlaced, z, capacityKg, positionName, hoistFunction);
          ++placed;
        }
      }
    };

    placeGroup(leftCount, spanLeft - offsetX, -1.0f);
    placeGroup(rightCount, spanRight + offsetX, 1.0f);
  };

  for (const HoistRequest &request : hoistRequests) {
    const std::string hoistFunction = ResolveHoistFunctionForTarget(request.target);
    if (request.target == "LX") {
      std::vector<std::string> lxNames;
      for (const auto &[name, info] : trussInfo) {
        if (info.found && name.rfind("LX", 0) == 0)
          lxNames.push_back(name);
      }
      std::sort(lxNames.begin(), lxNames.end(), [](const std::string &a, const std::string &b) {
        return ParseTrailingNumber(a) < ParseTrailingNumber(b);
      });
      if (lxNames.empty())
        lxNames.push_back("LX1");
      int base = request.quantity / static_cast<int>(lxNames.size());
      int rem = request.quantity % static_cast<int>(lxNames.size());
      for (size_t i = 0; i < lxNames.size(); ++i) {
        int qty = base + (static_cast<int>(i) < rem ? 1 : 0);
        distributeAcrossTruss(lxNames[i], qty, request.capacityKg, 2000.0f,
                              hoistFunction);
      }
    } else if (request.target == "PA") {
      distributePaOrSidefill("P.A.", request.quantity, request.capacityKg, false,
                             hoistFunction);
    } else if (request.target == "SIDEFILL") {
      distributePaOrSidefill("SIDEFILL", request.quantity, request.capacityKg, true,
                             hoistFunction);
    } else if (request.target == "SCREEN") {
      distributeAcrossTruss("SCREEN", request.quantity, request.capacityKg, 0.0f,
                            hoistFunction);
    } else {
      distributeAcrossTruss(request.target, request.quantity, request.capacityKg,
                            2000.0f, hoistFunction);
    }
  }

  std::unordered_map<std::string, std::vector<Fixture *>> fixturesByPos;
  fixturesByPos.reserve(importedFixtureUuids.size());
  for (const std::string &uuid : importedFixtureUuids) {
    auto fixtureIt = scene.fixtures.find(uuid);
    if (fixtureIt == scene.fixtures.end())
      continue;
    fixturesByPos[fixtureIt->second.positionName].push_back(&fixtureIt->second);
  }

  for (auto &[pos, fixturesVec] : fixturesByPos) {
    if (fixturesVec.empty())
      continue;

    // Count fixtures by type
    std::unordered_map<std::string, int> counts;
    std::vector<std::string> types;
    for (Fixture *f : fixturesVec) {
      if (!counts.count(f->typeName))
        types.push_back(f->typeName);
      counts[f->typeName]++;
    }

    // Build ordering ensuring odd counts place one fixture at the center
    int total = static_cast<int>(fixturesVec.size());
    std::vector<std::string> center;
    for (const std::string &t : types) {
      if (counts[t] % 2 == 1) {
        center.push_back(t);
        counts[t]--; // leave an even number for pairing
      }
    }

    int pairsPerSide = (total - static_cast<int>(center.size())) / 2;
    std::vector<std::string> left;
    size_t idx = 0;
    while (static_cast<int>(left.size()) < pairsPerSide) {
      const std::string &t = types[idx % types.size()];
      if (counts[t] > 0) {
        left.push_back(t);
        counts[t] -= 2; // use one pair of this type
      }
      ++idx;
    }

    std::vector<std::string> order = left;
    order.insert(order.end(), center.begin(), center.end());
    std::vector<std::string> right = left;
    std::reverse(right.begin(), right.end());
    order.insert(order.end(), right.begin(), right.end());

    // Map fixtures by type for assignment
    std::unordered_map<std::string, std::vector<Fixture *>> byType;
    for (Fixture *f : fixturesVec)
      byType[f->typeName].push_back(f);
    for (auto &[type, vec] : byType)
      std::reverse(vec.begin(), vec.end());

    std::vector<Fixture *> ordered;
    ordered.reserve(total);
    for (const std::string &t : order) {
      auto &vec = byType[t];
      if (vec.empty())
        continue;
      ordered.push_back(vec.back());
      vec.pop_back();
    }

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
    float step = (total > 1) ? (endX - startX) / (total - 1) : 0.0f;

    for (int i = 0; i < total && i < static_cast<int>(ordered.size()); ++i) {
      Fixture *f = ordered[i];
      f->transform.o[0] = startX + i * step;
      f->transform.o[1] = baseY - width * 0.5f;
      f->transform.o[2] = baseZ;
    }
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

  std::unordered_set<std::string> importedFixtureIdSet(importedFixtureUuids.begin(),
                                                       importedFixtureUuids.end());
  int highestExistingFixtureId = 100;
  std::unordered_map<std::string, int> nextUnitByType;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (importedFixtureIdSet.count(uuid) != 0)
      continue;
    highestExistingFixtureId = std::max(highestExistingFixtureId, fixture.fixtureId);

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
    std::string prefix = vec.empty() ? type : baseName(vec.front()->instanceName);
    int nextUnitNumber = nextUnitByType[type] + 1;
    for (size_t i = 0; i < vec.size(); ++i) {
      vec[i]->fixtureId = baseId + static_cast<int>(i);
      vec[i]->unitNumber = nextUnitNumber + static_cast<int>(i);
      vec[i]->instanceName =
          prefix + " " + std::to_string(nextUnitNumber + static_cast<int>(i));
    }
    baseId =
        ((baseId - 1 + static_cast<int>(vec.size()) + 99) / 100) * 100 + 1;
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

  auto autoPref = cfg.GetValue("rider_autopatch");
  if (!autoPref || *autoPref != "0")
    AutoPatcher::AutoPatch(scene);
  return true;
}
