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
    "^\\s*(?:[-*]\\s*)?(\\d+)\\s+(?:truss)\\s+([^\\n]*?)\\s+(\\d+(?:\\.\\d+)?)\\s*m(?:\\s+para\\s+(.+))?",
    std::regex::icase);
static const std::regex kTrussRe(
    "(?:truss)[^\\n]*?(\\d+(?:\\.\\d+)?)\\s*m", std::regex::icase);
static const std::regex kFixtureLineRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s+(.+)$",
                                       std::regex::icase);
static const std::regex kQuantityOnlyRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s*$");
static const std::regex kHangLineRe("^\\s*(LX\\d+|floor|efectos?)\\s*:?\\s*$",
                                    std::regex::icase);
static const std::regex kHangFindRe("(LX\\d+|floor|efectos?)",
                                    std::regex::icase);
std::string Trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
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

// ExtractPdfText moved to pdftext.cpp
} // namespace

std::string RiderImporter::LoadText(const std::string &path) {
  if (path.size() < 4)
    return {};
  std::string ext = path.substr(path.size() - 4);
  for (auto &c : ext)
    c = static_cast<char>(std::tolower(c));
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

bool RiderImporter::ImportText(const std::string &text) {
  if (text.empty())
    return false;

  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();
  std::string defaultLayer = cfg.GetCurrentLayer();
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool layerByType = modeVal && *modeVal == "type";

  auto getHangHeight = [&](const std::string &posName) {
    if (posName.rfind("LX", 0) == 0) {
      try {
        int idx = std::stoi(posName.substr(2));
        if (idx >= 1 && idx <= 6) {
          return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_height") *
                 1000.0f;
        }
      } catch (...) {
      }
    }
    return 0.0f;
  };

  auto getHangPos = [&](const std::string &posName) {
    if (posName.rfind("LX", 0) == 0) {
      try {
        int idx = std::stoi(posName.substr(2));
        if (idx >= 1 && idx <= 6) {
          return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_pos") *
                 1000.0f;
        }
      } catch (...) {
      }
    }
    return 0.0f;
  };

  auto getHangMargin = [&](const std::string &posName) {
    if (posName.rfind("LX", 0) == 0) {
      try {
        int idx = std::stoi(posName.substr(2));
        if (idx >= 1 && idx <= 6) {
          return cfg.GetFloat("rider_lx" + std::to_string(idx) + "_margin") *
                 1000.0f;
        }
      } catch (...) {
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
  int pendingQuantity = 0;
  bool havePending = false;

  auto addFixtures = [&](int baseQuantity, const std::string &desc) {
    auto parts = SplitPlus(desc);
    for (const auto &partRaw : parts) {
      std::smatch pm;
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, kFixtureLineRe)) {
        quantity = std::stoi(pm[1]);
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
          std::string parsed = Trim(GetGdtfFixtureName(f.gdtfSpec));
          if (!parsed.empty())
            f.typeName = parsed;
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
      if (ContainsCaseInsensitive(captured, "efecto")) {
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
    } else if (std::regex_match(line, m, kTrussLineRe)) {
      int quantity = std::stoi(m[1]);
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
      if (m.size() > 4 && m[4].matched)
        hang = Trim(m[4]);
      std::transform(
          hang.begin(), hang.end(), hang.begin(),
          [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      if (hang.rfind("PUENTES ", 0) == 0)
        hang = Trim(hang.substr(8));
      else if (hang.rfind("PUENTE ", 0) == 0)
        hang = Trim(hang.substr(7));

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
          t.name = "TRUSS " + model + " " + sizeStr;
          t.model = t.name;
          if (auto dictPath = TrussDictionary::Get(t.model)) {
            namespace fs = std::filesystem;
            if (fs::path(*dictPath).extension() == ".gtruss") {
              Truss parsed;
              if (LoadTrussArchive(*dictPath, parsed)) {
                t.symbolFile = parsed.symbolFile;
                t.modelFile = parsed.modelFile;
                t.manufacturer = parsed.manufacturer;
                // Only overwrite dimensions if the loaded model provides
                // meaningful values.  Some rider truss entries in the
                // dictionary may contain zero sizes which would otherwise
                // break fixture distribution.  Keep the dummy dimensions in
                // that case so spacing remains correct.
                if (parsed.lengthMm > 0.0f)
                  t.lengthMm = parsed.lengthMm;
                if (parsed.widthMm > 0.0f)
                  t.widthMm = parsed.widthMm;
                if (parsed.heightMm > 0.0f)
                  t.heightMm = parsed.heightMm;
                t.weightKg = parsed.weightKg;
                t.crossSection = parsed.crossSection;
              } else {
                t.symbolFile = *dictPath;
                t.modelFile = *dictPath;
              }
            } else {
              t.symbolFile = *dictPath;
              t.modelFile = *dictPath;
            }
          }
          scene.trusses.emplace(t.uuid, std::move(t));
          addToLayer(t.layer, t.uuid);
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
        std::transform(
            hang.begin(), hang.end(), hang.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
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
        t.model = t.name;
        if (auto dictPath = TrussDictionary::Get(t.model)) {
          namespace fs = std::filesystem;
          if (fs::path(*dictPath).extension() == ".gtruss") {
            Truss parsed;
            if (LoadTrussArchive(*dictPath, parsed)) {
              t.symbolFile = parsed.symbolFile;
              t.modelFile = parsed.modelFile;
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
              t.symbolFile = *dictPath;
              t.modelFile = *dictPath;
            }
          } else {
            t.symbolFile = *dictPath;
            t.modelFile = *dictPath;
          }
        }
        scene.trusses.emplace(t.uuid, std::move(t));
        addToLayer(t.layer, t.uuid);
        x += s;
      }
    } else if (inFixtures && std::regex_match(line, m, kFixtureLineRe)) {
      int baseQuantity = std::stoi(m[1]);
      std::string desc = Trim(m[2]);
      addFixtures(baseQuantity, desc);
    } else if (inFixtures && std::regex_match(line, m, kQuantityOnlyRe)) {
      pendingQuantity = std::stoi(m[1]);
      havePending = true;
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

  std::unordered_map<std::string, std::vector<Fixture *>> fixturesByPos;
  fixturesByPos.reserve(scene.fixtures.size());
  for (auto &[uuid, f] : scene.fixtures)
    fixturesByPos[f.positionName].push_back(&f);

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

  // Assign fixture IDs and instance names grouped by type, ordering fixtures
  // from left to right within each hang position and front to back across
  // positions. IDs start at 101, 201, ...
  std::unordered_map<std::string, std::vector<Fixture *>> fixturesByType;
  for (auto &[uuid, f] : scene.fixtures)
    fixturesByType[f.typeName].push_back(&f);

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

  int baseId = 101;
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
    for (size_t i = 0; i < vec.size(); ++i) {
      vec[i]->fixtureId = baseId + static_cast<int>(i);
      vec[i]->unitNumber = static_cast<int>(i) + 1;
      vec[i]->instanceName = prefix + " " + std::to_string(i + 1);
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
  cfg.PushUndoState("import rider");
  return true;
}
