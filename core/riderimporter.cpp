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
        if (auto dictPath = GdtfDictionary::Get(f.typeName)) {
          f.gdtfSpec = *dictPath;
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

    if (lower.find("ilumin") != std::string::npos ||
        lower.find("robotica") != std::string::npos ||
        lower.find("convencion") != std::string::npos) {
      inFixtures = true;
      inRigging = false;
      havePending = false;
      continue;
    }
    if (lower.find("rigging") != std::string::npos) {
      inFixtures = false;
      inRigging = true;
      havePending = false;
      continue;
    }
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
          t.transform.o[2] = getHangHeight(posName) + t.heightMm * 0.5f;
          std::string sizeStr = formatLength(s);
          t.name = "TRUSS " + model + " " + sizeStr;
          t.model = t.name;
          if (auto dictPath = TrussDictionary::Get(t.model))
            t.symbolFile = *dictPath;
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
        t.transform.o[2] = getHangHeight(hang) + t.heightMm * 0.5f;
        std::string sizeStr = formatLength(s);
        t.name = "TRUSS " + sizeStr;
        t.model = t.name;
        if (auto dictPath = TrussDictionary::Get(t.model))
          t.symbolFile = *dictPath;
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

  cfg.PushUndoState("import rider");
  return true;
}
