#include "riderimporter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "pdftext.h"

#include "configmanager.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "fixture.h"
#include "truss.h"


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

  std::regex trussRe("truss[^\n]*?(\\d+(?:\\.\\d+)?)\\s*m", std::regex::icase);
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
        lower.find("pantalla") != std::string::npos ||
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
    } else if (inRigging && std::regex_search(lower, m, trussRe)) {
      float length = std::stof(m[1]) * 1000.0f;
      std::string hang = currentHang;
      if (std::regex_search(line, hm, hangFindRe)) {
        hang = hm[1];
        std::transform(
            hang.begin(), hang.end(), hang.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      }
      int full = static_cast<int>(length) / 3000;
      float rem = length - full * 3000.0f;
      for (int i = 0; i < full; ++i) {
        Truss t;
        t.uuid = GenerateUuid();
        t.name = "Truss";
        t.layer = layer;
        t.lengthMm = 3000.0f;
        t.positionName = hang;
        scene.trusses[t.uuid] = t;
      }
      if (rem > 0.0f) {
        Truss t;
        t.uuid = GenerateUuid();
        t.name = "Truss";
        t.layer = layer;
        t.lengthMm = rem;
        t.positionName = hang;
        scene.trusses[t.uuid] = t;
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
