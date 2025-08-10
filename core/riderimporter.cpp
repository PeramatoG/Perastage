#include "riderimporter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <regex>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pdftext.h"

#include "configmanager.h"
#include "fixture.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "truss.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include <filesystem>

namespace {
// Generate a random UUID4 string
std::string GenerateUuid() {
  static std::mt19937_64 rng{std::random_device{}()};
  static std::uniform_int_distribution<int> dist(0, 15);
  const char *v = "0123456789abcdef";
  int groups[] = {8, 4, 4, 4, 12};
  std::string out;
  for (int g = 0; g < 5; ++g) {
    if (g)
      out.push_back('-');
    for (int i = 0; i < groups[g]; ++i)
      out.push_back(v[dist(rng)]);
  }
  return out;
}

std::string Trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
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

bool RiderImporter::Import(const std::string &path) {
  std::string text;
  if (path.size() >= 4) {
    std::string ext = path.substr(path.size() - 4);
    for (auto &c : ext)
      c = static_cast<char>(std::tolower(c));
    if (ext == ".txt")
      text = ReadTextFile(path);
    else if (ext == ".pdf")
      text = ExtractPdfText(path);
  }
  if (text.empty())
    return false;

  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();
  std::string layer = cfg.GetCurrentLayer();

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

  // Keywords that identify truss entries. Screens or drapes themselves are
  // ignored; only explicit truss mentions are parsed.
  const std::string trussKeywords = "(?:truss)";
  // Full truss line like: "3 TRUSS 40X40 14m PARA PUENTES LX"
  std::regex trussLineRe(
      "^\\s*(?:[-*]\\s*)?(\\d+)\\s+" + trussKeywords +
          "\\s+([^\\n]*?)\\s+(\\d+(?:\\.\\d+)?)\\s*m(?:\\s+para\\s+(.+))?",
      std::regex::icase);
  // Generic catch-all to find any truss mention with a length
  std::regex trussRe(trussKeywords + "[^\\n]*?(\\d+(?:\\.\\d+)?)\\s*m",
                     std::regex::icase);
  std::regex fixtureLineRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s+(.+)$");
  std::regex quantityOnlyRe("^\\s*(?:[-*]\\s*)?(\\d+)\\s*$");
  // Allow matching generic hang positions like LX1, FLOOR or EFECTOS
  std::regex hangLineRe("^\\s*(LX\\d+|floor|efectos?)\\s*:?\\s*$",
                        std::regex::icase);
  std::regex hangFindRe("(LX\\d+|floor|efectos?)", std::regex::icase);
  std::istringstream iss(text);
  std::string line;
  bool inFixtures = false;
  bool inRigging = false;
  bool inControl = false;
  std::string currentHang;
  std::unordered_map<std::string, int> nameCounters;
  int pendingQuantity = 0;
  bool havePending = false;

  auto addFixtures = [&](int baseQuantity, const std::string &desc) {
    auto parts = SplitPlus(desc);
    for (const auto &partRaw : parts) {
      std::smatch pm;
      std::string part = partRaw;
      int quantity = baseQuantity;
      if (std::regex_match(partRaw, pm, fixtureLineRe)) {
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
        f.layer = layer;
        f.positionName = currentHang;
        f.transform.o[1] = getHangPos(currentHang);
        f.transform.o[2] = getHangHeight(currentHang);
        scene.fixtures[f.uuid] = f;
      }
    }
  };
  while (std::getline(iss, line)) {
    // Remove Windows carriage returns to allow regexes anchored with '$' to
    // match lines extracted from external tools.
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    std::string lower = line;
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower.find("sonido") != std::string::npos ||
        lower.find("audio") != std::string::npos ||
        lower.find("control de p.a.") != std::string::npos ||
        lower.find("monitores") != std::string::npos ||
        lower.find("microfon") != std::string::npos ||
        lower.find("video") != std::string::npos ||
        lower.find("realizacion") != std::string::npos ||
        lower.find("control") != std::string::npos) {
      inFixtures = false;
      inRigging = false;
      inControl = lower.find("control") != std::string::npos;
      havePending = false;
      continue;
    }
    if (lower.find("rigging") != std::string::npos) {
      inFixtures = false;
      inRigging = true;
      inControl = false;
      havePending = false;
      continue;
    }
    if (!inControl && (lower.find("ilumin") != std::string::npos ||
                       lower.find("robotica") != std::string::npos ||
                       lower.find("convencion") != std::string::npos)) {
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }

    std::smatch m;
    std::smatch hm;
    if (std::regex_match(line, hm, hangLineRe)) {
      havePending = false;
      std::string captured = hm[1];
      std::string capturedLower = captured;
      std::transform(
          capturedLower.begin(), capturedLower.end(), capturedLower.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (capturedLower.find("efecto") != std::string::npos) {
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
    } else if (std::regex_match(line, m, trussLineRe)) {
      int quantity = std::stoi(m[1]);
      std::string model = Trim(m[2]);
      float length = std::stof(m[3]) * 1000.0f;
      float width = 400.0f;
      float height = 400.0f;
      std::smatch dm;
      if (std::regex_search(
              model, dm,
              std::regex("(\\d+(?:\\.\\d+)?)\\s*[xX]\\s*(\\d+(?:\\.\\d+)?)"))) {
        width = std::stof(dm[1]) * 10.0f;
        height = std::stof(dm[2]) * 10.0f;
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
          t.layer = layer;
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
                t.lengthMm = parsed.lengthMm;
                t.widthMm = parsed.widthMm;
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
          scene.trusses[t.uuid] = t;
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
    } else if (std::regex_search(lower, m, trussRe)) {
      float length = std::stof(m[1]) * 1000.0f;
      std::string hang = currentHang;
      if (std::regex_search(line, hm, hangFindRe)) {
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
        t.layer = layer;
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
              t.lengthMm = parsed.lengthMm;
              t.widthMm = parsed.widthMm;
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
        scene.trusses[t.uuid] = t;
        x += s;
      }
    } else if (inFixtures && std::regex_match(line, m, fixtureLineRe)) {
      int baseQuantity = std::stoi(m[1]);
      std::string desc = Trim(m[2]);
      addFixtures(baseQuantity, desc);
    } else if (inFixtures && std::regex_match(line, m, quantityOnlyRe)) {
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
    float startX = info.found ? info.startX + margin :
                                -0.5f * ((total - 1) * 500.0f);
    float endX = info.found ? info.endX - margin :
                              0.5f * ((total - 1) * 500.0f);
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

  cfg.PushUndoState("import rider");
  return true;
}
