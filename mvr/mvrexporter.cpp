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
#include "mvrexporter.h"
#include "configmanager.h"
#include "dummyprofilelibrary.h"
#include "logger.h"
#include "matrixutils.h"
#include "projectutils.h"
#include "support.h"
#include "uuidutils.h"
#include "truss_gdtf_builder.h"

#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/zipstrm.h>

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct GdtfOverrides {
  std::string color;
  bool hasWeightKg = false;
  float weightKg = 0.0f;
  bool hasPowerW = false;
  float powerW = 0.0f;
  bool hasLengthMm = false;
  float lengthMm = 0.0f;
  bool hasWidthMm = false;
  float widthMm = 0.0f;
  bool hasHeightMm = false;
  float heightMm = 0.0f;
  std::string manufacturer;
  std::string model;
};

struct ResourceEntry {
  fs::path sourcePath;
  std::string archivePath;
};

struct ThreeDsChunkHeader {
  uint16_t id = 0;
  uint32_t length = 0;
};

static bool TryParseInt(std::string_view text, int &out);
static bool ParseMvrAddressNodeText(const std::string &text, int &universeOut,
                                    int &channelOut);
static bool TryComputeAbsoluteDmx(int universe1Based, int address1Based,
                                  int &absoluteOut);
static std::string ToLowerAscii(std::string value);

static bool ShouldExportSupportHoistInfo(const Support &support);
static tinyxml2::XMLElement *FindFirstPerastageUserData(tinyxml2::XMLElement *node);
static tinyxml2::XMLElement *FindOrCreatePerastageDataNode(tinyxml2::XMLDocument &doc,
                                                            tinyxml2::XMLElement *node);
static void AppendSupportHoistInfoUserData(tinyxml2::XMLDocument &doc,
                                           tinyxml2::XMLElement *supportData,
                                           const Support &support);
static bool IsCanonicalUuidString(const std::string &value);
static void LogLegacyPositionUuidWarning(const std::string &message);

static constexpr const char *kMvrProvider = "Perastage";
static constexpr const char *kMvrProviderVersion = "1.0";
static constexpr const char *kFallbackFixtureGdtfFileName = "Generic 1ch.gdtf";

static bool Read3dsChunkHeader(std::ifstream &file, ThreeDsChunkHeader &chunk) {
  if (!file.read(reinterpret_cast<char *>(&chunk.id), sizeof(chunk.id)))
    return false;
  if (!file.read(reinterpret_cast<char *>(&chunk.length), sizeof(chunk.length)))
    return false;
  return true;
}

static std::string Read3dsCString(std::ifstream &file, std::streampos endPos) {
  std::string output;
  char ch = 0;
  while (file.tellg() < endPos && file.read(&ch, 1)) {
    if (ch == '\0')
      break;
    output.push_back(ch);
  }
  return output;
}

static std::vector<std::string> Collect3dsTextureReferences(const fs::path &modelPath) {
  std::vector<std::string> references;
  std::ifstream file(modelPath, std::ios::binary);
  if (!file.is_open())
    return references;

  ThreeDsChunkHeader root;
  if (!Read3dsChunkHeader(file, root) || root.id != 0x4D4D)
    return references;

  std::unordered_set<std::string> seenRefs;
  const std::streampos rootEnd = static_cast<std::streampos>(root.length);
  while (file.tellg() < rootEnd) {
    ThreeDsChunkHeader chunk;
    if (!Read3dsChunkHeader(file, chunk))
      break;
    const std::streampos chunkData = file.tellg();
    const std::streampos chunkEnd =
        chunkData + static_cast<std::streamoff>(chunk.length - 6);
    if (chunk.id != 0x3D3D) {
      file.seekg(chunkEnd);
      continue;
    }

    while (file.tellg() < chunkEnd) {
      ThreeDsChunkHeader sub;
      if (!Read3dsChunkHeader(file, sub))
        break;
      const std::streampos subData = file.tellg();
      const std::streampos subEnd =
          subData + static_cast<std::streamoff>(sub.length - 6);
      if (sub.id != 0xAFFF) {
        file.seekg(subEnd);
        continue;
      }

      while (file.tellg() < subEnd) {
        ThreeDsChunkHeader matChunk;
        if (!Read3dsChunkHeader(file, matChunk))
          break;
        const std::streampos matData = file.tellg();
        const std::streampos matEnd =
            matData + static_cast<std::streamoff>(matChunk.length - 6);
        if (matChunk.id != 0xA200) {
          file.seekg(matEnd);
          continue;
        }

        while (file.tellg() < matEnd) {
          ThreeDsChunkHeader texChunk;
          if (!Read3dsChunkHeader(file, texChunk))
            break;
          const std::streampos texData = file.tellg();
          const std::streampos texEnd =
              texData + static_cast<std::streamoff>(texChunk.length - 6);
          if (texChunk.id == 0xA300) {
            const std::string value = Read3dsCString(file, texEnd);
            if (!value.empty() && seenRefs.insert(ToLowerAscii(value)).second)
              references.push_back(value);
          }
          file.seekg(texEnd);
        }
      }
    }
  }

  return references;
}

static bool ResolveTextureDependencyPath(const fs::path &modelPath,
                                         const std::string &textureRef,
                                         fs::path &resolvedPath) {
  if (textureRef.empty())
    return false;

  const fs::path refPath = fs::u8path(textureRef);
  if (refPath.is_absolute() && fs::exists(refPath)) {
    resolvedPath = refPath;
    return true;
  }

  const fs::path modelDir =
      modelPath.has_parent_path() ? modelPath.parent_path() : fs::path();
  if (modelDir.empty())
    return false;

  const fs::path direct = modelDir / refPath;
  if (fs::exists(direct)) {
    resolvedPath = direct;
    return true;
  }

  std::error_code ec;
  for (const auto &entry :
       fs::directory_iterator(modelDir, fs::directory_options::skip_permission_denied, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;
    if (ToLowerAscii(entry.path().filename().string()) ==
        ToLowerAscii(refPath.filename().string())) {
      resolvedPath = entry.path();
      return true;
    }
  }

  return false;
}

enum class TrussGeometryAuthority {
  MvrGeometry = 0,
  Gdtf = 1,
};

static TrussGeometryAuthority GetTrussGeometryAuthoritySetting() {
  const float rawValue = ConfigManager::Get().GetFloat("mvr_truss_geometry_authority");
  return rawValue >= 0.5f ? TrussGeometryAuthority::Gdtf : TrussGeometryAuthority::MvrGeometry;
}

static std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char c) { return std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                          [&](unsigned char c) { return !isSpace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](unsigned char c) { return !isSpace(c); }).base(),
              value.end());
  return value;
}

static std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static void LogLegacyPositionUuidWarning(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Warn, message);
}

static std::string TruncateFileNamePreservingExtension(const std::string &fileName,
                                                       size_t maxLength) {
  if (fileName.size() <= maxLength)
    return fileName;

  fs::path filePath(fileName);
  const std::string extension = filePath.extension().generic_string();
  const std::string stem = filePath.stem().generic_string();

  if (maxLength <= extension.size())
    return fileName.substr(0, maxLength);

  const size_t stemMaxLength = maxLength - extension.size();
  std::string truncatedStem = stem.substr(0, stemMaxLength);
  if (truncatedStem.empty())
    return fileName.substr(0, maxLength);
  return truncatedStem + extension;
}

static std::string ResolveFallbackFixtureGdtfPath() {
  static const std::string resolvedPath = []() {
    const fs::path fallbackPath =
        ProjectUtils::GetBaseLibraryPath("fixtures") / kFallbackFixtureGdtfFileName;
    std::error_code ec;
    if (fs::exists(fallbackPath, ec) && !ec && fs::is_regular_file(fallbackPath, ec) && !ec)
      return fallbackPath.generic_string();
    return std::string{};
  }();
  return resolvedPath;
}

static std::string EnsureUniqueArchivePath(const std::string &proposed,
                                           std::unordered_set<std::string> &usedPaths) {
  constexpr size_t kMaxArchiveEntryNameLength = 120;
  fs::path path = fs::path(proposed).lexically_normal();
  std::string normalized =
      TruncateFileNamePreservingExtension(path.generic_string(), kMaxArchiveEntryNameLength);
  if (normalized.empty())
    normalized = "resource.bin";
  if (!usedPaths.contains(normalized)) {
    usedPaths.insert(normalized);
    return normalized;
  }

  fs::path stemPath = fs::path(normalized);
  std::string ext = stemPath.extension().generic_string();
  std::string stem = stemPath.stem().generic_string();
  fs::path parent = stemPath.parent_path();
  int index = 1;
  while (true) {
    const std::string suffix = "_" + std::to_string(index + 1);
    std::string adjustedStem = stem;
    const size_t candidateMaxStemLength =
        (kMaxArchiveEntryNameLength > ext.size() + suffix.size())
            ? kMaxArchiveEntryNameLength - ext.size() - suffix.size()
            : 0;
    if (adjustedStem.size() > candidateMaxStemLength)
      adjustedStem = adjustedStem.substr(0, candidateMaxStemLength);
    std::string candidate =
        (parent / (adjustedStem + suffix + ext)).generic_string();
    if (!usedPaths.contains(candidate)) {
      usedPaths.insert(candidate);
      return candidate;
    }
    ++index;
  }
}

static std::string SanitizeArchiveFileName(const std::string &input,
                                           const std::string &fallbackName) {
  constexpr size_t kMaxArchiveFileNameLength = 120;
  auto sanitizeSingleFileName = [](std::string fileName,
                                   const std::string &fallback) {
    if (fileName.empty())
      fileName = fallback;

    for (char &ch : fileName) {
      const unsigned char uch = static_cast<unsigned char>(ch);
      if (uch < 32 || ch == ':' || ch == '\\' || ch == '/' || ch == '*' ||
          ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
        ch = '_';
      }
    }

    if (fileName.empty())
      fileName = fallback;
    return fileName;
  };

  std::string candidate = TrimAscii(input);
  std::replace(candidate.begin(), candidate.end(), '\\', '/');
  if (candidate.empty())
    return TruncateFileNamePreservingExtension(
        sanitizeSingleFileName("", fallbackName), kMaxArchiveFileNameLength);

  const std::string fileName = fs::path(candidate).filename().generic_string();
  if (!fileName.empty())
    return TruncateFileNamePreservingExtension(
        sanitizeSingleFileName(fileName, fallbackName), kMaxArchiveFileNameLength);
  return TruncateFileNamePreservingExtension(
      sanitizeSingleFileName("", fallbackName), kMaxArchiveFileNameLength);
}

static std::string BuildTrussGdtfArchiveName(const Truss &truss) {
  std::string baseName = TrimAscii(truss.model);
  if (baseName.empty()) {
    if (truss.lengthMm > 0.0f) {
      const int lengthMeters = static_cast<int>(std::lround(truss.lengthMm / 1000.0f));
      if (lengthMeters > 0)
        baseName = "truss " + std::to_string(lengthMeters) + "m";
    }
  }
  if (baseName.empty())
    baseName = "truss";

  fs::path candidate(baseName);
  if (candidate.extension().empty())
    baseName += ".gdtf";

  return SanitizeArchiveFileName(baseName, "truss.gdtf");
}

static std::string BuildTrussTypeKey(const Truss &truss) {
  std::ostringstream key;
  key << TrimAscii(truss.gdtfSpec) << '|'
      << TrimAscii(truss.modelFile) << '|'
      << TrimAscii(truss.manufacturer) << '|'
      << TrimAscii(truss.model) << '|'
      << TrimAscii(truss.crossSection) << '|'
      << truss.lengthMm << '|'
      << truss.widthMm << '|'
      << truss.heightMm << '|'
      << truss.weightKg;
  return key.str();
}

static const char *ToRepresentationText(Truss::GeometryRepresentation representation) {
  switch (representation) {
  case Truss::GeometryRepresentation::SymbolSymdef:
    return "SymbolSymdef";
  case Truss::GeometryRepresentation::Geometry3D:
    return "Geometry3D";
  case Truss::GeometryRepresentation::PublicGdtf:
    return "PublicGdtf";
  case Truss::GeometryRepresentation::NativePerastage:
    return "NativePerastage";
  default:
    return "Unknown";
  }
}

static bool IsValidMvrFileName(const std::string &value) {
  if (value.empty())
    return false;
  if (value.front() == '/' || value.find("..") != std::string::npos)
    return false;
  for (unsigned char c : value) {
    if (c < 32)
      return false;
  }
  return value.find(':') == std::string::npos && value.find('\\') == std::string::npos &&
         value.find('*') == std::string::npos && value.find('?') == std::string::npos &&
         value.find('"') == std::string::npos && value.find('<') == std::string::npos &&
         value.find('>') == std::string::npos && value.find('|') == std::string::npos;
}

static bool IsCanonicalUuidString(const std::string &value) {
  if (value.empty())
    return false;
  return CanonicalizeUuid(value) == value;
}

static bool ValidateMvr16Export(
    tinyxml2::XMLDocument &doc,
    const std::unordered_map<std::string, std::string> &gdtfPathsByUuid,
    const std::unordered_map<std::string, int> &archiveEntryCount) {
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  if (!root) {
    wxLogError("MVR export validation failed: missing GeneralSceneDescription root");
    return false;
  }

  if (root->IntAttribute("verMajor") != 1 || root->IntAttribute("verMinor") != 6) {
    wxLogError("MVR export validation failed: root version must be 1.6");
    return false;
  }

  const char *provider = root->Attribute("provider");
  const char *providerVersion = root->Attribute("providerVersion");
  if (!provider || std::string(provider).empty() || !providerVersion ||
      std::string(providerVersion).empty()) {
    wxLogError("MVR export validation failed: provider/providerVersion are required for MVR 1.6");
    return false;
  }

  std::unordered_set<int> numericIds;
  std::vector<std::string> referencedFiles;
  std::unordered_set<std::string> positionUuids;
  std::unordered_set<std::string> referencedPositionUuids;

  if (tinyxml2::XMLElement *scene = root->FirstChildElement("Scene")) {
    if (scene->FirstChildElement("UserData")) {
      wxLogError("MVR export validation failed: Scene/UserData is not valid in MVR 1.6");
      return false;
    }
    if (tinyxml2::XMLElement *aux = scene->FirstChildElement("AUXData")) {
      for (tinyxml2::XMLElement *pos = aux->FirstChildElement("Position"); pos;
           pos = pos->NextSiblingElement("Position")) {
        const std::string uuid = TrimAscii(pos->Attribute("uuid") ? pos->Attribute("uuid") : "");
        if (!IsCanonicalUuidString(uuid)) {
          wxLogError("MVR export validation failed: Position uuid '%s' is not canonical", uuid);
          return false;
        }
        positionUuids.insert(uuid);
      }
    }
  }

  std::vector<tinyxml2::XMLElement *> stack;
  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    stack.push_back(node);
  }

  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *cur = stack.back();
      stack.pop_back();
        if (std::string(cur->Name()) == "Fixture") {
        auto *idNode = cur->FirstChildElement("FixtureID");
        auto *numNode = cur->FirstChildElement("FixtureIDNumeric");
        const std::string fixtureUuid = cur->Attribute("uuid") ? cur->Attribute("uuid") : "(missing uuid)";
        const std::string fixtureName = cur->Attribute("name") ? cur->Attribute("name") : "(unnamed fixture)";

        const std::string fixtureId =
            idNode && idNode->GetText() ? TrimAscii(idNode->GetText()) : "";
        const std::string fixtureNumericText =
            numNode && numNode->GetText() ? TrimAscii(numNode->GetText()) : "";
        int fixtureNumeric = 0;

        if (fixtureId.empty() || !TryParseInt(fixtureNumericText, fixtureNumeric) ||
            fixtureNumeric <= 0) {
          wxLogError(
              "MVR export validation failed: Fixture '%s' (uuid=%s) must have a non-empty FixtureID and a positive integer FixtureIDNumeric",
              fixtureName, fixtureUuid);
          return false;
        }

        if (auto *unitNode = cur->FirstChildElement("UnitNumber"); unitNode) {
          int unitValue = 0;
          const std::string unitText = unitNode->GetText() ? TrimAscii(unitNode->GetText()) : "";
          if (unitText.empty() || !TryParseInt(unitText, unitValue)) {
            wxLogError(
                "MVR export validation failed: Fixture '%s' (uuid=%s) has non-integer UnitNumber",
                fixtureName, fixtureUuid);
            return false;
          }
        }

        if (auto *addresses = cur->FirstChildElement("Addresses"); addresses) {
          std::unordered_set<int> usedBreaks;
          for (auto *addressNode = addresses->FirstChildElement("Address"); addressNode;
               addressNode = addressNode->NextSiblingElement("Address")) {
            int breakNum = 0;
            const std::string breakText = addressNode->Attribute("break")
                                              ? TrimAscii(addressNode->Attribute("break"))
                                              : "0";
            if (!TryParseInt(breakText, breakNum) || breakNum < 0) {
              wxLogError(
                  "MVR export validation failed: Fixture '%s' (uuid=%s) has invalid Address break '%s'",
                  fixtureName, fixtureUuid, breakText);
              return false;
            }
            if (!usedBreaks.insert(breakNum).second) {
              wxLogError(
                  "MVR export validation failed: Fixture '%s' (uuid=%s) has duplicate Address break %d",
                  fixtureName, fixtureUuid, breakNum);
              return false;
            }

            const std::string addressText =
                addressNode->GetText() ? TrimAscii(addressNode->GetText()) : "";
            int universe = 0;
            int channel = 0;
            if (!ParseMvrAddressNodeText(addressText, universe, channel)) {
              wxLogError(
                  "MVR export validation failed: Fixture '%s' (uuid=%s) has invalid Address value '%s'",
                  fixtureName, fixtureUuid, addressText);
              return false;
            }

          }
        }
      }

      if (std::string(cur->Name()) == "Position") {
        const tinyxml2::XMLElement *parent = cur->Parent() ? cur->Parent()->ToElement() : nullptr;
        const std::string parentName = parent ? std::string(parent->Name()) : std::string{};
        const bool isObjectPositionRef =
            parentName == "Fixture" || parentName == "Truss" || parentName == "Support" ||
            parentName == "VideoScreen" || parentName == "Projector";
        if (isObjectPositionRef) {
          const std::string positionRef = TrimAscii(cur->GetText() ? cur->GetText() : "");
          if (positionRef.empty())
            continue;
          if (!IsCanonicalUuidString(positionRef)) {
            wxLogError("MVR export validation failed: Position reference '%s' is not canonical", positionRef);
            return false;
          }
          referencedPositionUuids.insert(positionRef);
        }
      }

      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement())
        stack.push_back(child);
    }
  }
  while (!stack.empty()) {
    tinyxml2::XMLElement *cur = stack.back();
    stack.pop_back();

    if (std::string(cur->Name()) == "GDTFSpec") {
      const char *txt = cur->GetText();
      if (txt)
        referencedFiles.emplace_back(txt);
    }

    for (const tinyxml2::XMLAttribute *attr = cur->FirstAttribute(); attr;
         attr = attr->Next()) {
      std::string attrName = attr->Name();
      std::transform(attrName.begin(), attrName.end(), attrName.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (attrName == "filename")
        referencedFiles.emplace_back(attr->Value());
    }

    for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }

  for (const char *tagName : {"Truss", "Support", "VideoScreen", "Projector"}) {
    for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
         node = node->NextSiblingElement()) {
      std::vector<tinyxml2::XMLElement *> stack;
      stack.push_back(node);
      while (!stack.empty()) {
        tinyxml2::XMLElement *cur = stack.back();
        stack.pop_back();
        if (std::string(cur->Name()) == tagName) {
          bool isMultipatchChild = false;
          if (const char *mp = cur->Attribute("multipatch"); mp)
            isMultipatchChild = std::string(mp) == "true" || std::string(mp) == "1";
          if (!isMultipatchChild) {
            const char *idText = cur->FirstChildElement("FixtureID")
                                     ? cur->FirstChildElement("FixtureID")->GetText()
                                     : nullptr;
            const char *numText = cur->FirstChildElement("FixtureIDNumeric")
                                      ? cur->FirstChildElement("FixtureIDNumeric")->GetText()
                                      : nullptr;
            if (!idText || TrimAscii(idText).empty() || !numText) {
              wxLogError("MVR export validation failed: %s is missing FixtureID/FixtureIDNumeric", tagName);
              return false;
            }
            int numeric = 0;
            if (!TryParseInt(numText, numeric) || numeric <= 0 || !numericIds.insert(numeric).second) {
              wxLogError("MVR export validation failed: FixtureIDNumeric must be globally unique positive integer");
              return false;
            }
          }

          if (tinyxml2::XMLElement *gdtf = cur->FirstChildElement("GDTFSpec")) {
            const char *txt = gdtf->GetText();
            std::string value = txt ? txt : "";
            if (!IsValidMvrFileName(value)) {
              wxLogError("MVR export validation failed: GDTFSpec '%s' is not a valid archive-relative FileName", value);
              return false;
            }
            auto uidIt = gdtfPathsByUuid.find(cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
            if (uidIt != gdtfPathsByUuid.end() && uidIt->second != value) {
              wxLogError("MVR export validation failed: GDTFSpec mismatch for object uuid '%s'", cur->Attribute("uuid"));
              return false;
            }
            auto gdtfArchiveIt = archiveEntryCount.find(value);
            if (gdtfArchiveIt == archiveEntryCount.end() || gdtfArchiveIt->second != 1) {
              wxLogError("MVR export validation failed: GDTFSpec '%s' must be present exactly once in archive", value);
              return false;
            }
          }

          if (std::string(tagName) == "Support") {
            if (!cur->FirstChildElement("Geometries")) {
              wxLogError("MVR export validation failed: Support uuid '%s' has no Geometries",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
            if (!cur->FirstChildElement("ChainLength")) {
              wxLogError("MVR export validation failed: Support uuid '%s' has no ChainLength",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
            const bool hasGdtfSpec = cur->FirstChildElement("GDTFSpec") != nullptr;
            const bool hasGdtfMode = cur->FirstChildElement("GDTFMode") != nullptr;
            if (hasGdtfSpec != hasGdtfMode) {
              wxLogError("MVR export validation failed: Support uuid '%s' has inconsistent GDTFSpec/GDTFMode",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
          }
        }

        for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
             child = child->NextSiblingElement())
          stack.push_back(child);
      }
    }
  }

  for (const auto &fileRef : referencedFiles) {
    if (!IsValidMvrFileName(fileRef)) {
      wxLogError("MVR export validation failed: invalid FileName reference '%s'", fileRef);
      return false;
    }

    auto fileRefIt = archiveEntryCount.find(fileRef);
    if (fileRefIt == archiveEntryCount.end() || fileRefIt->second != 1) {
      wxLogError("MVR export validation failed: FileName reference '%s' must be present exactly once in archive", fileRef);
      return false;
    }
  }

  for (const auto &[archivePath, count] : archiveEntryCount) {
    if (archivePath.empty()) {
      wxLogError("MVR export validation failed: found empty ZIP entry path");
      return false;
    }
    if (count != 1) {
      wxLogError("MVR export validation failed: duplicate ZIP entry '%s'", archivePath);
      return false;
    }
  }

  for (const auto &positionRef : referencedPositionUuids) {
    if (!positionUuids.contains(positionRef)) {
      wxLogError("MVR export validation failed: Position reference '%s' has no AUXData/Position definition", positionRef);
      return false;
    }
  }

  return true;
}

static bool TryParseInt(std::string_view text, int &out) {
  if (text.empty())
    return false;

  const auto first = std::find_if_not(text.begin(), text.end(),
                                      [](unsigned char c) { return std::isspace(c); });
  if (first == text.end())
    return false;
  const auto last = std::find_if_not(text.rbegin(), text.rend(),
                                     [](unsigned char c) { return std::isspace(c); }).base();
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

static std::pair<int, int> ParseAddress(const std::string &addr) {
  size_t dot = addr.find('.');
  if (dot == std::string::npos)
    return {0, 0};
  int u = 0, c = 0;
  TryParseInt(std::string_view(addr).substr(0, dot), u);
  TryParseInt(std::string_view(addr).substr(dot + 1), c);
  return {u, c};
}

static bool ParseMvrAddressNodeText(const std::string &text, int &universeOut,
                                    int &channelOut) {
  const std::string trimmed = TrimAscii(text);
  if (trimmed.empty())
    return false;

  if (trimmed.find('.') != std::string::npos) {
    auto [u, c] = ParseAddress(trimmed);
    if (u < 1 || c < 1 || c > 512)
      return false;
    universeOut = u;
    channelOut = c;
    return true;
  }

  int absolute = 0;
  if (!TryParseInt(trimmed, absolute) || absolute < 1)
    return false;

  universeOut = ((absolute - 1) / 512) + 1;
  channelOut = ((absolute - 1) % 512) + 1;
  return true;
}

int ComputeAbsoluteDmx(int universe1Based, int address1Based) {
  return ((universe1Based - 1) * 512) + address1Based;
}

static bool ShouldExportSupportHoistInfo(const Support &support) {
  return support.capacityKg != 0.0f || support.weightKg != 0.0f ||
         support.loadKg != 0.0f ||
         !support.hoistFunction.empty() || !support.motorName.empty() ||
         !support.motorManufacturer.empty() || !support.motorModel.empty() ||
         !support.motorFixtureUuid.empty() || !support.useMotorDefaults ||
         !support.dummyProfileId.empty() || !support.dummyPreset.empty() ||
         NormalizeHoistDataSource(support.hoistDataSource) != "Inherited" ||
         NormalizeHoistDataSource(support.motorNameSource) != "Inherited" ||
         NormalizeHoistDataSource(support.motorManufacturerSource) != "Inherited" ||
         NormalizeHoistDataSource(support.motorModelSource) != "Inherited" ||
         NormalizeHoistDataSource(support.capacitySource) != "Inherited" ||
         NormalizeHoistDataSource(support.weightSource) != "Inherited" ||
         NormalizeHoistDataSource(support.hoistFunctionSource) != "Inherited";
}

static tinyxml2::XMLElement *FindFirstPerastageUserData(tinyxml2::XMLElement *node) {
  if (!node)
    return nullptr;

  for (tinyxml2::XMLElement *ud = node->FirstChildElement("UserData"); ud;
       ud = ud->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider = TrimAscii(
          data->Attribute("provider") ? data->Attribute("provider") : "");
      if (provider.empty() || ToLowerAscii(provider) == "perastage")
        return ud;
    }
  }

  return nullptr;
}

static tinyxml2::XMLElement *FindOrCreatePerastageDataNode(tinyxml2::XMLDocument &doc,
                                                            tinyxml2::XMLElement *node) {
  tinyxml2::XMLElement *ud = FindFirstPerastageUserData(node);
  if (!ud) {
    ud = doc.NewElement("UserData");
    node->InsertEndChild(ud);
  }

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const std::string provider =
        TrimAscii(data->Attribute("provider") ? data->Attribute("provider") : "");
    if (provider.empty() || ToLowerAscii(provider) == "perastage")
      return data;
  }

  tinyxml2::XMLElement *data = doc.NewElement("Data");
  data->SetAttribute("provider", kMvrProvider);
  data->SetAttribute("ver", kMvrProviderVersion);
  ud->InsertEndChild(data);
  return data;
}

static void AppendSupportHoistInfoUserData(tinyxml2::XMLDocument &doc,
                                           tinyxml2::XMLElement *supportData,
                                           const Support &support) {
  tinyxml2::XMLElement *info = doc.NewElement("HoistInfo");
  info->SetAttribute("uuid", support.uuid.c_str());

  auto addText = [&](const char *name, const std::string &value) {
    if (value.empty())
      return;
    tinyxml2::XMLElement *node = doc.NewElement(name);
    node->SetText(value.c_str());
    info->InsertEndChild(node);
  };

  auto addNum = [&](const char *name, float value, const char *unit) {
    if (value == 0.0f)
      return;
    tinyxml2::XMLElement *node = doc.NewElement(name);
    node->SetAttribute("unit", unit);
    node->SetText(std::to_string(value).c_str());
    info->InsertEndChild(node);
  };

  addNum("Capacity", support.capacityKg, "kg");
  addNum("Weight", support.weightKg, "kg");
  addNum("Load", support.loadKg, "kg");

  const std::string hoistFunction = NormalizeHoistFunction(support.hoistFunction);
  if (!hoistFunction.empty()) {
    addText("RiggingPoint", hoistFunction);
    addText("Function", hoistFunction); // Compatibility alias for older builds.
  }

  addText("MotorName", support.motorName);
  addText("MotorManufacturer", support.motorManufacturer);
  addText("MotorModel", support.motorModel);
  addText("MotorFixtureUuid", support.motorFixtureUuid);

  if (!support.useMotorDefaults)
    addText("UseMotorDefaults", "false");

  addText("DummyProfileId", support.dummyProfileId);
  if (!support.dummyPreset.empty()) {
    addText("DummyPreset", support.dummyPreset);
  } else if (!support.dummyProfileId.empty()) {
    const auto profile = DummyProfileLibrary::FindById(support.dummyProfileId);
    if (profile.has_value())
      addText("DummyPreset", profile->displayName);
  }

  const std::string source = NormalizeHoistDataSource(support.hoistDataSource);
  addText("ValueSource", source);
  addText("DataSource", source); // Compatibility alias for older builds.

  addText("MotorNameSource",
          ResolveHoistFieldDataSource(support.motorNameSource, source));
  addText("MotorManufacturerSource",
          ResolveHoistFieldDataSource(support.motorManufacturerSource, source));
  addText("MotorModelSource",
          ResolveHoistFieldDataSource(support.motorModelSource, source));
  addText("CapacitySource",
          ResolveHoistFieldDataSource(support.capacitySource, source));
  addText("WeightSource",
          ResolveHoistFieldDataSource(support.weightSource, source));
  const std::string hoistFunctionSource =
      ResolveHoistFieldDataSource(support.hoistFunctionSource, source);
  addText("RiggingPointSource", hoistFunctionSource);
  addText("FunctionSource", hoistFunctionSource); // Compatibility alias.

  supportData->InsertEndChild(info);
}

static bool TryComputeAbsoluteDmx(int universe1Based, int address1Based,
                                  int &absoluteOut) {
  if (universe1Based < 1 || address1Based < 1 || address1Based > 512)
    return false;
  absoluteOut = ComputeAbsoluteDmx(universe1Based, address1Based);
  return true;
}

static std::string HexToCie(const std::string &hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return {};
  unsigned int rgb = 0;
  std::istringstream iss(hex.substr(1));
  iss >> std::hex >> rgb;
  unsigned int R = (rgb >> 16) & 0xFF;
  unsigned int G = (rgb >> 8) & 0xFF;
  unsigned int B = rgb & 0xFF;
  auto invGamma = [](double c) {
    return c <= 0.04045 ? c / 12.92
                        : std::pow((c + 0.055) / 1.055, 2.4);
  };
  double r = invGamma(R / 255.0);
  double g = invGamma(G / 255.0);
  double b = invGamma(B / 255.0);
  double X = 0.4124 * r + 0.3576 * g + 0.1805 * b;
  double Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  double Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
  double sum = X + Y + Z;
  double x = 0.0, y = 0.0;
  if (sum > 0.0) {
    x = X / sum;
    y = Y / sum;
  }
  std::ostringstream colStr;
  colStr << std::fixed << std::setprecision(6) << x << "," << y << "," << Y;
  return colStr.str();
}

static std::string CreateTempDir() {
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  fs::path base = fs::temp_directory_path();
  fs::path full = base / ("GDTF_" + std::to_string(now));
  fs::create_directory(full);
  return full.string();
}

static bool ExtractZip(const std::string &zipPath, const std::string &destDir) {
  if (!fs::exists(zipPath))
    return false;
  wxLogNull logNo;
  wxFileInputStream input(zipPath);
  if (!input.IsOk())
    return false;
  wxZipInputStream zipStream(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipStream.GetNextEntry())), entry) {
    std::string filename = entry->GetName().ToStdString();
    std::string fullPath = destDir + "/" + filename;
    if (entry->IsDir()) {
      wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
      continue;
    }
    wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT,
                      wxPATH_MKDIR_FULL);
    std::ofstream output(fullPath, std::ios::binary);
    if (!output.is_open())
      return false;
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

static bool ZipDir(const std::string &srcDir, const std::string &dstZip) {
  wxFileOutputStream output(dstZip);
  if (!output.IsOk())
    return false;
  wxZipOutputStream zip(output);
  for (auto &p : fs::recursive_directory_iterator(srcDir)) {
    if (!p.is_regular_file())
      continue;
    fs::path rel = fs::relative(p.path(), srcDir);
    auto *e = new wxZipEntry(rel.generic_string());
    e->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(e);
    std::ifstream in(p.path(), std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }
  zip.Close();
  return true;
}

static std::string CreatePatchedGdtf(const std::string &gdtfPath,
                                     const GdtfOverrides &ov) {
  std::string tempDir = CreateTempDir();
  if (!ExtractZip(gdtfPath, tempDir))
    return {};
  std::string descPath = tempDir + "/description.xml";
  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
    return {};
  tinyxml2::XMLElement *ft = doc.FirstChildElement("GDTF");
  if (ft)
    ft = ft->FirstChildElement("FixtureType");
  else
    ft = doc.FirstChildElement("FixtureType");
  if (!ft)
    return {};
  if (!ov.color.empty()) {
    tinyxml2::XMLElement *models = ft->FirstChildElement("Models");
    if (models) {
      std::string cie = HexToCie(ov.color);
      for (tinyxml2::XMLElement *m = models->FirstChildElement("Model"); m;
           m = m->NextSiblingElement("Model"))
        m->SetAttribute("Color", cie.c_str());
    }
  }
  if (ov.hasWeightKg || ov.hasPowerW) {
    tinyxml2::XMLElement *phys = ft->FirstChildElement("PhysicalDescriptions");
    if (!phys)
      phys = ft->InsertNewChildElement("PhysicalDescriptions");
    tinyxml2::XMLElement *props = phys->FirstChildElement("Properties");
    if (!props)
      props = phys->InsertNewChildElement("Properties");
    if (ov.hasWeightKg) {
      tinyxml2::XMLElement *w = props->FirstChildElement("Weight");
      if (!w)
        w = props->InsertNewChildElement("Weight");
      w->SetAttribute("Value", ov.weightKg);
    }
    if (ov.hasPowerW) {
      tinyxml2::XMLElement *p = props->FirstChildElement("PowerConsumption");
      if (!p)
        p = props->InsertNewChildElement("PowerConsumption");
      p->SetAttribute("Value", ov.powerW);
    }
  }
  if (!ov.manufacturer.empty())
    ft->SetAttribute("Manufacturer", ov.manufacturer.c_str());
  if (!ov.model.empty())
    ft->SetAttribute("Name", ov.model.c_str());

  if (ov.hasLengthMm || ov.hasWidthMm || ov.hasHeightMm) {
    tinyxml2::XMLElement *models = ft->FirstChildElement("Models");
    tinyxml2::XMLElement *model =
        models ? models->FirstChildElement("Model") : nullptr;
    if (!models)
      models = ft->InsertNewChildElement("Models");
    if (!model)
      model = models->InsertNewChildElement("Model");
    if (ov.hasLengthMm)
      model->SetAttribute("Length", ov.lengthMm / 1000.0f);
    if (ov.hasWidthMm)
      model->SetAttribute("Width", ov.widthMm / 1000.0f);
    if (ov.hasHeightMm)
      model->SetAttribute("Height", ov.heightMm / 1000.0f);
  }

  doc.SaveFile(descPath.c_str());
  std::string outPath = tempDir + ".gdtf";
  if (!ZipDir(tempDir, outPath))
    return {};
  return outPath;
}

bool MvrExporter::ExportToFile(const std::string &filePath) {
  const auto &scene = ConfigManager::Get().GetScene();
  const TrussGeometryAuthority trussGeometryAuthority = GetTrussGeometryAuthoritySetting();
  std::unordered_map<std::string, std::string> positions;
  std::unordered_map<std::string, std::string> legacyPositionIdToCanonical;
  std::unordered_set<std::string> usedPositionUuids;

  auto reserveCanonicalPositionUuid = [&](const std::string &candidate,
                                          const std::string &seedBase) {
    std::string out = CanonicalizeUuid(candidate);
    if (out.empty() || usedPositionUuids.contains(out)) {
      int suffix = 0;
      do {
        out = DeriveDeterministicUuid(seedBase + "#" + std::to_string(suffix++));
      } while (usedPositionUuids.contains(out));
    }
    usedPositionUuids.insert(out);
    return out;
  };

  for (const auto &[rawUuid, rawName] : scene.positions) {
    const std::string name = TrimAscii(rawName);
    const std::string canonical = CanonicalizeUuid(rawUuid);
    if (!canonical.empty()) {
      const std::string stable = reserveCanonicalPositionUuid(
          canonical, "mvr:position:canonical:" + canonical + ":" + name);
      positions[stable] = name;
      if (stable != rawUuid)
        legacyPositionIdToCanonical[rawUuid] = stable;
      continue;
    }

    const std::string seed = "mvr:position:legacy:" + rawUuid + ":" + name;
    const std::string generated = reserveCanonicalPositionUuid({}, seed);
    positions[generated] = name.empty() ? rawUuid : name;
    legacyPositionIdToCanonical[rawUuid] = generated;
    LogLegacyPositionUuidWarning(
        "MVR export converted legacy Position uuid '" + rawUuid +
        "' to canonical '" + generated + "' (name='" + positions[generated] + "')");
  }

  std::unordered_map<std::string, std::string> positionByName;
  for (const auto &[uuid, name] : positions) {
    if (!name.empty())
      positionByName[name] = uuid;
  }

  auto ensurePositionEntry = [&](const std::string &positionId,
                                 const std::string &nameHint) {
    auto legacyIt = legacyPositionIdToCanonical.find(positionId);
    if (legacyIt != legacyPositionIdToCanonical.end()) {
      auto existing = positions.find(legacyIt->second);
      if (existing != positions.end() && !nameHint.empty() && existing->second != nameHint)
        existing->second = nameHint;
      if (!nameHint.empty())
        positionByName[nameHint] = legacyIt->second;
      return;
    }

    const std::string canonicalId = CanonicalizeUuid(positionId);
    if (!canonicalId.empty()) {
      auto it = positions.find(canonicalId);
      if (it == positions.end()) {
        positions[canonicalId] = nameHint;
      } else if (!nameHint.empty() && it->second != nameHint) {
        // Refresh the stored name so Hang Position edits are preserved on export.
        it->second = nameHint;
      }
      if (!nameHint.empty())
        positionByName.try_emplace(nameHint, canonicalId);
      return;
    }

    if (nameHint.empty())
      return;

    auto byName = positionByName.find(nameHint);
    if (byName != positionByName.end())
      return;

    std::string newUuid = reserveCanonicalPositionUuid({}, "mvr:position:name:" + nameHint);
    positions[newUuid] = nameHint;
    positionByName[nameHint] = newUuid;
    if (!positionId.empty()) {
      LogLegacyPositionUuidWarning(
          "MVR export normalized legacy Position uuid '" + positionId + "' -> '" +
          newUuid + "' (name='" + nameHint + "')");
    }
  };

  for (const auto &[uid, fixture] : scene.fixtures)
    ensurePositionEntry(fixture.position, fixture.positionName);
  for (const auto &[uid, truss] : scene.trusses)
    ensurePositionEntry(truss.position, truss.positionName);
  for (const auto &[uid, support] : scene.supports)
    ensurePositionEntry(support.position, support.positionName);

  auto resolvePositionReference = [&](const std::string &positionId,
                                      const std::string &nameHint) -> std::string {
    auto legacyIt = legacyPositionIdToCanonical.find(positionId);
    if (legacyIt != legacyPositionIdToCanonical.end())
      return legacyIt->second;

    const std::string canonicalId = CanonicalizeUuid(positionId);
    if (!canonicalId.empty() && positions.contains(canonicalId))
      return canonicalId;

    if (!nameHint.empty()) {
      auto byName = positionByName.find(nameHint);
      if (byName != positionByName.end()) {
        if (!positionId.empty() && byName->second != positionId) {
          Logger::Instance().Log(
              Logger::Level::Info,
              wxString::Format(
                  "MVR export remapped non-canonical Position '%s' to '%s' by name '%s'",
                  positionId.c_str(), byName->second.c_str(), nameHint.c_str())
                  .ToStdString());
        }
        return byName->second;
      }
    }
    return {};
  };

  wxFileOutputStream output(filePath);
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);

  std::vector<ResourceEntry> resourceEntries;
  std::unordered_map<std::string, std::string> sourceToArchivePath;
  std::unordered_map<std::string, std::string> gdtfArchiveByObjectUuid;
  std::unordered_map<std::string, GdtfOverrides> gdtfOverrides;
  std::unordered_map<std::string, std::string> trussArchiveByTypeKey;
  std::unordered_map<std::string, std::string> trussInstanceToTypeKey;
  std::unordered_set<std::string> reservedArchivePaths;

  auto normalizeSourcePath = [&](const std::string &rawPath) {
    fs::path src = fs::path(rawPath);
    if (src.is_relative() && !scene.basePath.empty())
      src = fs::path(scene.basePath) / src;
    std::error_code ec;
    fs::path weak = fs::weakly_canonical(src, ec);
    return ec ? fs::absolute(src).generic_string() : weak.generic_string();
  };

  auto registerResource = [&](const std::string &rawSource,
                              const std::string &preferredArchivePath,
                              bool allowReuseBySource = true) -> std::string {
    if (rawSource.empty())
      return {};
    std::string normalizedSource = normalizeSourcePath(rawSource);
    auto srcIt = sourceToArchivePath.find(normalizedSource);
    if (allowReuseBySource && srcIt != sourceToArchivePath.end())
      return srcIt->second;

    std::string archivePath = EnsureUniqueArchivePath(preferredArchivePath, reservedArchivePaths);
    sourceToArchivePath[normalizedSource] = archivePath;
    resourceEntries.push_back({fs::path(normalizedSource), archivePath});
    return archivePath;
  };

  auto registerGdtfResource = [&](const std::string &objectUuid,
                                  const std::string &rawGdtfPath,
                                  const std::string &preferredName,
                                  bool allowReuseBySource = true) -> std::string {
    if (rawGdtfPath.empty())
      return {};

    std::string fileName = preferredName;
    if (fileName.empty())
      fileName = SanitizeArchiveFileName(rawGdtfPath, "fixture.gdtf");
    std::string archivePath =
        registerResource(rawGdtfPath, fileName, allowReuseBySource);
    if (!objectUuid.empty() && !archivePath.empty())
      gdtfArchiveByObjectUuid[objectUuid] = archivePath;
    return archivePath;
  };

  auto registerModelTextureDependencies = [&](const std::string &rawModelSource) {
    if (rawModelSource.empty())
      return;
    const std::string normalizedModelPath = normalizeSourcePath(rawModelSource);
    const fs::path modelPath(normalizedModelPath);
    std::string ext = ToLowerAscii(modelPath.extension().string());
    if (ext != ".3ds")
      return;

    const std::vector<std::string> textureRefs =
        Collect3dsTextureReferences(modelPath);
    for (const std::string &textureRef : textureRefs) {
      fs::path texturePath;
      if (!ResolveTextureDependencyPath(modelPath, textureRef, texturePath))
        continue;

      std::string preferredTextureName =
          SanitizeArchiveFileName(textureRef, texturePath.filename().generic_string());
      registerResource(texturePath.generic_string(), preferredTextureName);
    }
  };

  auto registerModelResource = [&](const std::string &rawModelSource,
                                   const std::string &fallbackArchiveName) -> std::string {
    std::string archivePath = registerResource(
        rawModelSource, SanitizeArchiveFileName(rawModelSource, fallbackArchiveName));
    registerModelTextureDependencies(rawModelSource);
    return archivePath;
  };

  auto assignIds = [&]() {
    int nextNumericId = 1;
    std::unordered_set<int> usedIds;

    auto reserveId = [&](int candidate) {
      if (candidate > 0)
        usedIds.insert(candidate);
    };
    for (const auto &[uid, f] : scene.fixtures) {
      int existing = f.fixtureIdNumeric > 0 ? f.fixtureIdNumeric : f.fixtureId;
      reserveId(existing);
    }

    auto allocId = [&]() {
      while (usedIds.contains(nextNumericId))
        ++nextNumericId;
      usedIds.insert(nextNumericId);
      return nextNumericId++;
    };

    std::unordered_map<std::string, std::pair<std::string, int>> result;
    for (const auto &[uid, f] : scene.fixtures) {
      int numeric = f.fixtureIdNumeric > 0 ? f.fixtureIdNumeric : f.fixtureId;
      if (numeric <= 0) {
        numeric = allocId();
      }
      std::string stringId = TrimAscii(f.fixtureIdText);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }

    for (const auto &[uid, t] : scene.trusses) {
      int numeric = allocId();
      std::string stringId = TrimAscii(t.name);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }

    for (const auto &[uid, s] : scene.supports) {
      int numeric = allocId();
      std::string stringId = TrimAscii(s.name);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }
    return result;
  };

  const auto assignedIds = assignIds();

  tinyxml2::XMLDocument doc;
  doc.InsertEndChild(doc.NewDeclaration("xml version=\"1.0\" encoding=\"UTF-8\""));

  tinyxml2::XMLElement *root = doc.NewElement("GeneralSceneDescription");
  root->SetAttribute("verMajor", 1);
  root->SetAttribute("verMinor", 6);
  root->SetAttribute("provider", scene.provider.empty() ? kMvrProvider : scene.provider.c_str());
  root->SetAttribute("providerVersion",
                     scene.providerVersion.empty() ? kMvrProviderVersion
                                                   : scene.providerVersion.c_str());
  doc.InsertEndChild(root);

  tinyxml2::XMLElement *sceneNode = doc.NewElement("Scene");
  root->InsertEndChild(sceneNode);

  // ---- AUXData ----
  tinyxml2::XMLElement *aux = doc.NewElement("AUXData");
  for (const auto &[uuid, name] : positions) {
    tinyxml2::XMLElement *pos = doc.NewElement("Position");
    pos->SetAttribute("uuid", uuid.c_str());
    if (!name.empty())
      pos->SetAttribute("name", name.c_str());
    aux->InsertEndChild(pos);
  }
  for (const auto &[uuid, file] : scene.symdefFiles) {
    tinyxml2::XMLElement *sym = doc.NewElement("Symdef");
    sym->SetAttribute("uuid", uuid.c_str());

    auto tit = scene.symdefTypes.find(uuid);
    if (tit != scene.symdefTypes.end() && !tit->second.empty())
      sym->SetAttribute("geometryType", tit->second.c_str());

    std::vector<SymdefGeometry> geometries;
    auto geoIt = scene.symdefGeometries.find(uuid);
    if (geoIt != scene.symdefGeometries.end())
      geometries = geoIt->second;

    if (geometries.empty() && !file.empty()) {
      SymdefGeometry fallback;
      fallback.file = file;
      auto matIt = scene.symdefMatrices.find(uuid);
      fallback.transform =
          (matIt != scene.symdefMatrices.end()) ? matIt->second : MatrixUtils::Identity();
      if (tit != scene.symdefTypes.end())
        fallback.geometryType = tit->second;
      geometries.push_back(std::move(fallback));
    }

    if (!geometries.empty()) {
      tinyxml2::XMLElement *cl = doc.NewElement("ChildList");
      for (const SymdefGeometry &geo : geometries) {
        if (geo.file.empty())
          continue;

        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        std::string archivePath = registerModelResource(geo.file, "symbol.3ds");
        g3d->SetAttribute("fileName", archivePath.c_str());
        if (!geo.geometryType.empty())
          g3d->SetAttribute("geometryType", geo.geometryType.c_str());

        std::string matrixText = MatrixUtils::FormatMatrix(geo.transform);
        tinyxml2::XMLElement *matrix = doc.NewElement("Matrix");
        matrix->SetText(matrixText.c_str());
        g3d->InsertEndChild(matrix);

        cl->InsertEndChild(g3d);
      }

      if (cl->FirstChild())
        sym->InsertEndChild(cl);
    }

    aux->InsertEndChild(sym);
  }
  if (aux->FirstChild())
    sceneNode->InsertEndChild(aux);

  // ---- Layers ----
  tinyxml2::XMLElement *layersNode = doc.NewElement("Layers");

  std::unordered_set<std::string> usedFixtureUuids;

  auto exportFixture = [&](tinyxml2::XMLElement *parent, const Fixture &f) {
    tinyxml2::XMLElement *fe = doc.NewElement("Fixture");

    std::string stableUuid = CanonicalizeUuid(f.uuid);
    const std::string seed = "mvr-export-fixture:" + f.uuid + ":" +
                             f.instanceName + ":" +
                             MatrixUtils::FormatMatrix(f.transform);
    if (stableUuid.empty()) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format(
              "Fixture '%s' has non-canonical UUID '%s'. Applying deterministic fallback UUID for export.",
              f.instanceName.c_str(), f.uuid.c_str())
              .ToStdString());
      stableUuid = DeriveDeterministicUuid(seed);
    }
    if (usedFixtureUuids.contains(stableUuid)) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format(
              "Fixture UUID collision detected during export for '%s' (uuid=%s). Applying controlled fallback UUID.",
              f.instanceName.c_str(), stableUuid.c_str())
              .ToStdString());
      int suffix = 1;
      std::string candidate;
      do {
        candidate =
            DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
      } while (usedFixtureUuids.contains(candidate));
      stableUuid = std::move(candidate);
    }
    usedFixtureUuids.insert(stableUuid);

    fe->SetAttribute("uuid", stableUuid.c_str());
    const std::string fixtureExportName =
        TrimAscii(f.instanceName).empty() ? "Fixture" : TrimAscii(f.instanceName);
    fe->SetAttribute("name", fixtureExportName.c_str());

    auto addInt = [&](const char *n, int v) {
      if (v != 0) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(std::to_string(v).c_str());
        fe->InsertEndChild(e);
      }
    };
    auto addStr = [&](const char *n, const std::string &s) {
      if (!s.empty()) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(s.c_str());
        fe->InsertEndChild(e);
      }
    };
    auto addNum = [&](const char *n, float v, const char *unit) {
      if (v != 0.0f) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetAttribute("unit", unit);
        e->SetText(v);
        fe->InsertEndChild(e);
      }
    };

    auto idIt = assignedIds.find(f.uuid);
    int fixtureNumericId = f.fixtureIdNumeric > 0 ? f.fixtureIdNumeric : f.fixtureId;
    if (fixtureNumericId <= 0 && idIt != assignedIds.end())
      fixtureNumericId = idIt->second.second;
    if (fixtureNumericId <= 0)
      fixtureNumericId = 1;
    std::string fixtureId = TrimAscii(f.fixtureIdText);
    if (fixtureId.empty())
      fixtureId = std::to_string(fixtureNumericId);
    addStr("FixtureID", fixtureId);
    addInt("FixtureIDNumeric", fixtureNumericId);
    if (f.unitNumber != 0 && f.unitNumber != fixtureNumericId)
      addInt("UnitNumber", f.unitNumber);
    addInt("CustomId", f.customId);
    addInt("CustomIdType", f.customIdType);
    std::string fixtureSourceGdtf = f.gdtfSpec;
    if (fixtureSourceGdtf.empty()) {
      fixtureSourceGdtf = ResolveFallbackFixtureGdtfPath();
      if (fixtureSourceGdtf.empty()) {
        Logger::Instance().Log(
            Logger::Level::Warn,
            wxString::Format(
                "Fixture '%s' (uuid=%s) has no GDTF and fallback '%s' is not available.",
                fixtureExportName.c_str(), f.uuid.c_str(), kFallbackFixtureGdtfFileName)
                .ToStdString());
      } else {
        Logger::Instance().Log(
            Logger::Level::Info,
            wxString::Format(
                "Fixture '%s' (uuid=%s) has no GDTF. Using fallback '%s' for MVR export.",
                fixtureExportName.c_str(), f.uuid.c_str(), kFallbackFixtureGdtfFileName)
                .ToStdString());
      }
    }
    std::string fixtureName = SanitizeArchiveFileName(fixtureSourceGdtf, "fixture.gdtf");
    std::string fixtureGdtfArchivePath =
        registerGdtfResource(f.uuid, fixtureSourceGdtf, fixtureName);
    addStr("GDTFSpec", fixtureGdtfArchivePath);
    // Keep fixture GDTF payloads byte-preserved in exported MVR/project
    // packages. Fixture-specific metadata such as Color/Weight/Power is
    // already serialized at fixture level in GeneralSceneDescription.xml.
    // Repacking fixture GDTFs here can break model/texture references in some
    // vendor libraries after a save/reload cycle.
    addStr("GDTFMode", f.gdtfMode);
    addStr("Focus", f.focus);
    addStr("Function", f.function);
    if (!f.position.empty() || !f.positionName.empty())
      addStr("Position", resolvePositionReference(f.position, f.positionName));

    addNum("PowerConsumption", f.powerConsumptionW, "W");
    addNum("Weight", f.weightKg, "kg");

    if (!f.color.empty() && f.color.size() == 7 && f.color[0] == '#') {
      std::string cie = HexToCie(f.color);
      tinyxml2::XMLElement *col = doc.NewElement("Color");
      col->SetText(cie.c_str());
      fe->InsertEndChild(col);
    }

    if (f.dmxInvertPan) {
      tinyxml2::XMLElement *e = doc.NewElement("DMXInvertPan");
      e->SetText("true");
      fe->InsertEndChild(e);
    }
    if (f.dmxInvertTilt) {
      tinyxml2::XMLElement *e = doc.NewElement("DMXInvertTilt");
      e->SetText("true");
      fe->InsertEndChild(e);
    }

    if (!f.address.empty()) {
      const std::string trimmedAddress = TrimAscii(f.address);
      auto [universe, channel] = ParseAddress(trimmedAddress);
      int absoluteAddress = 0;
      if (TryComputeAbsoluteDmx(universe, channel, absoluteAddress)) {
        tinyxml2::XMLElement *addresses = doc.NewElement("Addresses");
        tinyxml2::XMLElement *addr = doc.NewElement("Address");
        addr->SetAttribute("break", 0);
        addr->SetText(std::to_string(absoluteAddress).c_str());
        addresses->InsertEndChild(addr);
        fe->InsertEndChild(addresses);
      } else {
        Logger::Instance().Log(
            Logger::Level::Warn,
            wxString::Format(
                "Skipping invalid DMX patch for fixture '%s' (uuid=%s): '%s' (expected Universe.Address with universe >= 1 and address in [1,512])",
                f.instanceName.c_str(), f.uuid.c_str(), trimmedAddress.c_str())
                .ToStdString());
      }
    }

    std::string mstr = MatrixUtils::FormatMatrix(f.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    fe->InsertEndChild(mat);

    tinyxml2::XMLElement *ud = doc.NewElement("UserData");
    tinyxml2::XMLElement *data = doc.NewElement("Data");
    data->SetAttribute("provider", "Perastage");
    data->SetAttribute("ver", "1.0");
    tinyxml2::XMLElement *info = doc.NewElement("FixtureInfo");
    info->SetAttribute("uuid", stableUuid.c_str());

    tinyxml2::XMLElement *stableIdNode = doc.NewElement("StableId");
    stableIdNode->SetText(stableUuid.c_str());
    info->InsertEndChild(stableIdNode);

    tinyxml2::XMLElement *scriptNode = doc.NewElement("Script");
    scriptNode->SetText(fixtureExportName.c_str());
    info->InsertEndChild(scriptNode);

    if (!f.instanceName.empty()) {
      tinyxml2::XMLElement *instanceName = doc.NewElement("InstanceName");
      instanceName->SetText(f.instanceName.c_str());
      info->InsertEndChild(instanceName);
    }

    if (!f.category.empty()) {
      tinyxml2::XMLElement *category = doc.NewElement("Category");
      category->SetText(f.category.c_str());
      info->InsertEndChild(category);
    }
    if (!f.categorySource.empty()) {
      tinyxml2::XMLElement *categorySource = doc.NewElement("CategorySource");
      categorySource->SetText(f.categorySource.c_str());
      info->InsertEndChild(categorySource);
    }
    if (!f.categorySourceReason.empty()) {
      tinyxml2::XMLElement *categoryReason = doc.NewElement("CategoryReason");
      categoryReason->SetText(f.categorySourceReason.c_str());
      info->InsertEndChild(categoryReason);
    }

    data->InsertEndChild(info);
    ud->InsertEndChild(data);
    fe->InsertEndChild(ud);

    parent->InsertEndChild(fe);
  };

  auto exportTruss = [&](tinyxml2::XMLElement *parent, const Truss &t) {
    tinyxml2::XMLElement *te = doc.NewElement("Truss");
    te->SetAttribute("uuid", t.uuid.c_str());
    if (!t.name.empty())
      te->SetAttribute("name", t.name.c_str());

    auto addInt = [&](const char *n, int v) {
      if (v != 0) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(std::to_string(v).c_str());
        te->InsertEndChild(e);
      }
    };
    auto addStr = [&](const char *n, const std::string &v) {
      if (!v.empty()) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(v.c_str());
        te->InsertEndChild(e);
      }
    };
    auto idIt = assignedIds.find(t.uuid);
    int fixtureNumericId = (idIt != assignedIds.end()) ? idIt->second.second : 0;
    if (fixtureNumericId <= 0)
      fixtureNumericId = 1;
    std::string fixtureId = std::to_string(fixtureNumericId);
    addStr("FixtureID", fixtureId);
    addInt("FixtureIDNumeric", fixtureNumericId);
    addInt("UnitNumber", t.unitNumber);
    addInt("CustomId", t.customId);
    addInt("CustomIdType", t.customIdType);

    std::string trussTypeKey = BuildTrussTypeKey(t);
    std::string trussGdtfArchivePath;
    auto trussArchiveIt = trussArchiveByTypeKey.find(trussTypeKey);
    if (trussArchiveIt != trussArchiveByTypeKey.end()) {
      trussGdtfArchivePath = trussArchiveIt->second;
      if (!t.uuid.empty())
        gdtfArchiveByObjectUuid[t.uuid] = trussGdtfArchivePath;
    } else {
      std::string trussSourceGdtf = t.gdtfSpec;
      if (trussSourceGdtf.empty() && fs::path(t.modelFile).extension() == ".gdtf")
        trussSourceGdtf = t.modelFile;

      if (trussSourceGdtf.empty()) {
        fs::path tempPath = fs::temp_directory_path() /
                            ("perastage-truss-export-" + (t.uuid.empty() ? std::string("truss") : t.uuid) + ".gdtf");
        std::string conversionError;
        if (BuildTrussGdtfFromInstance(t, tempPath, &conversionError))
          trussSourceGdtf = tempPath.string();
      }

      std::string trussPreferredName = "Perastage/truss_types/" + BuildTrussGdtfArchiveName(t);
      trussGdtfArchivePath =
          registerGdtfResource(t.uuid, trussSourceGdtf, trussPreferredName, true);
      if (!trussGdtfArchivePath.empty())
        trussArchiveByTypeKey[trussTypeKey] = trussGdtfArchivePath;
    }
    if (!trussTypeKey.empty())
      trussInstanceToTypeKey[t.uuid] = trussTypeKey;

    if (!trussGdtfArchivePath.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFSpec");
      e->SetText(trussGdtfArchivePath.c_str());
      te->InsertEndChild(e);

      tinyxml2::XMLElement *modeElement = doc.NewElement("GDTFMode");
      modeElement->SetText(t.gdtfMode.empty() ? "Default" : t.gdtfMode.c_str());
      te->InsertEndChild(modeElement);

      auto &ov = gdtfOverrides[trussGdtfArchivePath];
      ov.hasLengthMm = true;
      ov.lengthMm = t.lengthMm;
      ov.hasWidthMm = true;
      ov.widthMm = t.widthMm;
      ov.hasHeightMm = true;
      ov.heightMm = t.heightMm;
      ov.hasWeightKg = true;
      ov.weightKg = t.weightKg;
      ov.manufacturer = t.manufacturer;
      ov.model = t.model;
    }
    if (!t.function.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("Function");
      e->SetText(t.function.c_str());
      te->InsertEndChild(e);
    }
    {
      const std::string positionRef =
          resolvePositionReference(t.position, t.positionName);
      if (!positionRef.empty()) {
        tinyxml2::XMLElement *e = doc.NewElement("Position");
        e->SetText(positionRef.c_str());
        te->InsertEndChild(e);
      }
    }

    if (trussGeometryAuthority == TrussGeometryAuthority::MvrGeometry) {
      if (t.sourceRepresentation == Truss::GeometryRepresentation::SymbolSymdef &&
          !t.sourceSymdefUuid.empty()) {
        tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
        tinyxml2::XMLElement *sym = doc.NewElement("Symbol");
        sym->SetAttribute("symdef", t.sourceSymdefUuid.c_str());
        tinyxml2::XMLElement *symMat = doc.NewElement("Matrix");
        symMat->SetText(MatrixUtils::FormatMatrix(t.sourceSymbolMatrix).c_str());
        sym->InsertEndChild(symMat);
        geos->InsertEndChild(sym);
        te->InsertEndChild(geos);
        Logger::Instance().Log(
            Logger::Level::Info,
            wxString::Format("MVR export truss keeps Symbol/Symdef uuid=%s symdef=%s",
                             t.uuid.c_str(), t.sourceSymdefUuid.c_str())
                .ToStdString());
      } else if (!t.symbolFile.empty()) {
        std::string ext = fs::path(t.symbolFile).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".3ds" || ext == ".glb") {
          tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
          tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
          std::string symbolArchivePath =
              registerModelResource(t.symbolFile, "truss.3ds");
          g3d->SetAttribute("fileName", symbolArchivePath.c_str());
          if (!t.sourceGeometryType.empty())
            g3d->SetAttribute("geometryType", t.sourceGeometryType.c_str());
          tinyxml2::XMLElement *geoMat = doc.NewElement("Matrix");
          geoMat->SetText(MatrixUtils::FormatMatrix(t.sourceGeometryMatrix).c_str());
          g3d->InsertEndChild(geoMat);
          geos->InsertEndChild(g3d);
          te->InsertEndChild(geos);
          Logger::Instance().Log(
              Logger::Level::Info,
              wxString::Format("MVR export truss uses direct Geometry3D uuid=%s",
                               t.uuid.c_str())
                  .ToStdString());
        }
      }
    }
    const Matrix matrixToWrite = t.hasLocalTransform ? t.localTransform : t.transform;
    std::string mstr = MatrixUtils::FormatMatrix(matrixToWrite);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    te->InsertEndChild(mat);

    bool hasMeta =
                   (!t.manufacturer.empty() || !t.model.empty() ||
                    t.lengthMm != 0.0f || t.widthMm != 0.0f ||
                    t.heightMm != 0.0f || t.weightKg != 0.0f ||
                    !t.crossSection.empty() || !t.modelFile.empty() ||
                    !t.positionName.empty());
    if (hasMeta) {
      tinyxml2::XMLElement *ud = doc.NewElement("UserData");
      tinyxml2::XMLElement *data = doc.NewElement("Data");
      data->SetAttribute("provider", "Perastage");
      data->SetAttribute("ver", "1.0");
      tinyxml2::XMLElement *info = doc.NewElement("TrussInfo");
      info->SetAttribute("uuid", t.uuid.c_str());
      auto addTxt = [&](const char *n, const std::string &v) {
        if (!v.empty()) {
          tinyxml2::XMLElement *e = doc.NewElement(n);
          e->SetText(v.c_str());
          info->InsertEndChild(e);
        }
      };
      auto addNum = [&](const char *n, float v, const char *unit) {
        if (v != 0.0f) {
          tinyxml2::XMLElement *e = doc.NewElement(n);
          e->SetAttribute("unit", unit);
          e->SetText(std::to_string(v).c_str());
          info->InsertEndChild(e);
        }
      };
      addTxt("Manufacturer", t.manufacturer);
      addTxt("Model", t.model);
      addNum("Length", t.lengthMm, "mm");
      addNum("Width", t.widthMm, "mm");
      addNum("Height", t.heightMm, "mm");
      addNum("Weight", t.weightKg, "kg");
      addTxt("CrossSection", t.crossSection);
      addTxt("ModelFile", t.modelFile);
      addTxt("HangPos", t.positionName);
      addTxt("Representation", ToRepresentationText(t.sourceRepresentation));
      addTxt("TypeKey", trussTypeKey);
      addTxt("AuxGdtf", trussGdtfArchivePath);
      data->InsertEndChild(info);
      ud->InsertEndChild(data);
      te->InsertEndChild(ud);
    }

    parent->InsertEndChild(te);
  };

  auto exportSupport = [&](tinyxml2::XMLElement *parent, const Support &s) {
    Logger::Instance().Log(
        Logger::Level::Info,
        wxString::Format(
            "MVR export support uuid=%s uses SceneObject fallback to keep XML MVR 1.6-compliant",
            s.uuid.c_str())
            .ToStdString());
    tinyxml2::XMLElement *se = doc.NewElement("SceneObject");
    se->SetAttribute("uuid", s.uuid.c_str());
    se->SetAttribute("geometryType", "support");
    if (!s.name.empty())
      se->SetAttribute("name", s.name.c_str());

    auto addSupportInfo = [&](const char *n, const std::string &v,
                              tinyxml2::XMLElement *info) {
      if (!v.empty()) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(v.c_str());
        info->InsertEndChild(e);
      }
    };
    auto addSupportNum = [&](const char *n, float v,
                             tinyxml2::XMLElement *info) {
      if (v > 0.0f) {
        tinyxml2::XMLElement *e = doc.NewElement(n);
        e->SetText(std::to_string(v).c_str());
        info->InsertEndChild(e);
      }
    };

    tinyxml2::XMLElement *data = FindOrCreatePerastageDataNode(doc, se);
    tinyxml2::XMLElement *info = doc.NewElement("SupportInfo");
    info->SetAttribute("uuid", s.uuid.c_str());
    std::string supportGdtfArchivePath = registerGdtfResource(s.uuid, s.gdtfSpec, "");
    addSupportInfo("GDTFSpec", supportGdtfArchivePath, info);
    addSupportInfo("GDTFMode", s.gdtfMode, info);
    addSupportInfo("Function", s.function, info);
    addSupportInfo("HoistFunction", s.hoistFunction, info);
    addSupportNum("ChainLength", s.chainLength, info);
    addSupportInfo("Position", resolvePositionReference(s.position, s.positionName), info);
    addSupportInfo("PositionName", s.positionName, info);
    if (info->FirstChild())
      data->InsertEndChild(info);

    std::string mstr = MatrixUtils::FormatMatrix(s.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    se->InsertEndChild(mat);

    if (ShouldExportSupportHoistInfo(s))
      AppendSupportHoistInfoUserData(doc, data, s);

    parent->InsertEndChild(se);
  };

  auto exportSceneObject = [&](tinyxml2::XMLElement *parent,
                               const SceneObject &obj) {
    tinyxml2::XMLElement *oe = doc.NewElement("SceneObject");
    oe->SetAttribute("uuid", obj.uuid.c_str());
    if (!obj.name.empty())
      oe->SetAttribute("name", obj.name.c_str());

    if (!obj.geometries.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      for (const auto &geo : obj.geometries) {
        if (geo.modelFile.empty())
          continue;

        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        std::string modelArchivePath =
            registerModelResource(geo.modelFile, "object.3ds");
        g3d->SetAttribute("fileName", modelArchivePath.c_str());

        std::string geoMatrixText = MatrixUtils::FormatMatrix(geo.localTransform);
        tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
        geoMatrix->SetText(geoMatrixText.c_str());
        g3d->InsertEndChild(geoMatrix);

        geos->InsertEndChild(g3d);
      }

      if (geos->FirstChild())
        oe->InsertEndChild(geos);
    } else if (!obj.modelFile.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
      std::string modelArchivePath =
          registerModelResource(obj.modelFile, "object.3ds");
      g3d->SetAttribute("fileName", modelArchivePath.c_str());
      oe->InsertEndChild(geos);
      geos->InsertEndChild(g3d);
    }

    std::string mstr = MatrixUtils::FormatMatrix(obj.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    oe->InsertEndChild(mat);

    parent->InsertEndChild(oe);
  };

  auto exportGroupObject = [&](auto &&self, tinyxml2::XMLElement *parent,
                               const GroupObject &group) -> void {
    tinyxml2::XMLElement *go = doc.NewElement("GroupObject");
    go->SetAttribute("uuid", group.uuid.c_str());
    if (!group.name.empty())
      go->SetAttribute("name", group.name.c_str());
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(MatrixUtils::FormatMatrix(group.localTransform).c_str());
    go->InsertEndChild(mat);

    tinyxml2::XMLElement *childList = doc.NewElement("ChildList");
    for (const auto &childRef : group.children) {
      if (childRef.type == MvrNodeType::Truss) {
        auto it = scene.trusses.find(childRef.uuid);
        if (it != scene.trusses.end())
          exportTruss(childList, it->second);
      } else if (childRef.type == MvrNodeType::SceneObject) {
        auto it = scene.sceneObjects.find(childRef.uuid);
        if (it != scene.sceneObjects.end())
          exportSceneObject(childList, it->second);
      } else if (childRef.type == MvrNodeType::Fixture) {
        auto it = scene.fixtures.find(childRef.uuid);
        if (it != scene.fixtures.end())
          exportFixture(childList, it->second);
      } else if (childRef.type == MvrNodeType::Support) {
        auto it = scene.supports.find(childRef.uuid);
        if (it != scene.supports.end())
          exportSupport(childList, it->second);
      } else if (childRef.type == MvrNodeType::GroupObject) {
        auto it = scene.groupObjects.find(childRef.uuid);
        if (it != scene.groupObjects.end())
          self(self, childList, it->second);
      }
    }

    if (childList->FirstChild())
      go->InsertEndChild(childList);
    parent->InsertEndChild(go);
    Logger::Instance().Log(
        Logger::Level::Info,
        wxString::Format("MVR export preserved GroupObject uuid=%s",
                         group.uuid.c_str())
            .ToStdString());
  };

  for (const auto &[layerUuid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME)
      continue;
    tinyxml2::XMLElement *layerElem = doc.NewElement("Layer");
    if (!layerUuid.empty())
      layerElem->SetAttribute("uuid", layerUuid.c_str());
    if (!layer.name.empty())
      layerElem->SetAttribute("name", layer.name.c_str());

    if (!layer.color.empty() && layer.color.size() == 7 &&
        layer.color[0] == '#') {
      std::string cie = HexToCie(layer.color);
      tinyxml2::XMLElement *col = doc.NewElement("Color");
      col->SetText(cie.c_str());
      layerElem->InsertEndChild(col);
    }

    tinyxml2::XMLElement *childList = doc.NewElement("ChildList");

    for (const auto &[uid, group] : scene.groupObjects) {
      if (group.layer != layer.name || !group.parentGroupUuid.empty())
        continue;
      exportGroupObject(exportGroupObject, childList, group);
    }

    for (const auto &[uid, f] : scene.fixtures) {
      if (f.layer != layer.name)
        continue;
      bool grouped = false;
      for (const auto &[gid, g] : scene.groupObjects) {
        if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
              return r.type == MvrNodeType::Fixture && r.uuid == uid;
            })) {
          grouped = true;
          break;
        }
      }
      if (grouped)
        continue;
      exportFixture(childList, f);
    }

    for (const auto &[uid, t] : scene.trusses) {
      if (t.layer != layer.name)
        continue;
      if (!t.parentGroupUuid.empty())
        continue;
      exportTruss(childList, t);
    }

    for (const auto &[uid, s] : scene.supports) {
      if (s.layer != layer.name)
        continue;
      bool grouped = false;
      for (const auto &[gid, g] : scene.groupObjects) {
        if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
              return r.type == MvrNodeType::Support && r.uuid == uid;
            })) {
          grouped = true;
          break;
        }
      }
      if (grouped)
        continue;
      exportSupport(childList, s);
    }

    for (const auto &[uid, obj] : scene.sceneObjects) {
      if (obj.layer != layer.name)
        continue;
      bool grouped = false;
      for (const auto &[gid, g] : scene.groupObjects) {
        if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
              return r.type == MvrNodeType::SceneObject && r.uuid == uid;
            })) {
          grouped = true;
          break;
        }
      }
      if (grouped)
        continue;
      exportSceneObject(childList, obj);
    }

    if (childList->FirstChild())
      layerElem->InsertEndChild(childList);

    layersNode->InsertEndChild(layerElem);
  }

  // Objects with no layer
  tinyxml2::XMLElement *rootChildList = doc.NewElement("ChildList");
  for (const auto &[uid, group] : scene.groupObjects) {
    if (!(group.layer == DEFAULT_LAYER_NAME || group.layer.empty()) || !group.parentGroupUuid.empty())
      continue;
    exportGroupObject(exportGroupObject, rootChildList, group);
  }
  for (const auto &[uid, f] : scene.fixtures) {
    if (!(f.layer == DEFAULT_LAYER_NAME || f.layer.empty()))
      continue;
    bool grouped = false;
    for (const auto &[gid, g] : scene.groupObjects) {
      if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
            return r.type == MvrNodeType::Fixture && r.uuid == uid;
          })) {
        grouped = true;
        break;
      }
    }
    if (!grouped)
      exportFixture(rootChildList, f);
  }
  for (const auto &[uid, t] : scene.trusses) {
    if ((t.layer == DEFAULT_LAYER_NAME || t.layer.empty()) && t.parentGroupUuid.empty())
      exportTruss(rootChildList, t);
  }
  for (const auto &[uid, s] : scene.supports) {
    if (!(s.layer == DEFAULT_LAYER_NAME || s.layer.empty()))
      continue;
    bool grouped = false;
    for (const auto &[gid, g] : scene.groupObjects) {
      if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
            return r.type == MvrNodeType::Support && r.uuid == uid;
          })) {
        grouped = true;
        break;
      }
    }
    if (!grouped)
      exportSupport(rootChildList, s);
  }
  for (const auto &[uid, obj] : scene.sceneObjects) {
    if (!(obj.layer == DEFAULT_LAYER_NAME || obj.layer.empty()))
      continue;
    bool grouped = false;
    for (const auto &[gid, g] : scene.groupObjects) {
      if (std::any_of(g.children.begin(), g.children.end(), [&](const GroupObjectChildRef &r) {
            return r.type == MvrNodeType::SceneObject && r.uuid == uid;
          })) {
        grouped = true;
        break;
      }
    }
    if (!grouped)
      exportSceneObject(rootChildList, obj);
  }
  if (rootChildList->FirstChild())
    layersNode->InsertEndChild(rootChildList);

  sceneNode->InsertEndChild(layersNode);

  if (!trussArchiveByTypeKey.empty()) {
    tinyxml2::XMLElement *rootUserData = doc.NewElement("UserData");
    tinyxml2::XMLElement *data = doc.NewElement("Data");
    data->SetAttribute("provider", "Perastage");
    data->SetAttribute("ver", "1.0");
    tinyxml2::XMLElement *manifest = doc.NewElement("TrussSidecarManifest");
    for (const auto &[typeKey, archivePath] : trussArchiveByTypeKey) {
      tinyxml2::XMLElement *typeNode = doc.NewElement("Type");
      typeNode->SetAttribute("key", typeKey.c_str());
      typeNode->SetAttribute("gdtf", archivePath.c_str());
      manifest->InsertEndChild(typeNode);
      Logger::Instance().Log(
          Logger::Level::Info,
          wxString::Format("MVR export generated truss sidecar GDTF typeKey=%s archive=%s",
                           typeKey.c_str(), archivePath.c_str())
              .ToStdString());
    }
    for (const auto &[uuid, typeKey] : trussInstanceToTypeKey) {
      tinyxml2::XMLElement *instNode = doc.NewElement("Instance");
      instNode->SetAttribute("uuid", uuid.c_str());
      instNode->SetAttribute("typeKey", typeKey.c_str());
      manifest->InsertEndChild(instNode);
      Logger::Instance().Log(
          Logger::Level::Info,
          wxString::Format("MVR export linked truss instance to sidecar uuid=%s typeKey=%s",
                           uuid.c_str(), typeKey.c_str())
              .ToStdString());
    }
    data->InsertEndChild(manifest);
    rootUserData->InsertEndChild(data);
    root->InsertEndChild(rootUserData);
  }

  std::unordered_map<std::string, int> plannedArchiveEntries;
  plannedArchiveEntries["GeneralSceneDescription.xml"] = 1;

  for (auto &entry : resourceEntries) {
    if (!fs::exists(entry.sourcePath))
      continue;
    auto cit = gdtfOverrides.find(entry.archivePath);
    if (cit != gdtfOverrides.end()) {
      std::string tmp = CreatePatchedGdtf(entry.sourcePath.string(), cit->second);
      if (!tmp.empty())
        entry.sourcePath = fs::path(tmp);
    }
    ++plannedArchiveEntries[entry.archivePath];
  }

  if (!ValidateMvr16Export(doc, gdtfArchiveByObjectUuid, plannedArchiveEntries)) {
    zip.Close();
    return false;
  }

  // Serialize XML
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  std::string xmlData = printer.CStr();

  std::unordered_set<std::string> writtenArchiveEntries;
  {
    if (!writtenArchiveEntries.insert("GeneralSceneDescription.xml").second) {
      wxLogError("MVR export failed: duplicate ZIP entry GeneralSceneDescription.xml");
      zip.Close();
      return false;
    }
    auto *entry = new wxZipEntry("GeneralSceneDescription.xml");
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    zip.Write(xmlData.c_str(), xmlData.size());
    zip.CloseEntry();
  }

  for (const auto &resource : resourceEntries) {
    if (!fs::exists(resource.sourcePath) || resource.archivePath.empty())
      continue;
    if (!writtenArchiveEntries.insert(resource.archivePath).second) {
      wxLogError("MVR export failed: duplicate ZIP entry %s", resource.archivePath);
      zip.Close();
      return false;
    }
    auto *e = new wxZipEntry(resource.archivePath);
    e->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(e);
    std::ifstream in(resource.sourcePath, std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }

  zip.Close();
  return true;
}
