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
#include "app_version.h"
#include "configmanager.h"
#include "dummyprofilelibrary.h"
#include "filesystem_path_utils.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_canonicalizer.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "logger.h"
#include "matrixutils.h"
#include "mvr_preferences.h"
#include "primitive_model_resources.h"
#include "projectutils.h"
#include "support.h"
#include "truss_gdtf_builder.h"
#include "uuidutils.h"

#include <wx/wfstream.h>
#include <wx/wx.h>
class wxZipStreamLink;
#include <wx/filename.h>
#include <wx/zipstrm.h>

#include <tinyxml2.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
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

struct FixtureExportId {
  std::string text;
  int numeric = 0;
};

struct FixtureTypeInfoExport {
  std::string key;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string manufacturer;
  std::string model;
  std::string category;
  std::string categorySource;
  std::string visualColorHex;
};

static bool TryParseInt(std::string_view text, int &out);
static bool ParseMvrAddressNodeText(const std::string &text, int &universeOut,
                                    int &channelOut);
static bool TryComputeAbsoluteDmx(int universe1Based, int address1Based,
                                  int &absoluteOut);
static std::string TrimAscii(std::string value);
static std::string ToLowerAscii(std::string value);
static FixtureExportId ResolveFixtureExportId(const Fixture &fixture);
static std::unordered_map<std::string, int> BuildFixtureUnitNumbersForExport(
    const std::unordered_map<std::string, Fixture> &fixtures);
static std::string
BuildFixtureTypeInfoKey(const Fixture &fixture,
                                               const std::string &gdtfArchivePath);
static bool IsManualFixtureCategorySource(const std::string &source);
static void MergeFixtureTypeInfoExport(
    std::map<std::string, FixtureTypeInfoExport> &metadataByType,
    const Fixture &fixture, const std::string &gdtfArchivePath);
static void AppendFixtureTypeMetadata(
    tinyxml2::XMLDocument &doc, tinyxml2::XMLElement *perastageData,
    const std::map<std::string, FixtureTypeInfoExport> &metadataByType);

static bool ShouldExportSupportHoistInfo(const Support &support);
static bool NearlyEqualPhysicalValue(float lhs, float rhs);
static bool FixtureNeedsPhysicalGdtfPatch(const Fixture &fixture,
                                          const std::string &gdtfPath,
                                          GdtfOverrides &overrides);
static tinyxml2::XMLElement *
FindFirstPerastageUserData(tinyxml2::XMLElement *node);
static tinyxml2::XMLElement *
FindOrCreatePerastageDataNode(tinyxml2::XMLDocument &doc,
                                                            tinyxml2::XMLElement *node);

static void AppendSupportHoistInfoMetadata(tinyxml2::XMLDocument &doc,
                                           tinyxml2::XMLElement *hoistInfoMap,
                                           const Support &support);
static bool IsCanonicalUuidString(const std::string &value);
static std::string ExportLayerUuid(const std::string &layerUuid,
                                   const std::string &layerName);
static bool IsLayerColorMetadataValue(const std::string &color);
static bool HasLayerAppearanceMetadata(const MvrScene &scene);
static void AppendLayerAppearanceMetadata(tinyxml2::XMLDocument &doc,
                                          tinyxml2::XMLElement *perastageData,
                                          const MvrScene &scene);
static bool HasTrussInfoMetadata(const Truss &truss);
static void AppendTrussInfoMetadata(tinyxml2::XMLDocument &doc,
                                    tinyxml2::XMLElement *trussInfoMap,
                                    const Truss &truss,
                                    const std::string &exportUuid,
                                    const std::string &trussTypeKey,
                                    const std::string &auxGdtfArchivePath);
static void LogLegacyPositionUuidWarning(const std::string &message);

static constexpr const char *kMvrProvider = "Perastage";
static constexpr const char *kPerastageUserDataSchemaVersion = "1.0";
static constexpr const char *kDummyFallbackFixtureGdtfFileName =
    "Dummy 1ch.gdtf";
static constexpr const char *kPerastageNamedDummyFallbackFixtureGdtfFileName =
    "Perastage@Dummy_1ch@Perastage.gdtf";
static constexpr const char *kUnknownNamedDummyFallbackFixtureGdtfFileName =
    "Unknown@Dummy_1ch@Perastage.gdtf";
static constexpr const char *kLegacyFallbackFixtureGdtfFileName =
    "Generic 1ch.gdtf";
static constexpr const char *kPerastageNamedLegacyFallbackFixtureGdtfFileName =
    "Generic@Generic_1ch@Perastage.gdtf";
static constexpr const char *kPhysicalPropertiesRevisionText =
    "Updated physical properties for Perastage MVR export";

// Compares physical values using the exporter tolerance.
static bool NearlyEqualPhysicalValue(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= 0.001f;
}

// Decides whether fixture edits require a patched exported GDTF copy.
static bool FixtureNeedsPhysicalGdtfPatch(const Fixture &fixture,
                                          const std::string &gdtfPath,
                                          GdtfOverrides &overrides) {
  if (!fixture.physicalPropertiesDirty || gdtfPath.empty())
    return false;

  float gdtfWeightKg = 0.0f;
  float gdtfPowerW = 0.0f;
  const bool hasGdtfProperties =
      GetGdtfProperties(gdtfPath, gdtfWeightKg, gdtfPowerW);

  bool needsPatch = false;
  if (fixture.weightKg > 0.0f &&
      (!hasGdtfProperties ||
       !NearlyEqualPhysicalValue(fixture.weightKg, gdtfWeightKg))) {
    overrides.hasWeightKg = true;
    overrides.weightKg = fixture.weightKg;
    needsPatch = true;
  }
  if (fixture.powerConsumptionW > 0.0f &&
      (!hasGdtfProperties ||
       !NearlyEqualPhysicalValue(fixture.powerConsumptionW, gdtfPowerW))) {
    overrides.hasPowerW = true;
    overrides.powerW = fixture.powerConsumptionW;
    needsPatch = true;
  }
  return needsPatch;
}

// Resolves the MVR FixtureID values from the current editable fixture ID.
static FixtureExportId ResolveFixtureExportId(const Fixture &fixture) {
  FixtureExportId id;
  id.numeric =
      fixture.fixtureId > 0 ? fixture.fixtureId : fixture.fixtureIdNumeric;
  if (id.numeric <= 0)
    id.numeric = 0;

  const bool importedNumericStillMatches =
      fixture.fixtureIdNumeric > 0 &&
      fixture.fixtureId == fixture.fixtureIdNumeric;
  if (importedNumericStillMatches)
    id.text = TrimAscii(fixture.fixtureIdText);
  if (id.text.empty() && id.numeric > 0)
    id.text = std::to_string(id.numeric);
  return id;
}

// Builds a normalized fixture type key for UnitNumber export grouping.
static std::string BuildFixtureUnitNumberTypeKey(const Fixture &fixture) {
  auto normalize = [](std::string value) {
    value = TrimAscii(std::move(value));
    std::string normalized;
    bool pendingSpace = false;
    for (unsigned char ch : value) {
      if (std::isspace(ch)) {
        pendingSpace = !normalized.empty();
        continue;
      }
      if (pendingSpace) {
        normalized.push_back(' ');
        pendingSpace = false;
      }
      normalized.push_back(static_cast<char>(ch));
    }
    return normalized;
  };

  for (const std::string *candidate :
       {&fixture.typeName, &fixture.gdtfSpec, &fixture.requestedFixtureName,
                                       &fixture.instanceName}) {
    std::string key = normalize(*candidate);
    if (!key.empty())
      return key;
  }
  return "Unknown";
}

// Prepares deterministic UnitNumber values for fixtures without mutating the
// editable scene.
static std::unordered_map<std::string, int> BuildFixtureUnitNumbersForExport(
    const std::unordered_map<std::string, Fixture> &fixtures) {
  struct FixtureRef {
    std::string uuid;
    const Fixture *fixture = nullptr;
  };

  struct UnitGroup {
    std::set<int> used;
    std::vector<FixtureRef> missing;
  };

  std::unordered_map<std::string, int> result;
  std::unordered_map<std::string, UnitGroup> groups;
  for (const auto &[uuid, fixture] : fixtures) {
    const std::string typeKey = BuildFixtureUnitNumberTypeKey(fixture);
    UnitGroup &group = groups[typeKey];
    if (fixture.unitNumber > 0) {
      group.used.insert(fixture.unitNumber);
      result[uuid] = fixture.unitNumber;
    } else {
      group.missing.push_back({uuid, &fixture});
    }
  }

  constexpr float kPositionTieTolerance = 0.0001f;
  for (auto &[_, group] : groups) {
    std::sort(group.missing.begin(), group.missing.end(),
              [=](const FixtureRef &lhs, const FixtureRef &rhs) {
                const auto lhsPos = lhs.fixture->GetPosition();
                const auto rhsPos = rhs.fixture->GetPosition();
                if (std::fabs(lhsPos[1] - rhsPos[1]) > kPositionTieTolerance)
                  return lhsPos[1] < rhsPos[1];
                if (std::fabs(lhsPos[0] - rhsPos[0]) > kPositionTieTolerance)
                  return lhsPos[0] < rhsPos[0];

                const int lhsId = ResolveFixtureExportId(*lhs.fixture).numeric;
                const int rhsId = ResolveFixtureExportId(*rhs.fixture).numeric;
                if (lhsId != rhsId) {
                  if (lhsId <= 0)
                    return false;
                  if (rhsId <= 0)
                    return true;
                  return lhsId < rhsId;
                }
                if (lhs.fixture->instanceName != rhs.fixture->instanceName)
                  return lhs.fixture->instanceName < rhs.fixture->instanceName;
                return lhs.uuid < rhs.uuid;
              });

    int nextUnitNumber = 1;
    for (const FixtureRef &fixtureRef : group.missing) {
      while (group.used.contains(nextUnitNumber))
        ++nextUnitNumber;
      result[fixtureRef.uuid] = nextUnitNumber;
      group.used.insert(nextUnitNumber);
      ++nextUnitNumber;
    }
  }

  return result;
}

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

static std::vector<std::string>
Collect3dsTextureReferences(const fs::path &modelPath) {
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

static std::vector<std::string>
CollectGltfTextureReferences(const fs::path &modelPath) {
  std::vector<std::string> references;
  std::ifstream file(modelPath);
  if (!file.is_open())
    return references;

  std::ostringstream content;
  content << file.rdbuf();
  const std::string jsonText = content.str();
  if (jsonText.empty())
    return references;

  std::unordered_set<std::string> seenRefs;
  const std::regex uriRegex(R"re("uri"\s*:\s*"([^"]+)")re");
  for (std::sregex_iterator it(jsonText.begin(), jsonText.end(), uriRegex), end;
       it != end; ++it) {
    std::string uri = (*it)[1].str();
    if (uri.empty())
      continue;

    // Ignore embedded data URIs. We only need to collect external files.
    if (uri.rfind("data:", 0) == 0)
      continue;

    if (seenRefs.insert(ToLowerAscii(uri)).second)
      references.push_back(std::move(uri));
  }

  return references;
}

static bool ResolveTextureDependencyPath(const fs::path &modelPath,
                                         const std::string &textureRef,
                                         fs::path &resolvedPath) {
  const std::string normalizedRef = TrimAscii(textureRef);
  if (normalizedRef.empty())
    return false;

  const fs::path refPath = PathUtils::PathFromUtf8(normalizedRef);
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
  for (const auto &entry : fs::directory_iterator(
           modelDir, fs::directory_options::skip_permission_denied, ec)) {
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
  const float rawValue =
      ConfigManager::Get().GetFloat("mvr_truss_geometry_authority");
  return rawValue >= 0.5f ? TrussGeometryAuthority::Gdtf
                          : TrussGeometryAuthority::MvrGeometry;
}

static std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char c) { return std::isspace(c); };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(),
                                          [&](unsigned char c) { return !isSpace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](unsigned char c) { return !isSpace(c); })
                  .base(),
              value.end());
  return value;
}

static std::string ToLowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static void LogLegacyPositionUuidWarning(const std::string &message) {
  Logger::Instance().Log(Logger::Level::Warn, message);
}

static std::string
TruncateFileNamePreservingExtension(const std::string &fileName,
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

// Sanitizes arbitrary input into a single portable archive filename.
static std::string SanitizeArchiveFileName(const std::string &input,
                                           const std::string &fallbackName);

static std::string ResolveFallbackFixtureGdtfPath() {
  static const std::string resolvedPath = []() {
    const fs::path basePath = ProjectUtils::GetBaseLibraryPath("fixtures");
    const std::array<fs::path, 5> candidates = {
        basePath / kDummyFallbackFixtureGdtfFileName,
        basePath / kPerastageNamedDummyFallbackFixtureGdtfFileName,
        basePath / kUnknownNamedDummyFallbackFixtureGdtfFileName,
        basePath / kLegacyFallbackFixtureGdtfFileName,
        basePath / kPerastageNamedLegacyFallbackFixtureGdtfFileName,
    };
    for (const fs::path &fallbackPath : candidates) {
      std::error_code ec;
      if (fs::exists(fallbackPath, ec) && !ec &&
          fs::is_regular_file(fallbackPath, ec) && !ec) {
        return fallbackPath.generic_string();
      }
    }
    return std::string{};
  }();
  return resolvedPath;
}

// Returns true when an archive filename is already reserved case-insensitively.
static bool ContainsArchiveFileNameCaseInsensitive(
    const std::unordered_set<std::string> &usedPaths,
    const std::string &candidate) {
  const std::string candidateKey = ToLowerAscii(candidate);
  return std::any_of(usedPaths.begin(), usedPaths.end(),
                     [&](const std::string &used) {
                       return ToLowerAscii(used) == candidateKey;
                     });
}

// Creates a unique root-level MVR archive filename from any source-like path.
static std::string
EnsureUniqueArchivePath(const std::string &proposed,
                                           std::unordered_set<std::string> &usedPaths) {
  constexpr size_t kMaxArchiveEntryNameLength = 120;
  std::string normalized = SanitizeArchiveFileName(proposed, "resource.bin");
  normalized = TruncateFileNamePreservingExtension(normalized,
                                                   kMaxArchiveEntryNameLength);
  if (normalized.empty() ||
      fs::path(normalized).stem().generic_string().empty())
    normalized = "resource.bin";
  if (!ContainsArchiveFileNameCaseInsensitive(usedPaths, normalized)) {
    usedPaths.insert(normalized);
    return normalized;
  }

  fs::path stemPath = fs::path(normalized);
  std::string ext = stemPath.extension().generic_string();
  std::string stem = stemPath.stem().generic_string();
  if (stem.empty())
    stem = "resource";
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
    if (adjustedStem.empty())
      adjustedStem = "resource";
    std::string candidate = adjustedStem + suffix + ext;
    if (!ContainsArchiveFileNameCaseInsensitive(usedPaths, candidate)) {
      usedPaths.insert(candidate);
      return candidate;
    }
    ++index;
  }
}

// Sanitizes arbitrary input into a single portable archive filename.
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
        sanitizeSingleFileName(fileName, fallbackName),
        kMaxArchiveFileNameLength);
  return TruncateFileNamePreservingExtension(
      sanitizeSingleFileName("", fallbackName), kMaxArchiveFileNameLength);
}

// Sanitizes arbitrary input into a relative archive path for legacy helpers.
static std::string
SanitizeArchiveRelativePath(const std::string &input,
                                               const std::string &fallbackName) {
  std::string candidate = TrimAscii(input);
  std::replace(candidate.begin(), candidate.end(), '\\', '/');
  if (candidate.empty())
    return SanitizeArchiveFileName(candidate, fallbackName);

  fs::path raw = PathUtils::PathFromUtf8(candidate);
  std::vector<std::string> sanitizedParts;
  for (const auto &part : raw) {
    const std::string segment = TrimAscii(part.generic_string());
    if (segment.empty() || segment == "." || segment == "..")
      continue;
    sanitizedParts.push_back(SanitizeArchiveFileName(segment, "resource.bin"));
  }

  if (sanitizedParts.empty())
    return SanitizeArchiveFileName(candidate, fallbackName);

  fs::path out;
  for (const std::string &segment : sanitizedParts)
    out /= PathUtils::PathFromUtf8(segment);
  return out.generic_string();
}

// Builds a stable fixture type/profile key for root-level category metadata.
static std::string
BuildFixtureTypeInfoKey(const Fixture &fixture,
                                               const std::string &gdtfArchivePath) {
  std::ostringstream key;
  key << TrimAscii(gdtfArchivePath) << '|' << TrimAscii(fixture.gdtfMode);
  if (TrimAscii(gdtfArchivePath).empty())
    key << '|' << TrimAscii(fixture.typeName);
  std::string value = key.str();
  for (char &ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch < 32 || ch == '/' || ch == '\\')
      ch = '_';
  }
  return TrimAscii(value);
}

// Returns true when a category source represents an explicit user choice.
static bool IsManualFixtureCategorySource(const std::string &source) {
  return ToLowerAscii(TrimAscii(source)) == "manual";
}

// Merges one fixture's shared type metadata into the export map.
static void MergeFixtureTypeInfoExport(
    std::map<std::string, FixtureTypeInfoExport> &metadataByType,
    const Fixture &fixture, const std::string &gdtfArchivePath) {
  const std::string category = TrimAscii(fixture.category);
  const std::string visualColorHex = TrimAscii(fixture.visualColorHex);
  if (category.empty() && visualColorHex.empty())
    return;

  const std::string key = BuildFixtureTypeInfoKey(fixture, gdtfArchivePath);
  if (key.empty())
    return;

  const std::string source = TrimAscii(fixture.categorySource);
  auto [it, inserted] = metadataByType.try_emplace(key);
  FixtureTypeInfoExport &entry = it->second;
  if (inserted) {
    entry.key = key;
    entry.gdtfSpec = TrimAscii(gdtfArchivePath);
    entry.gdtfMode = TrimAscii(fixture.gdtfMode);
    entry.model = TrimAscii(fixture.typeName);
    entry.category = category;
    entry.categorySource = source;
    entry.visualColorHex = visualColorHex;
    return;
  }

  const bool incomingManual = IsManualFixtureCategorySource(source);
  const bool existingManual =
      IsManualFixtureCategorySource(entry.categorySource);
  if (incomingManual && !existingManual) {
    entry.category = category;
    entry.categorySource = source;
  }
  if (!visualColorHex.empty() &&
      (entry.visualColorHex.empty() || visualColorHex < entry.visualColorHex))
    entry.visualColorHex = visualColorHex;
}

// Appends deduplicated fixture type metadata to root Perastage UserData.
static void AppendFixtureTypeMetadata(
    tinyxml2::XMLDocument &doc, tinyxml2::XMLElement *perastageData,
    const std::map<std::string, FixtureTypeInfoExport> &metadataByType) {
  if (!perastageData || metadataByType.empty())
    return;

  tinyxml2::XMLElement *map = doc.NewElement("FixtureTypeInfoMap");
  for (const auto &[key, entry] : metadataByType) {
    tinyxml2::XMLElement *info = doc.NewElement("FixtureTypeInfo");
    info->SetAttribute("key", entry.key.c_str());
    if (!entry.gdtfSpec.empty())
      info->SetAttribute("gdtfSpec", entry.gdtfSpec.c_str());
    if (!entry.gdtfMode.empty())
      info->SetAttribute("gdtfMode", entry.gdtfMode.c_str());
    if (!entry.manufacturer.empty())
      info->SetAttribute("manufacturer", entry.manufacturer.c_str());
    if (!entry.model.empty())
      info->SetAttribute("model", entry.model.c_str());

    if (!entry.category.empty()) {
      tinyxml2::XMLElement *category = doc.NewElement("Category");
      category->SetText(entry.category.c_str());
      info->InsertEndChild(category);
    }
    if (!entry.category.empty() && !entry.categorySource.empty()) {
      tinyxml2::XMLElement *source = doc.NewElement("CategorySource");
      source->SetText(entry.categorySource.c_str());
      info->InsertEndChild(source);
    }
    if (!entry.visualColorHex.empty()) {
      tinyxml2::XMLElement *visualColor = doc.NewElement("VisualColor");
      visualColor->SetText(entry.visualColorHex.c_str());
      info->InsertEndChild(visualColor);
    }
    map->InsertEndChild(info);
  }
  perastageData->InsertEndChild(map);
}

// Builds the canonical root-level Perastage GDTF archive filename for a truss.
static std::string BuildTrussGdtfArchiveName(const Truss &truss) {
  std::string fallbackModel = TrimAscii(truss.model);
  if (fallbackModel.empty() && truss.lengthMm > 0.0f) {
    const int lengthMm = static_cast<int>(std::lround(truss.lengthMm));
    if (lengthMm > 0)
      fallbackModel = "Truss_" + std::to_string(lengthMm) + "mm";
  }
  if (fallbackModel.empty())
    fallbackModel = "Truss";

  return SanitizeArchiveFileName(
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
          truss.manufacturer, truss.model, fallbackModel),
      "Unknown@Truss@Perastage.gdtf");
}

// Builds the internal truss type key used for export-time resource reuse.
static std::string BuildTrussTypeKey(const Truss &truss) {
  std::ostringstream key;
  key << TrimAscii(truss.gdtfSpec) << '|' << TrimAscii(truss.modelFile) << '|'
      << TrimAscii(truss.manufacturer) << '|' << TrimAscii(truss.model) << '|'
      << TrimAscii(truss.crossSection) << '|' << truss.lengthMm << '|'
      << truss.widthMm << '|' << truss.heightMm << '|' << truss.weightKg;
  return key.str();
}

// Builds path-free Perastage truss type metadata for exported UserData.
static std::string
BuildExportTrussTypeKey(const Truss &truss,
                                           const std::string &auxGdtfArchivePath) {
  std::ostringstream key;
  key << TrimAscii(truss.manufacturer) << '|' << TrimAscii(truss.model) << '|'
      << TrimAscii(truss.crossSection) << '|' << truss.lengthMm << '|'
      << truss.widthMm << '|' << truss.heightMm << '|' << truss.weightKg << '|'
      << SanitizeArchiveFileName(auxGdtfArchivePath, "");
  std::string value = key.str();
  for (char &ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch < 32 || ch == '/' || ch == '\\')
      ch = '_';
  }
  return TrimAscii(value);
}

static const char *
ToRepresentationText(Truss::GeometryRepresentation representation) {
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

// Returns true when a value is a root-level MVR FileName reference.
static bool IsValidMvrFileName(const std::string &value) {
  if (value.empty())
    return false;
  if (fs::path(value).stem().generic_string().empty())
    return false;
  for (unsigned char c : value) {
    if (c < 32)
      return false;
  }
  return value.find('/') == std::string::npos &&
         value.find('\\') == std::string::npos &&
         value.find(':') == std::string::npos &&
         value.find('*') == std::string::npos &&
         value.find('?') == std::string::npos &&
         value.find('"') == std::string::npos &&
         value.find('<') == std::string::npos &&
         value.find('>') == std::string::npos &&
         value.find('|') == std::string::npos && value != "." && value != "..";
}

// Returns true when exported metadata text looks like a local filesystem path.
static bool LooksLikeLocalFilesystemPath(const std::string &value) {
  const std::string trimmed = TrimAscii(value);
  const std::string lower = ToLowerAscii(trimmed);
  return trimmed.find('\\') != std::string::npos ||
         trimmed.rfind("/", 0) == 0 ||
         (trimmed.size() >= 3 &&
          std::isalpha(static_cast<unsigned char>(trimmed[0])) &&
          trimmed[1] == ':' && (trimmed[2] == '/' || trimmed[2] == '\\')) ||
         lower.find("/users/") != std::string::npos ||
         lower.find("/home/") != std::string::npos ||
         lower.find("appdata") != std::string::npos ||
         lower.find("/tmp/") != std::string::npos ||
         lower.find("/temp/") != std::string::npos;
}

static bool IsCanonicalUuidString(const std::string &value) {
  if (value.empty())
    return false;
  return CanonicalizeUuid(value) == value;
}

// Returns a stable, unique MVR Symbol UUID for the current export.
static std::string ResolveExportSymbolUuid(
    const std::string &candidateUuid, const std::string &containerUuid,
    const std::string &symdefUuid, const std::string &deterministicSeed,
    std::unordered_set<std::string> &usedSymbolUuids,
    std::vector<std::string> *exportWarnings, const std::string &context) {
  const std::string canonicalCandidate = CanonicalizeUuid(candidateUuid);
  const std::string canonicalContainer = CanonicalizeUuid(containerUuid);
  const std::string canonicalSymdef = CanonicalizeUuid(symdefUuid);
  const bool candidateConflicts =
      !canonicalCandidate.empty() &&
      (usedSymbolUuids.contains(canonicalCandidate) ||
       (!canonicalContainer.empty() &&
        canonicalCandidate == canonicalContainer) ||
       (!canonicalSymdef.empty() && canonicalCandidate == canonicalSymdef));

  if (!canonicalCandidate.empty() && !candidateConflicts) {
    usedSymbolUuids.insert(canonicalCandidate);
    return canonicalCandidate;
  }

  if (!TrimAscii(candidateUuid).empty() && exportWarnings) {
    exportWarnings->push_back(
        "MVR export replaced invalid or conflicting Symbol uuid '" +
                              TrimAscii(candidateUuid) + "' for " + context + ".");
  }

  for (int suffix = 0;; ++suffix) {
    const std::string seed = deterministicSeed + "#" + std::to_string(suffix);
    const std::string generated = DeriveDeterministicUuid(seed);
    if (generated.empty() || usedSymbolUuids.contains(generated) ||
        (!canonicalContainer.empty() && generated == canonicalContainer) ||
        (!canonicalSymdef.empty() && generated == canonicalSymdef)) {
      continue;
    }
    usedSymbolUuids.insert(generated);
    return generated;
  }
}

// Validate MVR 1.6 XML/archive consistency while downgrading missing resource
// issues to warnings.
static bool ValidateMvr16Export(
    tinyxml2::XMLDocument &doc,
    const std::unordered_map<std::string, std::string> &gdtfPathsByUuid,
    const std::unordered_map<std::string, int> &archiveEntryCount,
    std::vector<std::string> *exportWarnings) {
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  if (!root) {
    wxLogError(
        "MVR export validation failed: missing GeneralSceneDescription root");
    return false;
  }

  if (root->IntAttribute("verMajor") != 1 ||
      root->IntAttribute("verMinor") != 6) {
    wxLogError("MVR export validation failed: root version must be 1.6");
    return false;
  }

  const char *provider = root->Attribute("provider");
  const char *providerVersion = root->Attribute("providerVersion");
  if (!provider || std::string(provider).empty() || !providerVersion ||
      std::string(providerVersion).empty()) {
    wxLogError("MVR export validation failed: provider/providerVersion are "
               "required for MVR 1.6");
    return false;
  }
  if (std::string(providerVersion) == "1.0" &&
      std::string(app::kVersion) != "1.0") {
    wxLogError("MVR export validation failed: providerVersion fell back to 1.0 "
               "instead of the Perastage app version");
    return false;
  }

  std::unordered_set<int> numericIds;
  std::vector<std::string> referencedFiles;
  std::unordered_set<std::string> positionUuids;
  std::unordered_set<std::string> referencedPositionUuids;
  std::unordered_set<std::string> exportedTrussUuids;


  auto validateChildOrder = [](tinyxml2::XMLElement *objectNode,
                               const std::vector<std::string> &officialOrder) {
    int lastIndex = -1;
    std::string lastName;
    for (tinyxml2::XMLElement *child = objectNode->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const std::string childName = child->Name() ? child->Name() : "";
      auto it = std::find(officialOrder.begin(), officialOrder.end(), childName);
      if (it == officialOrder.end())
        continue;
      const int index = static_cast<int>(std::distance(officialOrder.begin(), it));
      if (index < lastIndex) {
        wxLogError("MVR export validation failed: %s uuid '%s' writes child %s after %s, violating MVR 1.6 order",
                   objectNode->Name() ? objectNode->Name() : "Object",
                   objectNode->Attribute("uuid") ? objectNode->Attribute("uuid") : "",
                   childName.c_str(), lastName.c_str());
        return false;
      }
      lastIndex = index;
      lastName = childName;
    }
    return true;
  };

  if (tinyxml2::XMLElement *scene = root->FirstChildElement("Scene")) {
    if (tinyxml2::XMLElement *layers = scene->FirstChildElement("Layers")) {
      for (tinyxml2::XMLElement *layer = layers->FirstChildElement("Layer");
           layer; layer = layer->NextSiblingElement("Layer")) {
        if (layer->FirstChildElement("Color")) {
          wxLogError("MVR export validation failed: Layer/Color is not valid "
                     "in MVR 1.6");
          return false;
        }
      }
    }
    if (scene->FirstChildElement("UserData")) {
      wxLogError("MVR export validation failed: Scene/UserData is not valid in "
                 "MVR 1.6");
      return false;
    }
    if (tinyxml2::XMLElement *aux = scene->FirstChildElement("AUXData")) {
      for (tinyxml2::XMLElement *pos = aux->FirstChildElement("Position"); pos;
           pos = pos->NextSiblingElement("Position")) {
        const std::string uuid =
            TrimAscii(pos->Attribute("uuid") ? pos->Attribute("uuid") : "");
        if (!IsCanonicalUuidString(uuid)) {
          wxLogError("MVR export validation failed: Position uuid '%s' is not "
                     "canonical",
                     uuid);
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
      if (std::string(cur->Name()) == "Symbol") {
        const tinyxml2::XMLElement *parent =
            cur->Parent() ? cur->Parent()->ToElement() : nullptr;
        const std::string context =
            parent && parent->Name() ? std::string(parent->Name()) : "unknown";
        const std::string symbolUuid =
            TrimAscii(cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
        const std::string symdefUuid =
            TrimAscii(cur->Attribute("symdef") ? cur->Attribute("symdef") : "");
        if (!IsCanonicalUuidString(symbolUuid)) {
          wxLogError("MVR export validation failed: Symbol in %s has missing "
                     "or non-canonical uuid '%s'",
                     context, symbolUuid);
          return false;
        }
        if (symdefUuid.empty()) {
          wxLogError("MVR export validation failed: Symbol uuid '%s' in %s has "
                     "no symdef",
                     symbolUuid, context);
          return false;
        }
      }

      if (std::string(cur->Name()) == "Truss") {
        if (!validateChildOrder(cur, {"Matrix", "Classing", "Position", "Geometries", "Function", "GDTFSpec", "GDTFMode", "CastShadow", "Addresses", "Alignments", "CustomCommands", "Overwrites", "Connections", "ChildPosition", "ChildList", "FixtureID", "FixtureIDNumeric", "UnitNumber", "CustomIdType", "CustomId"}))
          return false;
        const std::string trussUuid =
            TrimAscii(cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
        if (!IsCanonicalUuidString(trussUuid)) {
          wxLogError("MVR export validation failed: Truss uuid '%s' is missing "
                     "or non-canonical",
                     trussUuid);
          return false;
        }
        exportedTrussUuids.insert(trussUuid);
        if (cur->FirstChildElement("UserData")) {
          wxLogError("MVR export validation failed: Truss uuid '%s' contains "
                     "direct UserData",
                     trussUuid);
          return false;
        }
      }

      if (std::string(cur->Name()) == "Fixture") {
        if (!validateChildOrder(cur, {"Matrix", "Classing", "GDTFSpec", "GDTFMode", "Focus", "CastShadow", "DMXInvertPan", "DMXInvertTilt", "Position", "Function", "FixtureID", "FixtureIDNumeric", "UnitNumber", "ChildPosition", "Addresses", "Protocols", "Alignments", "CustomCommands", "Overwrites", "Connections", "Color", "CustomIdType", "CustomId", "Mappings", "Gobo", "ChildList"}))
          return false;
        if (cur->FirstChildElement("UserData")) {
          wxLogError("MVR export validation failed: Fixture uuid '%s' contains "
                     "direct UserData",
                     cur->Attribute("uuid") ? cur->Attribute("uuid")
                                            : "(missing uuid)");
          return false;
        }
        if (cur->FirstChildElement("Category") ||
            cur->FirstChildElement("CategorySource") ||
            cur->FirstChildElement("CategoryReason")) {
          wxLogError("MVR export validation failed: Fixture contains Perastage "
                     "category metadata");
          return false;
        }
        auto *idNode = cur->FirstChildElement("FixtureID");
        auto *numNode = cur->FirstChildElement("FixtureIDNumeric");
        const std::string fixtureUuid =
            cur->Attribute("uuid") ? cur->Attribute("uuid") : "(missing uuid)";
        const std::string fixtureName = cur->Attribute("name")
                                            ? cur->Attribute("name")
                                            : "(unnamed fixture)";

        const std::string fixtureId =
            idNode && idNode->GetText() ? TrimAscii(idNode->GetText()) : "";
        const std::string fixtureNumericText =
            numNode && numNode->GetText() ? TrimAscii(numNode->GetText()) : "";
        int fixtureNumeric = 0;

        if (fixtureId.empty() ||
            !TryParseInt(fixtureNumericText, fixtureNumeric) ||
            fixtureNumeric <= 0) {
          wxLogError(
              "MVR export validation failed: Fixture '%s' (uuid=%s) must have "
              "a non-empty FixtureID and a positive integer FixtureIDNumeric",
              fixtureName, fixtureUuid);
          return false;
        }

        auto *unitNode = cur->FirstChildElement("UnitNumber");
        int unitValue = 0;
        const std::string unitText = unitNode && unitNode->GetText()
                                         ? TrimAscii(unitNode->GetText())
                                         : "";
        if (unitText.empty() || !TryParseInt(unitText, unitValue) ||
            unitValue <= 0) {
          wxLogError("MVR export validation failed: Fixture '%s' (uuid=%s) "
                     "must have a positive integer UnitNumber",
              fixtureName, fixtureUuid);
          return false;
        }

        if (!numericIds.insert(fixtureNumeric).second) {
          wxLogError("MVR export validation failed: FixtureIDNumeric must be globally unique positive integer");
          return false;
        }

        if (auto *addresses = cur->FirstChildElement("Addresses"); addresses) {
          std::unordered_set<int> usedBreaks;
          for (auto *addressNode = addresses->FirstChildElement("Address");
               addressNode;
               addressNode = addressNode->NextSiblingElement("Address")) {
            int breakNum = 0;
            const std::string breakText =
                addressNode->Attribute("break")
                                              ? TrimAscii(addressNode->Attribute("break"))
                                              : "0";
            if (!TryParseInt(breakText, breakNum) || breakNum < 0) {
              wxLogError("MVR export validation failed: Fixture '%s' (uuid=%s) "
                         "has invalid Address break '%s'",
                  fixtureName, fixtureUuid, breakText);
              return false;
            }
            if (!usedBreaks.insert(breakNum).second) {
              wxLogError("MVR export validation failed: Fixture '%s' (uuid=%s) "
                         "has duplicate Address break %d",
                  fixtureName, fixtureUuid, breakNum);
              return false;
            }

            const std::string addressText =
                addressNode->GetText() ? TrimAscii(addressNode->GetText()) : "";
            int universe = 0;
            int channel = 0;
            if (!ParseMvrAddressNodeText(addressText, universe, channel)) {
              wxLogError("MVR export validation failed: Fixture '%s' (uuid=%s) "
                         "has invalid Address value '%s'",
                  fixtureName, fixtureUuid, addressText);
              return false;
            }
          }
        }
      }

      if (std::string(cur->Name()) == "SceneObject") {
        if (!validateChildOrder(cur, {"Matrix", "Classing", "Geometries", "GDTFSpec", "GDTFMode", "CastShadow", "Addresses", "Alignments", "CustomCommands", "Overwrites", "Connections", "FixtureID", "FixtureIDNumeric", "UnitNumber", "CustomId", "CustomIdType", "ChildList"}))
          return false;
        if (cur->FirstChildElement("UserData")) {
          wxLogError("MVR export validation failed: SceneObject uuid '%s' "
                     "must not contain direct UserData",
                     cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
          return false;
        }
        auto *sceneObjectIdNode = cur->FirstChildElement("FixtureID");
        auto *sceneObjectNumNode = cur->FirstChildElement("FixtureIDNumeric");
        const std::string sceneObjectId =
            sceneObjectIdNode && sceneObjectIdNode->GetText()
                ? TrimAscii(sceneObjectIdNode->GetText())
                : "";
        const std::string sceneObjectNumericText =
            sceneObjectNumNode && sceneObjectNumNode->GetText()
                ? TrimAscii(sceneObjectNumNode->GetText())
                : "";
        int sceneObjectNumeric = 0;
        if (sceneObjectId.empty() ||
            !TryParseInt(sceneObjectNumericText, sceneObjectNumeric) ||
            sceneObjectNumeric <= 0) {
          wxLogError("MVR export validation failed: SceneObject uuid '%s' must have FixtureID and FixtureIDNumeric",
                     cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
          return false;
        }
        if (!numericIds.insert(sceneObjectNumeric).second) {
          wxLogError("MVR export validation failed: FixtureIDNumeric must be globally unique positive integer");
          return false;
        }
        int matrixOrder = -1;
        int geometriesOrder = -1;
        int childOrder = 0;
        for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
             child = child->NextSiblingElement(), ++childOrder) {
          const std::string childName = child->Name() ? child->Name() : "";
          if (childName == "Matrix" && matrixOrder < 0)
            matrixOrder = childOrder;
          else if (childName == "Geometries" && geometriesOrder < 0)
            geometriesOrder = childOrder;
        }
        if (matrixOrder >= 0 && geometriesOrder >= 0 &&
            geometriesOrder < matrixOrder) {
          wxLogError("MVR export validation failed: SceneObject uuid '%s' "
                     "writes Geometries before Matrix",
                     cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
          return false;
        }
      }

      if (std::string(cur->Name()) == "GroupObject") {
        if (!validateChildOrder(cur, {"Matrix", "Classing", "ChildList"}))
          return false;
        if (cur->FirstChildElement("UserData")) {
          wxLogError("MVR export validation failed: GroupObject uuid '%s' contains direct UserData",
                     cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
          return false;
        }
      }

      if ((std::string(cur->Name()) == "Fixture" ||
           std::string(cur->Name()) == "SceneObject" ||
           std::string(cur->Name()) == "Support" ||
           std::string(cur->Name()) == "Truss") &&
          cur->FirstChildElement("GDTFSpec") &&
          !cur->FirstChildElement("GDTFMode")) {
        wxLogError("MVR export validation failed: %s uuid '%s' has GDTFSpec without GDTFMode",
                   cur->Name(),
                   cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
        return false;
      }

      if (std::string(cur->Name()) == "Position") {
        const tinyxml2::XMLElement *parent =
            cur->Parent() ? cur->Parent()->ToElement() : nullptr;
        const std::string parentName =
            parent ? std::string(parent->Name()) : std::string{};
        const bool isObjectPositionRef =
            parentName == "Fixture" || parentName == "Truss" ||
            parentName == "Support" || parentName == "VideoScreen" ||
            parentName == "Projector";
        if (isObjectPositionRef) {
          const std::string positionRef =
              TrimAscii(cur->GetText() ? cur->GetText() : "");
          if (positionRef.empty())
            continue;
          if (!IsCanonicalUuidString(positionRef)) {
            wxLogError("MVR export validation failed: Position reference '%s' "
                       "is not canonical",
                       positionRef);
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
      std::transform(
          attrName.begin(), attrName.end(), attrName.begin(),
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
            isMultipatchChild =
                std::string(mp) == "true" || std::string(mp) == "1";
          if (!isMultipatchChild) {
            const char *idText =
                cur->FirstChildElement("FixtureID")
                                     ? cur->FirstChildElement("FixtureID")->GetText()
                                     : nullptr;
            const char *numText =
                cur->FirstChildElement("FixtureIDNumeric")
                                      ? cur->FirstChildElement("FixtureIDNumeric")->GetText()
                                      : nullptr;
            if (!idText || TrimAscii(idText).empty() || !numText) {
              wxLogError("MVR export validation failed: %s is missing "
                         "FixtureID/FixtureIDNumeric",
                         tagName);
              return false;
            }
            int numeric = 0;
            if (!TryParseInt(numText, numeric) || numeric <= 0 ||
                !numericIds.insert(numeric).second) {
              wxLogError("MVR export validation failed: FixtureIDNumeric must "
                         "be globally unique positive integer");
              return false;
            }
          }

          if (tinyxml2::XMLElement *gdtf = cur->FirstChildElement("GDTFSpec")) {
            const char *txt = gdtf->GetText();
            std::string value = txt ? txt : "";
            if (!IsValidMvrFileName(value)) {
              wxLogError("MVR export validation failed: GDTFSpec '%s' is not a "
                         "valid archive-relative FileName",
                         value);
              return false;
            }
            auto uidIt = gdtfPathsByUuid.find(
                cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
            if (uidIt != gdtfPathsByUuid.end() && uidIt->second != value) {
              wxLogError("MVR export validation failed: GDTFSpec mismatch for "
                         "object uuid '%s'",
                         cur->Attribute("uuid"));
              return false;
            }
            auto gdtfArchiveIt = archiveEntryCount.find(value);
            if (gdtfArchiveIt == archiveEntryCount.end() ||
                gdtfArchiveIt->second < 1) {
              if (exportWarnings)
                exportWarnings->push_back(
                    "Referenced file '" + value +
                                          "' is missing from the archive and will be omitted.");
            } else if (gdtfArchiveIt->second > 1) {
              if (exportWarnings)
                exportWarnings->push_back(
                    "Referenced file '" + value +
                                          "' appears multiple times; duplicates will be ignored.");
            }
          }

          if (std::string(tagName) == "Support") {
            if (!validateChildOrder(cur, {"Matrix", "Classing", "Position", "Geometries", "Function", "ChainLength", "GDTFSpec", "GDTFMode", "CastShadow", "Addresses", "Alignments", "CustomCommands", "Overwrites", "Connections", "FixtureID", "FixtureIDNumeric", "UnitNumber", "CustomIdType", "CustomId", "ChildList"}))
              return false;
            if (!cur->FirstChildElement("Geometries")) {
              wxLogError("MVR export validation failed: Support uuid '%s' has "
                         "no Geometries",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
            if (!cur->FirstChildElement("ChainLength")) {
              wxLogError("MVR export validation failed: Support uuid '%s' has "
                         "no ChainLength",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
            const bool hasGdtfSpec =
                cur->FirstChildElement("GDTFSpec") != nullptr;
            const bool hasGdtfMode =
                cur->FirstChildElement("GDTFMode") != nullptr;
            if (hasGdtfSpec != hasGdtfMode) {
              wxLogError("MVR export validation failed: Support uuid '%s' has "
                         "inconsistent GDTFSpec/GDTFMode",
                         cur->Attribute("uuid") ? cur->Attribute("uuid") : "");
              return false;
            }
            if (cur->FirstChildElement("UserData")) {
              wxLogError("MVR export validation failed: Support uuid '%s' "
                         "contains direct UserData",
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

  if (tinyxml2::XMLElement *rootUserData =
          root->FirstChildElement("UserData")) {
    for (tinyxml2::XMLElement *data = rootUserData->FirstChildElement("Data");
         data; data = data->NextSiblingElement("Data")) {
      const std::string provider = ToLowerAscii(TrimAscii(
          data->Attribute("provider") ? data->Attribute("provider") : ""));
      if (provider != "perastage")
        continue;
      for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap");
           map; map = map->NextSiblingElement("TrussInfoMap")) {
        for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo");
             info; info = info->NextSiblingElement("TrussInfo")) {
          const std::string uuid =
              TrimAscii(info->Attribute("uuid") ? info->Attribute("uuid") : "");
          if (!IsCanonicalUuidString(uuid) ||
              !exportedTrussUuids.contains(uuid)) {
            wxLogError("MVR export validation failed: root TrussInfo "
                       "references invalid exported Truss uuid '%s'",
                       uuid);
            return false;
          }
          for (const char *nodeName : {"ModelFile", "AuxGdtf", "TypeKey"}) {
            if (tinyxml2::XMLElement *pathNode =
                    info->FirstChildElement(nodeName)) {
              const std::string value =
                  TrimAscii(pathNode->GetText() ? pathNode->GetText() : "");
              if (LooksLikeLocalFilesystemPath(value) ||
                  value.find('/') != std::string::npos ||
                  value.find('\\') != std::string::npos) {
                wxLogError("MVR export validation failed: Perastage UserData "
                           "%s contains non-portable path text '%s'",
                           nodeName, value);
                return false;
              }
            }
          }
        }
      }
      if (tinyxml2::XMLElement *manifest =
              data->FirstChildElement("TrussSidecarManifest")) {
        for (tinyxml2::XMLElement *type = manifest->FirstChildElement("Type");
             type; type = type->NextSiblingElement("Type")) {
          const std::string key =
              TrimAscii(type->Attribute("key") ? type->Attribute("key") : "");
          const std::string gdtf =
              TrimAscii(type->Attribute("gdtf") ? type->Attribute("gdtf") : "");
          if (LooksLikeLocalFilesystemPath(key) ||
              key.find('/') != std::string::npos ||
              key.find('\\') != std::string::npos) {
            wxLogError("MVR export validation failed: Perastage UserData "
                       "TrussSidecarManifest Type key contains non-portable "
                       "path text '%s'",
                       key);
            return false;
          }
          if (!IsValidMvrFileName(gdtf)) {
            wxLogError("MVR export validation failed: Perastage UserData "
                       "TrussSidecarManifest Type gdtf '%s' is not a "
                       "root-level MVR filename",
                       gdtf);
            return false;
          }
        }
        for (tinyxml2::XMLElement *inst =
                 manifest->FirstChildElement("Instance");
             inst; inst = inst->NextSiblingElement("Instance")) {
          const std::string uuid =
              TrimAscii(inst->Attribute("uuid") ? inst->Attribute("uuid") : "");
          if (!IsCanonicalUuidString(uuid) ||
              !exportedTrussUuids.contains(uuid)) {
            wxLogError("MVR export validation failed: Truss sidecar manifest "
                       "references invalid exported Truss uuid '%s'",
                       uuid);
            return false;
          }
          const std::string typeKey = TrimAscii(
              inst->Attribute("typeKey") ? inst->Attribute("typeKey") : "");
          if (LooksLikeLocalFilesystemPath(typeKey) ||
              typeKey.find('/') != std::string::npos ||
              typeKey.find('\\') != std::string::npos) {
            wxLogError("MVR export validation failed: Perastage UserData "
                       "TrussSidecarManifest Instance typeKey contains "
                       "non-portable path text '%s'",
                       typeKey);
            return false;
          }
        }
      }
    }
  }

  std::unordered_set<std::string> warnedMissingFiles;
  std::unordered_set<std::string> warnedDuplicateFiles;
  for (const auto &fileRef : referencedFiles) {
    if (!IsValidMvrFileName(fileRef)) {
      wxLogError(
          "MVR export validation failed: invalid FileName reference '%s'",
          fileRef);
      return false;
    }

    auto fileRefIt = archiveEntryCount.find(fileRef);
    if (fileRefIt == archiveEntryCount.end() || fileRefIt->second < 1) {
      if (exportWarnings && warnedMissingFiles.insert(fileRef).second) {
        exportWarnings->push_back(
            "Referenced file '" + fileRef +
                                  "' is missing from the archive and will be omitted.");
      }
      continue;
    }

    if (fileRefIt->second > 1) {
      if (exportWarnings && warnedDuplicateFiles.insert(fileRef).second) {
        exportWarnings->push_back(
            "Referenced file '" + fileRef +
                                  "' appears multiple times; duplicates will be ignored.");
      }
    }
  }

  std::unordered_set<std::string> lowercaseArchiveEntries;
  for (const auto &[archivePath, count] : archiveEntryCount) {
    if (!IsValidMvrFileName(archivePath)) {
      wxLogError("MVR export validation failed: ZIP entry '%s' is not a "
                 "root-level MVR filename",
                 archivePath);
      return false;
    }
    if (archivePath.empty()) {
      wxLogError("MVR export validation failed: found empty ZIP entry path");
      return false;
    }
    const std::string lowercaseArchivePath = ToLowerAscii(archivePath);
    if (!lowercaseArchiveEntries.insert(lowercaseArchivePath).second) {
      wxLogError("MVR export validation failed: ZIP entries must not differ only by case: '%s'",
                 archivePath);
      return false;
    }
    if (count != 1) {
      wxLogError("MVR export validation failed: duplicate ZIP entry '%s'",
                 archivePath);
      return false;
    }
  }

  for (const auto &positionRef : referencedPositionUuids) {
    if (!positionUuids.contains(positionRef)) {
      wxLogError("MVR export validation failed: Position reference '%s' has no "
                 "AUXData/Position definition",
                 positionRef);
      return false;
    }
  }

  return true;
}

// Normalizes archive entry paths for stable comparisons during resource
// pruning.
static std::string NormalizeArchiveEntryPath(std::string path) {
  path = TrimAscii(std::move(path));
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0)
    path.erase(0, 2);
  while (!path.empty() && path.front() == '/')
    path.erase(path.begin());
  return path;
}

// Collects every archive-relative file path referenced by file-bearing XML
// attributes.
static std::unordered_set<std::string>
CollectReferencedArchivePaths(const tinyxml2::XMLDocument &doc) {
  std::unordered_set<std::string> referencedPaths;
  const tinyxml2::XMLElement *root =
      doc.FirstChildElement("GeneralSceneDescription");
  if (!root)
    return referencedPaths;

  std::vector<const tinyxml2::XMLElement *> stack;
  for (const tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    stack.push_back(node);
  }

  while (!stack.empty()) {
    const tinyxml2::XMLElement *current = stack.back();
    stack.pop_back();
    if (!current)
      continue;

    const char *fileName = current->Attribute("fileName");
    if (fileName) {
      const std::string normalized =
          NormalizeArchiveEntryPath(TrimAscii(fileName));
      if (!normalized.empty())
        referencedPaths.insert(normalized);
    }

    // Captures <GDTFSpec>archive/path.gdtf</GDTFSpec> element text references.
    if (std::string(current->Name()) == "GDTFSpec") {
      const char *gdtfSpecText = current->GetText();
      if (gdtfSpecText) {
        const std::string normalized =
            NormalizeArchiveEntryPath(TrimAscii(gdtfSpecText));
        if (!normalized.empty())
          referencedPaths.insert(normalized);
      }
    }

    for (const tinyxml2::XMLElement *child = current->FirstChildElement();
         child; child = child->NextSiblingElement()) {
      stack.push_back(child);
    }
  }

  return referencedPaths;
}

// Logs referenced and pruned archive paths to diagnose unexpected retained
// resources.
static void LogResourcePruneDiagnostics(
    const std::unordered_set<std::string> &referencedArchivePaths,
    const std::vector<ResourceEntry> &allResourceEntries,
    const std::vector<ResourceEntry> &keptResourceEntries) {
  std::unordered_set<std::string> keptNormalized;
  keptNormalized.reserve(keptResourceEntries.size());
  for (const auto &entry : keptResourceEntries) {
    const std::string normalized = NormalizeArchiveEntryPath(entry.archivePath);
    if (!normalized.empty())
      keptNormalized.insert(normalized);
  }

  size_t prunedCount = 0;
  for (const auto &entry : allResourceEntries) {
    const std::string normalized = NormalizeArchiveEntryPath(entry.archivePath);
    if (normalized.empty())
      continue;
    if (!keptNormalized.contains(normalized)) {
      ++prunedCount;
      Logger::Instance().Log(
          Logger::Level::Info,
          "MVR export pruned unreferenced archive resource: " + normalized);
    }
  }

  Logger::Instance().Log(
      Logger::Level::Info,
      "MVR export resource pruning summary: referenced_paths=" +
          std::to_string(referencedArchivePaths.size()) +
          ", planned_resources_before=" +
          std::to_string(allResourceEntries.size()) +
          ", planned_resources_after=" +
          std::to_string(keptResourceEntries.size()) +
          ", pruned=" + std::to_string(prunedCount));
}

// Returns whether a Symdef has enough geometry data to flatten into Geometry3D
// nodes.
static bool CanFlattenSymdefGeometry(const MvrScene &scene,
                                     const std::string &symdefUuid) {
  auto geoIt = scene.symdefGeometries.find(symdefUuid);
  if (geoIt != scene.symdefGeometries.end()) {
    for (const SymdefGeometry &geometry : geoIt->second) {
      if (!TrimAscii(geometry.file).empty())
        return true;
    }
  }

  auto fileIt = scene.symdefFiles.find(symdefUuid);
  return fileIt != scene.symdefFiles.end() &&
         !TrimAscii(fileIt->second).empty();
}

// Collects Symdef UUIDs referenced by trusses that preserve Symbol/Symdef
// geometry representation.
static std::unordered_set<std::string>
CollectReferencedSymdefUuids(const MvrScene &scene,
                             const MvrExportOptions &options) {
  std::unordered_set<std::string> referencedSymdefs;
  for (const auto &[uuid, truss] : scene.trusses) {
    if (truss.sourceRepresentation !=
        Truss::GeometryRepresentation::SymbolSymdef)
      continue;
    const std::string symdefUuid = TrimAscii(truss.sourceSymdefUuid);
    if (symdefUuid.empty())
      continue;
    if (options.trussGeometryExportMode ==
            MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols &&
        CanFlattenSymdefGeometry(scene, symdefUuid))
      continue;
    referencedSymdefs.insert(symdefUuid);
  }
  return referencedSymdefs;
}

static bool TryParseInt(std::string_view text, int &out) {
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
         !support.motorName.empty() || !support.motorManufacturer.empty() ||
         !support.motorModel.empty() || !support.motorFixtureUuid.empty() ||
         !support.useMotorDefaults || !support.dummyProfileId.empty() ||
         !support.dummyPreset.empty() ||
         NormalizeHoistDataSource(support.hoistDataSource) != "Inherited" ||
         NormalizeHoistDataSource(support.motorNameSource) != "Inherited" ||
         NormalizeHoistDataSource(support.motorManufacturerSource) !=
             "Inherited" ||
         NormalizeHoistDataSource(support.motorModelSource) != "Inherited" ||
         NormalizeHoistDataSource(support.capacitySource) != "Inherited" ||
         NormalizeHoistDataSource(support.weightSource) != "Inherited" ||
         NormalizeHoistDataSource(support.hoistFunctionSource) != "Inherited";
}

// Finds the first UserData element that already contains Perastage-owned data.
static tinyxml2::XMLElement *
FindFirstPerastageUserData(tinyxml2::XMLElement *node) {
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

// Finds or creates the Perastage Data element under a valid parent node.
static tinyxml2::XMLElement *
FindOrCreatePerastageDataNode(tinyxml2::XMLDocument &doc,
                                                            tinyxml2::XMLElement *node) {
  tinyxml2::XMLElement *ud = FindFirstPerastageUserData(node);
  if (!ud) {
    ud = doc.NewElement("UserData");
    node->InsertEndChild(ud);
  }

  for (tinyxml2::XMLElement *data = ud->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const std::string provider = TrimAscii(
        data->Attribute("provider") ? data->Attribute("provider") : "");
    if (provider.empty() || ToLowerAscii(provider) == "perastage")
      return data;
  }

  tinyxml2::XMLElement *data = doc.NewElement("Data");
  data->SetAttribute("provider", kMvrProvider);
  data->SetAttribute("ver", kPerastageUserDataSchemaVersion);
  ud->InsertEndChild(data);
  return data;
}

// Resolves the canonical MVR layer UUID exported for a Perastage layer.
static std::string ExportLayerUuid(const std::string &layerUuid,
                                   const std::string &layerName) {
  if (layerUuid.empty())
    return {};
  return IsCanonicalUuidString(layerUuid)
             ? layerUuid
             : DeriveDeterministicUuid("mvr:layer:" + layerName + ":" +
                                       layerUuid);
}

// Returns true when the color can be stored as Perastage #RRGGBB metadata.
static bool IsLayerColorMetadataValue(const std::string &color) {
  if (color.size() != 7 || color[0] != '#')
    return false;
  return std::all_of(color.begin() + 1, color.end(),
                     [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

// Returns true when any layer has Perastage color metadata to export.
static bool HasLayerAppearanceMetadata(const MvrScene &scene) {
  return std::any_of(scene.layers.begin(), scene.layers.end(),
                     [](const auto &entry) {
    return IsLayerColorMetadataValue(entry.second.color);
  });
}

// Appends the root-level Perastage layer appearance map when layers define
// colors.
static void AppendLayerAppearanceMetadata(tinyxml2::XMLDocument &doc,
                                          tinyxml2::XMLElement *perastageData,
                                          const MvrScene &scene) {
  if (!perastageData)
    return;

  tinyxml2::XMLElement *map = nullptr;
  for (tinyxml2::XMLElement *existing =
           perastageData->FirstChildElement("LayerAppearanceMap");
       existing;
       existing = existing->NextSiblingElement("LayerAppearanceMap")) {
    map = existing;
    break;
  }

  for (const auto &[layerUuid, layer] : scene.layers) {
    if (!IsLayerColorMetadataValue(layer.color))
      continue;
    if (!map)
      map = doc.NewElement("LayerAppearanceMap");

    tinyxml2::XMLElement *entry = doc.NewElement("PerastageLayerAppearance");
    const std::string exportUuid = ExportLayerUuid(layerUuid, layer.name);
    if (!exportUuid.empty())
      entry->SetAttribute("uuid", exportUuid.c_str());
    if (!layer.name.empty())
      entry->SetAttribute("name", layer.name.c_str());
    entry->SetAttribute("color", layer.color.c_str());
    map->InsertEndChild(entry);
  }

  if (map && !map->Parent())
    perastageData->InsertEndChild(map);
}

// Returns true when a truss carries Perastage-specific metadata for export.
static bool HasTrussInfoMetadata(const Truss &truss) {
  return truss.hasManualLoadOverride || !truss.crossSection.empty() ||
         !truss.modelFile.empty() || !truss.positionName.empty() ||
         !truss.manufacturer.empty() || !truss.model.empty() ||
         truss.lengthMm > 0.0f || truss.widthMm > 0.0f ||
         truss.heightMm > 0.0f || truss.weightKg > 0.0f ||
         truss.sourceRepresentation != Truss::GeometryRepresentation::Unknown ||
         !truss.perastageTypeKey.empty() ||
         !truss.perastageAuxGdtfArchivePath.empty();
}

// Appends one root-level Perastage truss metadata entry keyed by exported UUID.
static void AppendTrussInfoMetadata(tinyxml2::XMLDocument &doc,
                                    tinyxml2::XMLElement *trussInfoMap,
                                    const Truss &truss,
                                    const std::string &exportUuid,
                                    const std::string &trussTypeKey,
                                    const std::string &auxGdtfArchivePath) {
  if (!trussInfoMap || !HasTrussInfoMetadata(truss))
    return;

  tinyxml2::XMLElement *info = doc.NewElement("TrussInfo");
  info->SetAttribute("uuid", exportUuid.c_str());
  auto addTxt = [&](const char *name, const std::string &value) {
    if (value.empty())
      return;
    tinyxml2::XMLElement *node = doc.NewElement(name);
    node->SetText(value.c_str());
    info->InsertEndChild(node);
  };
  if (truss.hasManualLoadOverride) {
    tinyxml2::XMLElement *load = doc.NewElement("Load");
    load->SetAttribute("unit", "kg");
    load->SetAttribute("source", "Manual");
    load->SetText(std::to_string(truss.manualLoadKg).c_str());
    info->InsertEndChild(load);
  }
  addTxt("Manufacturer", truss.manufacturer);
  addTxt("Model", truss.model);
  if (truss.lengthMm > 0.0f)
    addTxt("Length", std::to_string(truss.lengthMm));
  if (truss.widthMm > 0.0f)
    addTxt("Width", std::to_string(truss.widthMm));
  if (truss.heightMm > 0.0f)
    addTxt("Height", std::to_string(truss.heightMm));
  if (truss.weightKg > 0.0f)
    addTxt("Weight", std::to_string(truss.weightKg));
  addTxt("CrossSection", truss.crossSection);
  addTxt("ModelFile", SanitizeArchiveFileName(truss.modelFile, ""));
  addTxt("PositionName", truss.positionName);
  addTxt("HangPos", truss.positionName);
  addTxt("Representation", ToRepresentationText(truss.sourceRepresentation));
  addTxt("TypeKey", trussTypeKey.empty()
                        ? SanitizeArchiveFileName(truss.perastageTypeKey, "")
                        : trussTypeKey);
  const std::string exportedAuxGdtf = auxGdtfArchivePath.empty()
                                          ? truss.perastageAuxGdtfArchivePath
                                          : auxGdtfArchivePath;
  addTxt("AuxGdtf", SanitizeArchiveFileName(exportedAuxGdtf, ""));
  trussInfoMap->InsertEndChild(info);
}

// Appends root-level Perastage hoist metadata keyed by Support UUID.
static void AppendSupportHoistInfoMetadata(tinyxml2::XMLDocument &doc,
                                           tinyxml2::XMLElement *hoistInfoMap,
                                           const Support &support) {
  if (!hoistInfoMap)
    return;

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
  if (support.loadSource == "Manual") {
    tinyxml2::XMLElement *load = doc.NewElement("Load");
    load->SetAttribute("unit", "kg");
    load->SetText(std::to_string(support.loadKg).c_str());
    info->InsertEndChild(load);
  }

  const std::string officialFunction =
      NormalizeHoistFunction(support.function.empty() ? support.hoistFunction
                                                      : support.function);
  const std::string hoistFunction =
      NormalizeHoistFunction(support.hoistFunction);
  if (!hoistFunction.empty() && hoistFunction != officialFunction)
    addText("RiggingPoint", hoistFunction);

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
  if (!hoistFunction.empty() && hoistFunction != officialFunction)
    addText("RiggingPointSource", hoistFunctionSource);

  hoistInfoMap->InsertEndChild(info);
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
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
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


// Inserts a GDTF FixtureType child at the standard schema position.
static tinyxml2::XMLElement *InsertGdtfFixtureTypeChildInOrder(
    tinyxml2::XMLElement *fixtureType, tinyxml2::XMLDocument &doc,
    const char *name) {
  tinyxml2::XMLElement *node = doc.NewElement(name);
  static constexpr const char *kOrder[] = {
      "AttributeDefinitions", "Wheels", "PhysicalDescriptions", "Models",
      "Geometries", "DMXModes", "Revisions", "FTPresets", "Protocols"};

  int targetIndex = -1;
  for (int i = 0; i < static_cast<int>(sizeof(kOrder) / sizeof(kOrder[0])); ++i) {
    if (std::string(name) == kOrder[i]) {
      targetIndex = i;
      break;
    }
  }

  tinyxml2::XMLElement *previous = nullptr;
  if (targetIndex >= 0) {
    for (tinyxml2::XMLElement *child = fixtureType->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      for (int i = targetIndex + 1; i < static_cast<int>(sizeof(kOrder) / sizeof(kOrder[0]));
           ++i) {
        if (std::string(child->Name()) == kOrder[i]) {
          tinyxml2::XMLNode *inserted =
              previous ? fixtureType->InsertAfterChild(previous, node)
                       : fixtureType->InsertFirstChild(node);
          return inserted ? inserted->ToElement() : nullptr;
        }
      }
      previous = child;
    }
  }

  tinyxml2::XMLNode *inserted = fixtureType->InsertEndChild(node);
  return inserted ? inserted->ToElement() : nullptr;
}

// Creates a temporary patched GDTF copy for intentional MVR export overrides.
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
  bool patched = false;
  if (!ov.color.empty()) {
    tinyxml2::XMLElement *models = ft->FirstChildElement("Models");
    if (models) {
      std::string cie = HexToCie(ov.color);
      for (tinyxml2::XMLElement *m = models->FirstChildElement("Model"); m;
           m = m->NextSiblingElement("Model"))
        m->SetAttribute("Color", cie.c_str());
      patched = true;
    }
  }
  const std::optional<float> weightKg =
      ov.hasWeightKg ? std::optional<float>(ov.weightKg) : std::nullopt;
  const std::optional<float> powerW =
      ov.hasPowerW ? std::optional<float>(ov.powerW) : std::nullopt;
  patched =
      GdtfMutationAudit::ApplyPhysicalProperties(ft, doc, weightKg, powerW) ||
            patched;
  if (!ov.manufacturer.empty()) {
    ft->SetAttribute("Manufacturer", ov.manufacturer.c_str());
    patched = true;
  }
  if (!ov.model.empty()) {
    ft->SetAttribute("Name", ov.model.c_str());
    patched = true;
  }

  if (ov.hasLengthMm || ov.hasWidthMm || ov.hasHeightMm) {
    tinyxml2::XMLElement *models = ft->FirstChildElement("Models");
    tinyxml2::XMLElement *model =
        models ? models->FirstChildElement("Model") : nullptr;
    if (!models)
      models = InsertGdtfFixtureTypeChildInOrder(ft, doc, "Models");
    if (!model)
      model = models->InsertNewChildElement("Model");
    if (ov.hasLengthMm)
      model->SetAttribute("Length", ov.lengthMm / 1000.0f);
    if (ov.hasWidthMm)
      model->SetAttribute("Width", ov.widthMm / 1000.0f);
    if (ov.hasHeightMm)
      model->SetAttribute("Height", ov.heightMm / 1000.0f);
    patched = true;
  }

  if (patched) {
    GdtfMutationAudit::AppendRevision(
        ft, doc,
        (ov.hasWeightKg || ov.hasPowerW)
                     ? kPhysicalPropertiesRevisionText
                     : "Patched fixture metadata for MVR export",
        GdtfMutationAudit::BuildPerastageModifiedBy());
  }

  doc.SaveFile(descPath.c_str());
  std::string outPath = tempDir + ".gdtf";
  if (!ZipDir(tempDir, outPath))
    return {};
  return outPath;
}

// Serialize the configured scene into a .mvr archive and collect non-fatal
// export warnings.
bool MvrExporter::ExportToFile(const std::string &filePath) {
  return ExportToFile(
      filePath, mvr::preferences::LoadExportOptions(ConfigManager::Get()));
}

// Serialize the configured scene into a .mvr archive with explicit export
// options.
bool MvrExporter::ExportToFile(const std::string &filePath,
                               const MvrExportOptions &options) {
  m_exportWarnings.clear();
  const auto &scene = ConfigManager::Get().GetScene();
  const TrussGeometryAuthority trussGeometryAuthority =
      GetTrussGeometryAuthoritySetting();
  std::unordered_map<std::string, std::string> positions;
  std::unordered_map<std::string, std::string> legacyPositionIdToCanonical;
  std::unordered_set<std::string> usedPositionUuids;
  std::unordered_set<std::string> usedSymbolUuids;

  auto reserveCanonicalPositionUuid = [&](const std::string &candidate,
                                          const std::string &seedBase) {
    std::string out = CanonicalizeUuid(candidate);
    if (out.empty() || usedPositionUuids.contains(out)) {
      int suffix = 0;
      do {
        out =
            DeriveDeterministicUuid(seedBase + "#" + std::to_string(suffix++));
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
    LogLegacyPositionUuidWarning("MVR export converted legacy Position uuid '" +
                                 rawUuid + "' to canonical '" + generated +
                                 "' (name='" + positions[generated] + "')");
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
      if (existing != positions.end() && !nameHint.empty() &&
          existing->second != nameHint)
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
        // Refresh the stored name so Hang Position edits are preserved on
        // export.
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

    std::string newUuid =
        reserveCanonicalPositionUuid({}, "mvr:position:name:" + nameHint);
    positions[newUuid] = nameHint;
    positionByName[nameHint] = newUuid;
    if (!positionId.empty()) {
      LogLegacyPositionUuidWarning(
          "MVR export normalized legacy Position uuid '" + positionId +
          "' -> '" + newUuid + "' (name='" + nameHint + "')");
    }
  };

  for (const auto &[uid, fixture] : scene.fixtures)
    ensurePositionEntry(fixture.position, fixture.positionName);
  for (const auto &[uid, truss] : scene.trusses)
    ensurePositionEntry(truss.position, truss.positionName);
  for (const auto &[uid, support] : scene.supports)
    ensurePositionEntry(support.position, support.positionName);

  auto resolvePositionReference =
      [&](const std::string &positionId,
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
              wxString::Format("MVR export remapped non-canonical Position "
                               "'%s' to '%s' by name '%s'",
                               positionId.c_str(), byName->second.c_str(),
                               nameHint.c_str())
                  .ToStdString());
        }
        return byName->second;
      }
    }
    return {};
  };

  wxFileOutputStream output(filePath);
  auto failExport = [&](const std::string &operation,
                        const std::string &entryName,
                        const std::string &sourcePath,
                        const std::string &reason) {
    std::ostringstream message;
    message << "MVR export failed during " << operation;
    if (!entryName.empty())
      message << " entry='" << entryName << "'";
    if (!sourcePath.empty())
      message << " source='" << sourcePath << "'";
    message << ": " << reason;
    m_exportWarnings.push_back(message.str());
    Logger::Instance().Log(Logger::Level::Error, message.str());
    return false;
  };
  if (!output.IsOk())
    return failExport("OpenOutput", {}, filePath, "could not open output file");

  wxZipOutputStream zip(output);

  std::vector<ResourceEntry> resourceEntries;
  std::unordered_map<std::string, std::string> sourceToArchivePath;
  std::unordered_map<std::string, std::string> physicalPatchArchiveByKey;
  std::unordered_map<std::string, std::string> gdtfArchiveByObjectUuid;
  std::unordered_map<std::string, GdtfOverrides> gdtfOverrides;
  std::unordered_map<std::string, std::string> trussArchiveByTypeKey;
  std::unordered_map<std::string, std::string> trussExportTypeKeyByTypeKey;
  std::unordered_map<std::string, std::string> trussInstanceToTypeKey;
  std::unordered_map<std::string, std::string> primitiveSourceByToken;
  std::unordered_set<std::string> reservedArchivePaths;
  const std::string primitiveTempDir = CreateTempDir();

  auto normalizeSourcePath = [&](const std::string &rawPath) {
    fs::path src = PathUtils::PathFromUtf8(rawPath);
    if (src.is_relative() && !scene.basePath.empty())
      src = PathUtils::PathFromUtf8(scene.basePath) / src;
    std::error_code ec;
    fs::path weak = fs::weakly_canonical(src, ec);
    return ec ? fs::absolute(src).generic_string() : weak.generic_string();
  };

  auto findSceneResourceByFileName =
      [&](const std::string &rawSource) -> std::string {
    if (rawSource.empty() || scene.basePath.empty())
      return {};

    const fs::path sourcePath = PathUtils::PathFromUtf8(rawSource);
    const fs::path requestedFileName = sourcePath.filename();
    if (requestedFileName.empty())
      return {};

    const fs::path basePath = PathUtils::PathFromUtf8(scene.basePath);
    std::error_code ec;
    if (!fs::exists(basePath, ec) || ec)
      return {};

    const std::string requestedLower =
        ToLowerAscii(requestedFileName.generic_string());
    for (const auto &entry : fs::directory_iterator(basePath, ec)) {
      if (ec)
        break;
      std::error_code regularEc;
      if (!entry.is_regular_file(regularEc) || regularEc)
        continue;
      if (ToLowerAscii(entry.path().filename().generic_string()) ==
          requestedLower) {
        return entry.path().generic_string();
      }
    }
    return {};
  };

  auto resolveExistingResourceSourcePath =
      [&](const std::string &rawSource) -> std::string {
    if (rawSource.empty())
      return {};

    std::string normalizedSource = normalizeSourcePath(rawSource);
    std::error_code sourceExistsEc;
    if (fs::exists(PathUtils::PathFromUtf8(normalizedSource), sourceExistsEc) &&
        !sourceExistsEc) {
      return normalizedSource;
    }

    const std::string sceneResourceSource =
        findSceneResourceByFileName(rawSource);
    if (!sceneResourceSource.empty()) {
      normalizedSource = normalizeSourcePath(sceneResourceSource);
      Logger::Instance().Log(
          Logger::Level::Info,
          "MVR export resolved packaged resource '" + rawSource +
              "' by filename in scene resources: " + normalizedSource);
      return normalizedSource;
    }

    return {};
  };

  auto registerResource = [&](const std::string &rawSource,
                              const std::string &preferredArchivePath,
                              bool allowReuseBySource = true) -> std::string {
    if (rawSource.empty())
      return {};
    std::string normalizedSource = resolveExistingResourceSourcePath(rawSource);
    if (normalizedSource.empty())
      normalizedSource = normalizeSourcePath(rawSource);
    auto srcIt = sourceToArchivePath.find(normalizedSource);
    if (allowReuseBySource && srcIt != sourceToArchivePath.end())
      return srcIt->second;

    std::string archivePath =
        EnsureUniqueArchivePath(preferredArchivePath, reservedArchivePaths);
    if (allowReuseBySource)
      sourceToArchivePath[normalizedSource] = archivePath;
    resourceEntries.push_back({fs::path(normalizedSource), archivePath});
    return archivePath;
  };

  auto registerGdtfResource =
      [&](const std::string &objectUuid, const std::string &rawGdtfPath,
          const std::string &preferredName, bool allowReuseBySource = true,
          bool usePreferredDerivativeName = false) -> std::string {
    if (rawGdtfPath.empty())
      return {};

    std::string resolvedGdtfPath = resolveExistingResourceSourcePath(rawGdtfPath);
    if (resolvedGdtfPath.empty()) {
      resolvedGdtfPath = ResolveFallbackFixtureGdtfPath();
      if (!resolvedGdtfPath.empty()) {
        Logger::Instance().Log(
            Logger::Level::Warn,
            "MVR export could not resolve fixture GDTF '" + rawGdtfPath +
                "'. Using fallback '" +
                fs::path(resolvedGdtfPath).filename().generic_string() + "'.");
      }
    }
    const std::string gdtfSourceForExport =
        resolvedGdtfPath.empty() ? rawGdtfPath : resolvedGdtfPath;

    std::string fileName = preferredName;
    if (!usePreferredDerivativeName &&
        ToLowerAscii(PathUtils::PathFromUtf8(rawGdtfPath).extension().string()) ==
            ".gdtf" &&
        !GdtfDictionary::IsPerastageNamedGdtfFile(rawGdtfPath)) {
      fileName =
          GdtfDictionary::BuildPerastageCanonicalGdtfFileName(gdtfSourceForExport);
    }
    if (fileName.empty())
      fileName = SanitizeArchiveFileName(rawGdtfPath, "fixture.gdtf");
    std::string archivePath =
        registerResource(gdtfSourceForExport, fileName, allowReuseBySource);
    if (!objectUuid.empty() && !archivePath.empty())
      gdtfArchiveByObjectUuid[objectUuid] = archivePath;
    return archivePath;
  };

  auto registerModelTextureDependencies = [&](const std::string
                                                  &rawModelSource) {
    if (rawModelSource.empty())
      return;
    const std::string normalizedModelPath = normalizeSourcePath(rawModelSource);
    const fs::path modelPath(normalizedModelPath);
    std::string ext = ToLowerAscii(modelPath.extension().string());
    if (ext == ".3ds") {
      const std::vector<std::string> textureRefs =
          Collect3dsTextureReferences(modelPath);
      for (const std::string &textureRef : textureRefs) {
        fs::path texturePath;
        if (!ResolveTextureDependencyPath(modelPath, textureRef, texturePath))
          continue;

        std::string preferredTextureName = SanitizeArchiveFileName(
            textureRef, texturePath.filename().generic_string());
        registerResource(texturePath.generic_string(), preferredTextureName);
      }
      return;
    }

    if (ext == ".gltf" || ext == ".glb") {
      const std::vector<std::string> textureRefs =
          CollectGltfTextureReferences(modelPath);
      const fs::path modelDir =
          modelPath.has_parent_path() ? modelPath.parent_path() : fs::path();
      for (const std::string &textureRef : textureRefs) {
        const std::string trimmedRef = TrimAscii(textureRef);
        if (trimmedRef.empty())
          continue;
        const fs::path candidate =
            modelDir / PathUtils::PathFromUtf8(trimmedRef);
        if (!fs::exists(candidate))
          continue;

        std::string preferredTextureName = SanitizeArchiveFileName(
            trimmedRef, candidate.filename().generic_string());
        registerResource(candidate.generic_string(), preferredTextureName);
      }
    }
  };

  auto registerModelResource =
      [&](const std::string &rawModelSource,
                                   const std::string &fallbackArchiveName) -> std::string {
    std::string archivePath = registerResource(
        rawModelSource,
        SanitizeArchiveFileName(rawModelSource, fallbackArchiveName));
    registerModelTextureDependencies(rawModelSource);
    return archivePath;
  };

  auto registerPrimitiveModelResource =
      [&](const std::string &modelRef,
                                            const std::string &objectUuid) -> std::string {
    std::string primitiveToken;
    if (!mvr::ResolvePrimitiveTokenFromModelRef(modelRef, primitiveToken))
      return {};
    const std::string normalizedModelRef = ToLowerAscii(TrimAscii(modelRef));

    auto convertCylinderTokenMillimetersToMeters =
        [&](const std::string &token) {
      std::string normalized = ToLowerAscii(TrimAscii(token));
      if (normalized.rfind("primitive:cylinder", 0) != 0)
        return normalized;
      const size_t separator = normalized.find(';');
          if (separator == std::string::npos ||
              separator + 1 >= normalized.size())
        return normalized;

      std::vector<std::string> fields;
      std::stringstream stream(normalized.substr(separator + 1));
      std::string field;
      bool changed = false;
      while (std::getline(stream, field, ';')) {
        const size_t equalPos = field.find('=');
        if (equalPos == std::string::npos) {
          fields.push_back(field);
          continue;
        }
        const std::string key = field.substr(0, equalPos);
        const std::string value = field.substr(equalPos + 1);
        if (key == "top" || key == "bottom" || key == "height") {
          try {
            const float parsed = std::stof(value);
            const float meters = parsed / 1000.0f;
            fields.push_back(key + "=" + std::to_string(meters));
            changed = true;
          } catch (...) {
            fields.push_back(field);
          }
        } else {
          fields.push_back(field);
        }
      }

      if (!changed)
        return normalized;
      std::string out = "primitive:cylinder";
      for (const auto &entry : fields) {
        if (!entry.empty())
          out += ";" + entry;
      }
      return out;
    };

    const std::string primitiveKeyRaw =
        normalizedModelRef.empty() ? primitiveToken : normalizedModelRef;
    const std::string primitiveKey =
        convertCylinderTokenMillimetersToMeters(primitiveKeyRaw);

    auto sourceIt = primitiveSourceByToken.find(primitiveKey);
    std::string sourcePath;
    if (sourceIt != primitiveSourceByToken.end()) {
      sourcePath = sourceIt->second;
    } else {
      std::string primitiveLabel = primitiveToken;
      const size_t colonPos = primitiveLabel.find(':');
      if (colonPos != std::string::npos && colonPos + 1 < primitiveLabel.size())
        primitiveLabel = primitiveLabel.substr(colonPos + 1);
      for (char &ch : primitiveLabel) {
        if (!std::isalnum(static_cast<unsigned char>(ch)))
          ch = '_';
      }
      if (primitiveLabel.empty())
        primitiveLabel = "shape";
      const std::size_t primitiveHash = std::hash<std::string>{}(primitiveKey);
      const std::string tempFileName =
          wxString::Format("primitive_%s_%zx.glb", primitiveLabel.c_str(),
                           primitiveHash)
                                           .ToStdString();
      fs::path outputPath = PathUtils::PathFromUtf8(primitiveTempDir) /
                            PathUtils::PathFromUtf8(tempFileName);
      if (!mvr::WritePrimitiveModelForToken(primitiveKey,
                                            outputPath.generic_string()))
        return {};
      sourcePath = outputPath.generic_string();
      primitiveSourceByToken[primitiveKey] = sourcePath;
    }

    const std::string preferredArchivePath =
        mvr::PrimitiveArchivePathForToken(primitiveToken, objectUuid);
    return registerResource(sourcePath, preferredArchivePath);
  };

  auto assignIds = [&]() {
    int nextNumericId = 1;
    std::unordered_set<int> usedIds;

    auto reserveId = [&](int candidate) {
      if (candidate > 0)
        return usedIds.insert(candidate).second;
      return false;
    };
    for (const auto &[uid, f] : scene.fixtures) {
      reserveId(ResolveFixtureExportId(f).numeric);
    }

    auto allocId = [&]() {
      while (usedIds.contains(nextNumericId))
        ++nextNumericId;
      usedIds.insert(nextNumericId);
      return nextNumericId++;
    };

    std::unordered_map<std::string, std::pair<std::string, int>> result;
    for (const auto &[uid, f] : scene.fixtures) {
      FixtureExportId fixtureId = ResolveFixtureExportId(f);
      if (fixtureId.numeric <= 0) {
        fixtureId.numeric = allocId();
        fixtureId.text = std::to_string(fixtureId.numeric);
      }
      result[uid] = {fixtureId.text, fixtureId.numeric};
    }

    auto sortedKeys = [](const auto &map) {
      std::vector<std::string> keys;
      keys.reserve(map.size());
      for (const auto &[uid, value] : map) {
        (void)value;
        keys.push_back(uid);
      }
      std::sort(keys.begin(), keys.end());
      return keys;
    };

    for (const std::string &uid : sortedKeys(scene.trusses)) {
      const Truss &t = scene.trusses.at(uid);
      int numeric = allocId();
      std::string stringId = TrimAscii(t.name);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }

    for (const std::string &uid : sortedKeys(scene.supports)) {
      const Support &s = scene.supports.at(uid);
      int numeric = allocId();
      std::string stringId = TrimAscii(s.name);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }

    for (const std::string &uid : sortedKeys(scene.sceneObjects)) {
      const SceneObject &obj = scene.sceneObjects.at(uid);
      int numeric = 0;
      if (obj.fixtureIdNumeric > 0 && reserveId(obj.fixtureIdNumeric))
        numeric = obj.fixtureIdNumeric;
      else
        numeric = allocId();
      std::string stringId = TrimAscii(obj.fixtureIdText);
      if (stringId.empty())
        stringId = std::to_string(numeric);
      result[uid] = {stringId, numeric};
    }
    return result;
  };

  const auto assignedIds = assignIds();
  const auto assignedUnitNumbers =
      BuildFixtureUnitNumbersForExport(scene.fixtures);

  tinyxml2::XMLDocument doc;
  doc.InsertEndChild(
      doc.NewDeclaration("xml version=\"1.0\" encoding=\"UTF-8\""));

  auto appendPlaceholderCubeGeometry = [&](tinyxml2::XMLElement *owner,
                                          const std::string &objectUuid,
                                          const char *nodeName) -> bool {
    const std::string modelArchivePath =
        registerPrimitiveModelResource("primitive:cube", objectUuid);
    if (modelArchivePath.empty()) {
      Logger::Instance().Log(
          Logger::Level::Warn,
                             std::string("MVR export could not create placeholder geometry for ") +
                                 nodeName + " uuid=" + objectUuid);
      return false;
    }

    tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
    tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
    g3d->SetAttribute("fileName", modelArchivePath.c_str());
    constexpr float kPlaceholderCubeSizeMeters = 0.1f;
    Matrix placeholderMatrix = MatrixUtils::Identity();
    placeholderMatrix.u = {kPlaceholderCubeSizeMeters, 0.0f, 0.0f};
    placeholderMatrix.v = {0.0f, kPlaceholderCubeSizeMeters, 0.0f};
    placeholderMatrix.w = {0.0f, 0.0f, kPlaceholderCubeSizeMeters};
    tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
    geoMatrix->SetText(MatrixUtils::FormatMatrix(placeholderMatrix).c_str());
    g3d->InsertEndChild(geoMatrix);
    geos->InsertEndChild(g3d);
    owner->InsertEndChild(geos);
    Logger::Instance().Log(
        Logger::Level::Warn,
                           std::string("MVR export added placeholder cube geometry for ") +
                               nodeName + " uuid=" + objectUuid);
    return true;
  };

  tinyxml2::XMLElement *root = doc.NewElement("GeneralSceneDescription");
  root->SetAttribute("verMajor", 1);
  root->SetAttribute("verMinor", 6);
  root->SetAttribute("provider", kMvrProvider);
  root->SetAttribute("providerVersion", app::kVersion);
  doc.InsertEndChild(root);

  if (HasLayerAppearanceMetadata(scene)) {
    tinyxml2::XMLElement *rootPerastageData =
        FindOrCreatePerastageDataNode(doc, root);
    AppendLayerAppearanceMetadata(doc, rootPerastageData, scene);
  }

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
  const std::unordered_set<std::string> referencedSymdefUuids =
      CollectReferencedSymdefUuids(scene, options);
  for (const auto &[uuid, file] : scene.symdefFiles) {
    if (!referencedSymdefUuids.contains(uuid))
      continue;
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
      fallback.transform = (matIt != scene.symdefMatrices.end())
                               ? matIt->second
                               : MatrixUtils::Identity();
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
  std::unordered_set<std::string> usedObjectExportUuids;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const std::string exportUuid = CanonicalizeUuid(uuid);
    if (!exportUuid.empty())
      usedObjectExportUuids.insert(exportUuid);
  }
  for (const auto &[uuid, support] : scene.supports) {
    const std::string exportUuid = CanonicalizeUuid(uuid);
    if (!exportUuid.empty())
      usedObjectExportUuids.insert(exportUuid);
  }
  for (const auto &[uuid, object] : scene.sceneObjects) {
    const std::string exportUuid = CanonicalizeUuid(uuid);
    if (!exportUuid.empty())
      usedObjectExportUuids.insert(exportUuid);
  }
  for (const auto &[uuid, group] : scene.groupObjects) {
    const std::string exportUuid = CanonicalizeUuid(uuid);
    if (!exportUuid.empty())
      usedObjectExportUuids.insert(exportUuid);
  }
  std::unordered_map<std::string, std::string> trussExportUuids;
  for (const auto &[uuid, truss] : scene.trusses) {
    std::string exportUuid = CanonicalizeUuid(truss.uuid);
    const std::string seed = "mvr-export-truss:" + truss.uuid + ":" +
                             truss.name + ":" +
                             MatrixUtils::FormatMatrix(truss.transform);
    const bool needsRepair =
        exportUuid.empty() || usedObjectExportUuids.contains(exportUuid);
    if (needsRepair) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format(
              "Truss '%s' has missing, non-canonical, or conflicting UUID "
              "'%s'. Applying deterministic fallback UUID for export.",
              truss.name.c_str(), truss.uuid.c_str())
              .ToStdString());
      for (int suffix = 0;; ++suffix) {
        const std::string candidate =
            DeriveDeterministicUuid(seed + "#" + std::to_string(suffix));
        if (!candidate.empty() && !usedObjectExportUuids.contains(candidate)) {
          exportUuid = candidate;
          break;
        }
      }
    }
    usedObjectExportUuids.insert(exportUuid);
    trussExportUuids[uuid] = exportUuid;
  }
  tinyxml2::XMLElement *trussInfoMap = doc.NewElement("TrussInfoMap");
  tinyxml2::XMLElement *hoistInfoMap = doc.NewElement("HoistInfoMap");
  std::map<std::string, FixtureTypeInfoExport> fixtureTypeMetadata;

  int exportedRealFixtureGdtfCount = 0;
  int exportedDummyFixtureGdtfCount = 0;
  int preservedOriginalFixtureGdtfRecoveries = 0;
  std::vector<std::string> dummyFallbackFixtureExamples;

  auto exportFixture = [&](tinyxml2::XMLElement *parent, const Fixture &f) {
    tinyxml2::XMLElement *fe = doc.NewElement("Fixture");

    std::string stableUuid = CanonicalizeUuid(f.uuid);
    const std::string seed = "mvr-export-fixture:" + f.uuid + ":" +
                             f.instanceName + ":" +
                             MatrixUtils::FormatMatrix(f.transform);
    if (stableUuid.empty()) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format("Fixture '%s' has non-canonical UUID '%s'. Applying "
                           "deterministic fallback UUID for export.",
              f.instanceName.c_str(), f.uuid.c_str())
              .ToStdString());
      stableUuid = DeriveDeterministicUuid(seed);
    }
    if (usedFixtureUuids.contains(stableUuid)) {
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format("Fixture UUID collision detected during export for "
                           "'%s' (uuid=%s). Applying controlled fallback UUID.",
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
    const std::string fixtureExportName = TrimAscii(f.instanceName).empty()
                                              ? "Fixture"
                                              : TrimAscii(f.instanceName);
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
    auto idIt = assignedIds.find(f.uuid);
    FixtureExportId fixtureExportId = ResolveFixtureExportId(f);
    if (fixtureExportId.numeric <= 0 && idIt != assignedIds.end())
      fixtureExportId = {idIt->second.first, idIt->second.second};
    if (fixtureExportId.numeric <= 0) {
      fixtureExportId.numeric = 1;
      fixtureExportId.text = "1";
    }
    if (fixtureExportId.text.empty())
      fixtureExportId.text = std::to_string(fixtureExportId.numeric);
    std::string fixtureSourceGdtf = f.gdtfSpec;
    if (fixtureSourceGdtf.empty() && !f.originalMvrGdtfSpec.empty()) {
      fixtureSourceGdtf = f.originalMvrGdtfSpec;
      ++preservedOriginalFixtureGdtfRecoveries;
      Logger::Instance().Log(
          Logger::Level::Warn,
          wxString::Format(
              "Fixture '%s' (uuid=%s) had an empty current GDTF. Recovering "
              "preserved original MVR GDTFSpec '%s' for export.",
              fixtureExportName.c_str(), f.uuid.c_str(),
              fixtureSourceGdtf.c_str())
              .ToStdString());
    }
    bool usedDummyFallbackForFixture = false;
    if (fixtureSourceGdtf.empty()) {
      fixtureSourceGdtf = ResolveFallbackFixtureGdtfPath();
      const std::string fallbackHint =
          std::string(kDummyFallbackFixtureGdtfFileName) +
          " (legacy: " + kLegacyFallbackFixtureGdtfFileName + ")";
      if (fixtureSourceGdtf.empty()) {
        std::ostringstream msg;
        msg << "Fixture '" << fixtureExportName << "' (uuid=" << f.uuid
            << ") has no GDTF and fallback '" << fallbackHint
            << "' is not available.";
        Logger::Instance().Log(Logger::Level::Warn, msg.str());
      } else {
        std::ostringstream msg;
        msg << "Fixture '" << fixtureExportName << "' (uuid=" << f.uuid
            << ") has no GDTF. Using fallback '"
            << fs::path(fixtureSourceGdtf).filename().string()
            << "' for MVR export.";
        Logger::Instance().Log(Logger::Level::Info, msg.str());
        usedDummyFallbackForFixture = true;
        if (dummyFallbackFixtureExamples.size() < 5)
          dummyFallbackFixtureExamples.push_back(fixtureExportName +
                                                 " (uuid=" + f.uuid + ")");
      }
    }
    if (!fixtureSourceGdtf.empty()) {
      const std::string resolvedFixtureSource =
          resolveExistingResourceSourcePath(fixtureSourceGdtf);
      if (!resolvedFixtureSource.empty()) {
        fixtureSourceGdtf = resolvedFixtureSource;
      } else if (!usedDummyFallbackForFixture) {
        const std::string fallbackGdtf = ResolveFallbackFixtureGdtfPath();
        if (!fallbackGdtf.empty()) {
          Logger::Instance().Log(
              Logger::Level::Warn,
              "Fixture '" + fixtureExportName + "' (uuid=" + f.uuid +
                  ") references missing GDTF '" + fixtureSourceGdtf +
                  "'. Using fallback '" +
                  fs::path(fallbackGdtf).filename().generic_string() +
                  "' for MVR export.");
          fixtureSourceGdtf = fallbackGdtf;
          usedDummyFallbackForFixture = true;
          if (dummyFallbackFixtureExamples.size() < 5)
            dummyFallbackFixtureExamples.push_back(fixtureExportName +
                                                   " (uuid=" + f.uuid + ")");
        }
      }
    }
    if (usedDummyFallbackForFixture)
      ++exportedDummyFixtureGdtfCount;
    else if (!fixtureSourceGdtf.empty())
      ++exportedRealFixtureGdtfCount;
    std::string fixtureName =
        SanitizeArchiveFileName(fixtureSourceGdtf, "fixture.gdtf");
    GdtfOverrides fixtureOverrides;
    const bool needsPhysicalPatch =
        FixtureNeedsPhysicalGdtfPatch(f, fixtureSourceGdtf, fixtureOverrides);
    if (needsPhysicalPatch) {
      const fs::path archiveName(fixtureName);
      const std::string stem = archiveName.stem().generic_string();
      const std::string extension = archiveName.extension().generic_string();
      fixtureName = SanitizeArchiveFileName(
          stem + "_physical_" + f.uuid + extension, "fixture_physical.gdtf");
    }
    std::string fixtureGdtfArchivePath;
    if (needsPhysicalPatch) {
      std::ostringstream patchKey;
      patchKey << normalizeSourcePath(fixtureSourceGdtf) << '|'
               << (fixtureOverrides.hasWeightKg ? fixtureOverrides.weightKg
                                                : -1.0f)
               << '|'
               << (fixtureOverrides.hasPowerW ? fixtureOverrides.powerW
                                              : -1.0f);
      auto patchIt = physicalPatchArchiveByKey.find(patchKey.str());
      if (patchIt != physicalPatchArchiveByKey.end()) {
        fixtureGdtfArchivePath = patchIt->second;
        if (!f.uuid.empty())
          gdtfArchiveByObjectUuid[f.uuid] = fixtureGdtfArchivePath;
      } else {
        fixtureGdtfArchivePath =
            registerGdtfResource(f.uuid, fixtureSourceGdtf, fixtureName, false);
        physicalPatchArchiveByKey[patchKey.str()] = fixtureGdtfArchivePath;
        gdtfOverrides[fixtureGdtfArchivePath] = fixtureOverrides;
      }
    } else {
      fixtureGdtfArchivePath =
          registerGdtfResource(f.uuid, fixtureSourceGdtf, fixtureName);
    }
    MergeFixtureTypeInfoExport(fixtureTypeMetadata, f,
                                   fixtureGdtfArchivePath);

    const Matrix fixtureMatrixToWrite =
        f.parentGroupUuid.empty()
            ? f.transform
            : (f.hasLocalTransform ? f.localTransform : f.transform);
    std::string mstr = MatrixUtils::FormatMatrix(fixtureMatrixToWrite);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    fe->InsertEndChild(mat);

    addStr("GDTFSpec", fixtureGdtfArchivePath);
    // Keep fixture GDTF payloads byte-preserved unless the user intentionally
    // edits type-level physical properties that must be exported through GDTF.
    if (!fixtureGdtfArchivePath.empty())
      addStr("GDTFMode", f.gdtfMode.empty() ? "Default" : f.gdtfMode);
    if (!f.position.empty() || !f.positionName.empty())
      addStr("Position", resolvePositionReference(f.position, f.positionName));
    addStr("FixtureID", fixtureExportId.text);
    addInt("FixtureIDNumeric", fixtureExportId.numeric);
    auto unitIt = assignedUnitNumbers.find(f.uuid);
    addInt("UnitNumber",
           unitIt != assignedUnitNumbers.end() ? unitIt->second : f.unitNumber);

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
            wxString::Format("Skipping invalid DMX patch for fixture '%s' "
                             "(uuid=%s): '%s' (expected Universe.Address with "
                             "universe >= 1 and address in [1,512])",
                             f.instanceName.c_str(), f.uuid.c_str(),
                             trimmedAddress.c_str())
                .ToStdString());
      }
    }

    if (!f.mvrFixtureColorHex.empty())
      addStr("Color", HexToCie(f.mvrFixtureColorHex));

    parent->InsertEndChild(fe);
  };

  auto exportTruss = [&](tinyxml2::XMLElement *parent, const Truss &t) {
    tinyxml2::XMLElement *te = doc.NewElement("Truss");
    const auto exportUuidIt = trussExportUuids.find(t.uuid);
    const std::string exportedTrussUuid =
        exportUuidIt != trussExportUuids.end()
                                             ? exportUuidIt->second
            : DeriveDeterministicUuid("mvr-export-truss:" + t.uuid + ":" +
                                      t.name);
    te->SetAttribute("uuid", exportedTrussUuid.c_str());
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
    int fixtureNumericId =
        (idIt != assignedIds.end()) ? idIt->second.second : 0;
    if (fixtureNumericId <= 0)
      fixtureNumericId = 1;
    std::string fixtureId = std::to_string(fixtureNumericId);

    std::string trussTypeKey = BuildTrussTypeKey(t);
    std::string trussGdtfArchivePath;
    auto trussArchiveIt = trussArchiveByTypeKey.find(trussTypeKey);
    if (trussArchiveIt != trussArchiveByTypeKey.end()) {
      trussGdtfArchivePath = trussArchiveIt->second;
      if (!exportedTrussUuid.empty())
        gdtfArchiveByObjectUuid[exportedTrussUuid] = trussGdtfArchivePath;
    } else {
      std::string trussSourceGdtf = t.gdtfSpec;
      if (trussSourceGdtf.empty() &&
          fs::path(t.modelFile).extension() == ".gdtf")
        trussSourceGdtf = t.modelFile;

      const bool importedFromMvrGeometry =
          t.sourceRepresentation ==
              Truss::GeometryRepresentation::SymbolSymdef ||
          t.sourceRepresentation == Truss::GeometryRepresentation::Geometry3D;
      if (trussSourceGdtf.empty() && !importedFromMvrGeometry) {
        fs::path tempPath =
            fs::temp_directory_path() /
            ("perastage-truss-export-" +
             (t.uuid.empty() ? std::string("truss") : t.uuid) + ".gdtf");
        std::string conversionError;
        if (BuildTrussGdtfFromInstance(t, tempPath, &conversionError))
          trussSourceGdtf = tempPath.string();
      }

      std::string trussPreferredName = BuildTrussGdtfArchiveName(t);
      trussGdtfArchivePath = registerGdtfResource(
          exportedTrussUuid, trussSourceGdtf, trussPreferredName, true, true);
      if (!trussGdtfArchivePath.empty())
        trussArchiveByTypeKey[trussTypeKey] = trussGdtfArchivePath;
    }
    const std::string exportTrussTypeKey =
        BuildExportTrussTypeKey(t, trussGdtfArchivePath);
    if (!trussTypeKey.empty()) {
      trussExportTypeKeyByTypeKey[trussTypeKey] = exportTrussTypeKey;
      trussInstanceToTypeKey[exportedTrussUuid] = exportTrussTypeKey;
    }

    if (!trussGdtfArchivePath.empty()) {
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

    const bool writeWorldTransform = t.parentGroupUuid.empty();
    const Matrix matrixToWrite =
        writeWorldTransform
            ? t.transform
            : (t.hasLocalTransform ? t.localTransform : t.transform);
    std::string mstr = MatrixUtils::FormatMatrix(matrixToWrite);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    te->InsertEndChild(mat);

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
      if (t.sourceRepresentation ==
              Truss::GeometryRepresentation::SymbolSymdef &&
          !t.sourceSymdefUuid.empty()) {
        const bool flattenSymbol =
            options.trussGeometryExportMode ==
            MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
        tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
        if (flattenSymbol) {
          std::vector<SymdefGeometry> geometries;
          auto geoIt = scene.symdefGeometries.find(t.sourceSymdefUuid);
          if (geoIt != scene.symdefGeometries.end())
            geometries = geoIt->second;
          auto fileIt = scene.symdefFiles.find(t.sourceSymdefUuid);
          if (geometries.empty() && fileIt != scene.symdefFiles.end() &&
              !fileIt->second.empty()) {
            SymdefGeometry fallback;
            fallback.file = fileIt->second;
            auto matIt = scene.symdefMatrices.find(t.sourceSymdefUuid);
            fallback.transform = (matIt != scene.symdefMatrices.end())
                                     ? matIt->second
                                     : MatrixUtils::Identity();
            auto typeIt = scene.symdefTypes.find(t.sourceSymdefUuid);
            if (typeIt != scene.symdefTypes.end())
              fallback.geometryType = typeIt->second;
            geometries.push_back(std::move(fallback));
          }

          for (const SymdefGeometry &geo : geometries) {
            if (geo.file.empty())
              continue;
            tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
            const std::string archivePath =
                registerModelResource(geo.file, "truss.3ds");
            if (archivePath.empty())
              continue;
            g3d->SetAttribute("fileName", archivePath.c_str());
            if (!geo.geometryType.empty())
              g3d->SetAttribute("geometryType", geo.geometryType.c_str());
            const Matrix composedMatrix =
                MatrixUtils::Multiply(t.sourceSymbolMatrix, geo.transform);
            tinyxml2::XMLElement *geoMat = doc.NewElement("Matrix");
            geoMat->SetText(MatrixUtils::FormatMatrix(composedMatrix).c_str());
            g3d->InsertEndChild(geoMat);
            geos->InsertEndChild(g3d);
          }

          if (geos->FirstChildElement("Geometry3D")) {
            te->InsertEndChild(geos);
            Logger::Instance().Log(
                Logger::Level::Info,
                wxString::Format("MVR export truss flattened Symbol/Symdef "
                                 "geometry uuid=%s symdef=%s",
                                 t.uuid.c_str(), t.sourceSymdefUuid.c_str())
                    .ToStdString());
          } else {
            m_exportWarnings.push_back(
                "MVR export could not resolve truss Symdef '" +
                t.sourceSymdefUuid +
                "' for direct Geometry3D export; preserving Symbol/Symdef "
                "representation.");
            tinyxml2::XMLElement *sym = doc.NewElement("Symbol");
            const std::string symbolMatrixText =
                MatrixUtils::FormatMatrix(t.sourceSymbolMatrix);
            const std::string symbolUuid = ResolveExportSymbolUuid(
                t.sourceSymbolUuid, t.uuid, t.sourceSymdefUuid,
                "mvr:symbol:truss:" + t.uuid + ":" + t.sourceSymdefUuid + ":" +
                    symbolMatrixText + ":0",
                usedSymbolUuids, &m_exportWarnings, "Truss uuid " + t.uuid);
            sym->SetAttribute("uuid", symbolUuid.c_str());
            sym->SetAttribute("symdef", t.sourceSymdefUuid.c_str());
            tinyxml2::XMLElement *symMat = doc.NewElement("Matrix");
            symMat->SetText(symbolMatrixText.c_str());
            sym->InsertEndChild(symMat);
            geos->InsertEndChild(sym);
            te->InsertEndChild(geos);
          }
        } else {
          tinyxml2::XMLElement *sym = doc.NewElement("Symbol");
          const std::string symbolMatrixText =
              MatrixUtils::FormatMatrix(t.sourceSymbolMatrix);
          const std::string symbolUuid = ResolveExportSymbolUuid(
              t.sourceSymbolUuid, t.uuid, t.sourceSymdefUuid,
              "mvr:symbol:truss:" + t.uuid + ":" + t.sourceSymdefUuid + ":" +
                  symbolMatrixText + ":0",
              usedSymbolUuids, &m_exportWarnings, "Truss uuid " + t.uuid);
          sym->SetAttribute("uuid", symbolUuid.c_str());
          sym->SetAttribute("symdef", t.sourceSymdefUuid.c_str());
          tinyxml2::XMLElement *symMat = doc.NewElement("Matrix");
          symMat->SetText(symbolMatrixText.c_str());
          sym->InsertEndChild(symMat);
          geos->InsertEndChild(sym);
          te->InsertEndChild(geos);
          Logger::Instance().Log(
              Logger::Level::Info,
              wxString::Format("MVR export truss keeps Symbol/Symdef uuid=%s "
                               "symbol=%s symdef=%s",
                               t.uuid.c_str(), symbolUuid.c_str(),
                               t.sourceSymdefUuid.c_str())
                  .ToStdString());
        }
      } else if (!t.symbolFile.empty()) {
        std::string ext = fs::path(t.symbolFile).extension().string();
        std::transform(
            ext.begin(), ext.end(), ext.begin(),
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
          geoMat->SetText(
              MatrixUtils::FormatMatrix(t.sourceGeometryMatrix).c_str());
          g3d->InsertEndChild(geoMat);
          geos->InsertEndChild(g3d);
          te->InsertEndChild(geos);
          Logger::Instance().Log(
              Logger::Level::Info,
              wxString::Format(
                  "MVR export truss uses direct Geometry3D uuid=%s",
                               t.uuid.c_str())
                  .ToStdString());
        }
      }
    }
    if (!t.function.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("Function");
      e->SetText(t.function.c_str());
      te->InsertEndChild(e);
    }
    if (!trussGdtfArchivePath.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFSpec");
      e->SetText(trussGdtfArchivePath.c_str());
      te->InsertEndChild(e);

      tinyxml2::XMLElement *modeElement = doc.NewElement("GDTFMode");
      modeElement->SetText(t.gdtfMode.empty() ? "Default" : t.gdtfMode.c_str());
      te->InsertEndChild(modeElement);
    }
    addStr("FixtureID", fixtureId);
    addInt("FixtureIDNumeric", fixtureNumericId);
    addInt("UnitNumber", t.unitNumber);
    addInt("CustomId", t.customId);
    addInt("CustomIdType", t.customIdType);

    AppendTrussInfoMetadata(doc, trussInfoMap, t, exportedTrussUuid,
                            exportTrussTypeKey, trussGdtfArchivePath);

    parent->InsertEndChild(te);
  };

  auto exportSupport = [&](tinyxml2::XMLElement *parent, const Support &s) {
    tinyxml2::XMLElement *se = doc.NewElement("Support");
    se->SetAttribute("uuid", s.uuid.c_str());
    if (!s.name.empty())
      se->SetAttribute("name", s.name.c_str());

    const Matrix supportMatrixToWrite =
        s.parentGroupUuid.empty()
            ? s.transform
            : (s.hasLocalTransform ? s.localTransform : s.transform);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(MatrixUtils::FormatMatrix(supportMatrixToWrite).c_str());
    se->InsertEndChild(mat);

    const std::string supportPositionRef =
        resolvePositionReference(s.position, s.positionName);
    if (!supportPositionRef.empty()) {
      tinyxml2::XMLElement *position = doc.NewElement("Position");
      position->SetText(supportPositionRef.c_str());
      se->InsertEndChild(position);
    }

    tinyxml2::XMLElement *geos = nullptr;
    auto ensureGeometries = [&]() {
      if (!geos)
        geos = doc.NewElement("Geometries");
      return geos;
    };
    for (const auto &geo : s.geometries) {
      if (geo.modelFile.empty())
        continue;
      std::string modelArchivePath =
          registerModelResource(geo.modelFile, "support.3ds");
      if (modelArchivePath.empty())
        continue;
      tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
      g3d->SetAttribute("fileName", modelArchivePath.c_str());
      tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
      geoMatrix->SetText(MatrixUtils::FormatMatrix(geo.localTransform).c_str());
      g3d->InsertEndChild(geoMatrix);
      ensureGeometries()->InsertEndChild(g3d);
    }
    if (!geos && !s.modelFile.empty()) {
      std::string modelArchivePath =
          registerModelResource(s.modelFile, "support.3ds");
      if (!modelArchivePath.empty()) {
        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        g3d->SetAttribute("fileName", modelArchivePath.c_str());
        tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
        geoMatrix->SetText(
            MatrixUtils::FormatMatrix(MatrixUtils::Identity()).c_str());
        g3d->InsertEndChild(geoMatrix);
        ensureGeometries()->InsertEndChild(g3d);
      }
    }
    if (geos && geos->FirstChild()) {
      se->InsertEndChild(geos);
    } else {
      tinyxml2::XMLElement *emptyGeometries = doc.NewElement("Geometries");
      se->InsertEndChild(emptyGeometries);
      Logger::Instance().Log(
          Logger::Level::Warn,
                             "MVR export kept Support uuid=" + s.uuid +
                                 " with empty Geometries because no source geometry is available");
    }

    const std::string supportFunction =
        s.function.empty() ? NormalizeHoistFunction(s.hoistFunction)
                           : s.function;
    if (!supportFunction.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("Function");
      e->SetText(supportFunction.c_str());
      se->InsertEndChild(e);
    }
    tinyxml2::XMLElement *chainLength = doc.NewElement("ChainLength");
    chainLength->SetText(std::to_string(std::max(s.chainLength, 0.0f)).c_str());
    se->InsertEndChild(chainLength);

    std::string supportGdtfArchivePath =
        registerGdtfResource(s.uuid, s.gdtfSpec, "");
    if (!supportGdtfArchivePath.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFSpec");
      e->SetText(supportGdtfArchivePath.c_str());
      se->InsertEndChild(e);
    }
    if (!supportGdtfArchivePath.empty()) {
      tinyxml2::XMLElement *e = doc.NewElement("GDTFMode");
      e->SetText(s.gdtfMode.empty() ? "Default" : s.gdtfMode.c_str());
      se->InsertEndChild(e);
    }

    auto supportIdIt = assignedIds.find(s.uuid);
    int supportNumericId =
        (supportIdIt != assignedIds.end()) ? supportIdIt->second.second : 0;
    if (supportNumericId <= 0)
      supportNumericId = 1;
    tinyxml2::XMLElement *supportId = doc.NewElement("FixtureID");
    supportId->SetText(std::to_string(supportNumericId).c_str());
    se->InsertEndChild(supportId);
    tinyxml2::XMLElement *supportIdNumeric = doc.NewElement("FixtureIDNumeric");
    supportIdNumeric->SetText(std::to_string(supportNumericId).c_str());
    se->InsertEndChild(supportIdNumeric);

    if (ShouldExportSupportHoistInfo(s))
      AppendSupportHoistInfoMetadata(doc, hoistInfoMap, s);

    parent->InsertEndChild(se);
  };

  struct PrimitiveGeometryMapEntry {
    std::string sceneObjectUuid;
    std::string fileName;
    std::string perastageModelRef;
    size_t geometryIndex = 0;
  };
  std::vector<PrimitiveGeometryMapEntry> primitiveGeometryMapEntries;

  auto exportSceneObject = [&](tinyxml2::XMLElement *parent,
                               const SceneObject &obj) {
    struct CylinderTokenParams {
      float topRadiusMm = 0.5f;
      float bottomRadiusMm = 0.5f;
      float heightMm = 1.0f;
      char axis = 'y';
      bool hasExplicitDimensions = false;
    };

    auto parseCylinderTokenParams = [&](const std::string &modelRef,
                                        CylinderTokenParams &out) {
      std::string primitiveToken;
      if (!mvr::ResolvePrimitiveTokenFromModelRef(modelRef, primitiveToken))
        return false;
      std::string normalized = ToLowerAscii(TrimAscii(primitiveToken));
      if (normalized.rfind("primitive:cylinder", 0) != 0)
        return false;

      const auto parsePositive = [](const std::string &value, float fallback) {
        if (value.empty())
          return fallback;
        try {
          const float parsed = std::stof(value);
          if (std::isfinite(parsed) && parsed > 0.0f)
            return parsed;
        } catch (...) {
        }
        return fallback;
      };

      const size_t separator = normalized.find(';');
      if (separator == std::string::npos || separator + 1 >= normalized.size())
        return true;

      std::stringstream stream(normalized.substr(separator + 1));
      std::string field;
      while (std::getline(stream, field, ';')) {
        const size_t equalPos = field.find('=');
        if (equalPos == std::string::npos)
          continue;
        const std::string key = field.substr(0, equalPos);
        const std::string value = field.substr(equalPos + 1);
        if (key == "top") {
          out.topRadiusMm = parsePositive(value, out.topRadiusMm);
          out.hasExplicitDimensions = true;
        } else if (key == "bottom") {
          out.bottomRadiusMm = parsePositive(value, out.bottomRadiusMm);
          out.hasExplicitDimensions = true;
        } else if (key == "height") {
          out.heightMm = parsePositive(value, out.heightMm);
          out.hasExplicitDimensions = true;
        } else if (key == "axis") {
          if (value == "x" || value == "y" || value == "z")
            out.axis = value[0];
        }
      }

      return true;
    };

    Matrix objectMatrixToWrite =
        obj.parentGroupUuid.empty()
            ? obj.transform
            : (obj.hasLocalTransform ? obj.localTransform : obj.transform);

    tinyxml2::XMLElement *oe = doc.NewElement("SceneObject");
    oe->SetAttribute("uuid", obj.uuid.c_str());
    if (!obj.name.empty())
      oe->SetAttribute("name", obj.name.c_str());

    std::string mstr = MatrixUtils::FormatMatrix(objectMatrixToWrite);
    tinyxml2::XMLElement *mat = doc.NewElement("Matrix");
    mat->SetText(mstr.c_str());
    oe->InsertEndChild(mat);

    if (!obj.geometries.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      size_t geometryIndex = 0;
      for (const auto &geo : obj.geometries) {
        if (geo.modelFile.empty())
          continue;

        tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
        std::string modelRef = geo.modelFile;
        std::string modelArchivePath =
            registerPrimitiveModelResource(modelRef, obj.uuid);
        if (modelArchivePath.empty()) {
          modelArchivePath = registerModelResource(geo.modelFile, "object.3ds");
        }
        if (modelArchivePath.empty())
          continue;
        g3d->SetAttribute("fileName", modelArchivePath.c_str());

        Matrix geoMatrixToWrite = geo.localTransform;

        CylinderTokenParams cylinderParams;
        const bool hasCylinderToken =
            parseCylinderTokenParams(modelRef, cylinderParams);
        const bool hasExplicitCylinderDimensions =
            hasCylinderToken && cylinderParams.hasExplicitDimensions;
        const bool isRoundCylinder =
            hasExplicitCylinderDimensions &&
            std::fabs(cylinderParams.topRadiusMm -
                      cylinderParams.bottomRadiusMm) < 1e-3f;
        if (hasExplicitCylinderDimensions) {
          // Explicit parametric cylinders already encode final dimensions in
          // the generated primitive mesh; keep geometry matrix scale neutral.
          geoMatrixToWrite = MatrixUtils::Identity();
        }
        if (isRoundCylinder) {
          if (cylinderParams.axis == 'x')
            modelRef = "primitive:cylinder;axis=x";
          else if (cylinderParams.axis == 'z')
            modelRef = "primitive:cylinder;axis=z";
          else
            modelRef = "primitive:cylinder";
          modelArchivePath = registerPrimitiveModelResource(modelRef, obj.uuid);
          if (!modelArchivePath.empty())
            g3d->SetAttribute("fileName", modelArchivePath.c_str());

          // Convert token dimensions (stored in millimeters) to scene meters
          // for Geometry3D matrix scaling, matching importer expectations in
          // MA3.
          constexpr float kMillimetersPerMeter = 1000.0f;
          const float radialScale = std::max(
              (cylinderParams.topRadiusMm * 2.0f) / kMillimetersPerMeter,
                                             0.000001f);
          const float heightScale = std::max(
              cylinderParams.heightMm / kMillimetersPerMeter, 0.000001f);
          if (cylinderParams.axis == 'x') {
            for (float &component : geoMatrixToWrite.u)
              component *= heightScale;
            for (float &component : geoMatrixToWrite.v)
              component *= radialScale;
            for (float &component : geoMatrixToWrite.w)
              component *= radialScale;
          } else if (cylinderParams.axis == 'y') {
            for (float &component : geoMatrixToWrite.u)
              component *= radialScale;
            for (float &component : geoMatrixToWrite.v)
              component *= heightScale;
            for (float &component : geoMatrixToWrite.w)
              component *= radialScale;
          } else {
            for (float &component : geoMatrixToWrite.u)
              component *= radialScale;
            for (float &component : geoMatrixToWrite.v)
              component *= radialScale;
            for (float &component : geoMatrixToWrite.w)
              component *= heightScale;
          }
        }

        std::string primitiveToken;
        if (mvr::ResolvePrimitiveTokenFromModelRef(geo.modelFile,
                                                   primitiveToken)) {
          // Persist the effective model reference used for Geometry3D so a
          // roundtrip keeps primitive token parameters/axis aligned with the
          // stored geometry matrix.
          primitiveGeometryMapEntries.push_back(
              {obj.uuid, modelArchivePath, modelRef, geometryIndex});
        }

        std::string geoMatrixText = MatrixUtils::FormatMatrix(geoMatrixToWrite);
        tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
        geoMatrix->SetText(geoMatrixText.c_str());
        g3d->InsertEndChild(geoMatrix);

        geos->InsertEndChild(g3d);
        ++geometryIndex;
      }

      if (geos->FirstChild())
        oe->InsertEndChild(geos);
    } else if (!obj.modelFile.empty()) {
      tinyxml2::XMLElement *geos = doc.NewElement("Geometries");
      tinyxml2::XMLElement *g3d = doc.NewElement("Geometry3D");
      std::string modelArchivePath =
          registerPrimitiveModelResource(obj.modelFile, obj.uuid);
      if (modelArchivePath.empty()) {
        modelArchivePath = registerModelResource(obj.modelFile, "object.3ds");
      }
      if (!modelArchivePath.empty()) {
        g3d->SetAttribute("fileName", modelArchivePath.c_str());
        std::string primitiveToken;
        if (mvr::ResolvePrimitiveTokenFromModelRef(obj.modelFile,
                                                   primitiveToken)) {
          primitiveGeometryMapEntries.push_back(
              {obj.uuid, modelArchivePath, obj.modelFile, 0});
        }
        tinyxml2::XMLElement *geoMatrix = doc.NewElement("Matrix");
        geoMatrix->SetText(
            MatrixUtils::FormatMatrix(MatrixUtils::Identity()).c_str());
        g3d->InsertEndChild(geoMatrix);
        oe->InsertEndChild(geos);
        geos->InsertEndChild(g3d);
      }
    }

    if (!oe->FirstChildElement("Geometries") &&
        !appendPlaceholderCubeGeometry(oe, obj.uuid, "SceneObject")) {
      doc.DeleteNode(oe);
      return;
    }

    auto sceneObjectIdIt = assignedIds.find(obj.uuid);
    int sceneObjectNumericId = sceneObjectIdIt != assignedIds.end()
                                   ? sceneObjectIdIt->second.second
                                   : 0;
    if (sceneObjectNumericId <= 0)
      sceneObjectNumericId = 1;
    std::string sceneObjectFixtureId = sceneObjectIdIt != assignedIds.end()
                                           ? sceneObjectIdIt->second.first
                                           : std::to_string(sceneObjectNumericId);
    if (sceneObjectFixtureId.empty())
      sceneObjectFixtureId = std::to_string(sceneObjectNumericId);
    auto appendSceneObjectText = [&](const char *name, const std::string &value) {
      tinyxml2::XMLElement *element = doc.NewElement(name);
      element->SetText(value.c_str());
      oe->InsertEndChild(element);
    };
    appendSceneObjectText("FixtureID", sceneObjectFixtureId);
    appendSceneObjectText("FixtureIDNumeric", std::to_string(sceneObjectNumericId));

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

  std::string defaultLayerUuid;
  std::string defaultLayerName = DEFAULT_LAYER_NAME;
  for (const auto &[layerUuid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME) {
      defaultLayerUuid = layerUuid;
      if (!layer.name.empty())
        defaultLayerName = layer.name;
    }
  }

  for (const auto &[layerUuid, layer] : scene.layers) {
    if (layer.name == DEFAULT_LAYER_NAME)
      continue;
    tinyxml2::XMLElement *layerElem = doc.NewElement("Layer");
    if (!layerUuid.empty()) {
      const std::string exportLayerUuid =
          ExportLayerUuid(layerUuid, layer.name);
      layerElem->SetAttribute("uuid", exportLayerUuid.c_str());
    }
    if (!layer.name.empty())
      layerElem->SetAttribute("name", layer.name.c_str());

    tinyxml2::XMLElement *childList = doc.NewElement("ChildList");

    for (const auto &[uid, group] : scene.groupObjects) {
      if (group.layer != layer.name || !group.parentGroupUuid.empty())
        continue;
      exportGroupObject(exportGroupObject, childList, group);
    }

    for (const auto &[uid, f] : scene.fixtures) {
      if (f.layer != layer.name || !f.parentGroupUuid.empty())
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
      if (s.layer != layer.name || !s.parentGroupUuid.empty())
        continue;
      exportSupport(childList, s);
    }

    for (const auto &[uid, obj] : scene.sceneObjects) {
      if (obj.layer != layer.name || !obj.parentGroupUuid.empty())
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
    if (!(group.layer == DEFAULT_LAYER_NAME || group.layer.empty()) ||
        !group.parentGroupUuid.empty())
      continue;
    exportGroupObject(exportGroupObject, rootChildList, group);
  }
  for (const auto &[uid, f] : scene.fixtures) {
    if ((f.layer == DEFAULT_LAYER_NAME || f.layer.empty()) &&
        f.parentGroupUuid.empty())
      exportFixture(rootChildList, f);
  }
  for (const auto &[uid, t] : scene.trusses) {
    if ((t.layer == DEFAULT_LAYER_NAME || t.layer.empty()) &&
        t.parentGroupUuid.empty())
      exportTruss(rootChildList, t);
  }
  for (const auto &[uid, s] : scene.supports) {
    if ((s.layer == DEFAULT_LAYER_NAME || s.layer.empty()) &&
        s.parentGroupUuid.empty())
      exportSupport(rootChildList, s);
  }
  for (const auto &[uid, obj] : scene.sceneObjects) {
    if ((obj.layer == DEFAULT_LAYER_NAME || obj.layer.empty()) &&
        obj.parentGroupUuid.empty())
      exportSceneObject(rootChildList, obj);
  }
  if (rootChildList->FirstChild()) {
    tinyxml2::XMLElement *defaultLayerElem = doc.NewElement("Layer");
    if (!defaultLayerUuid.empty()) {
      const std::string exportDefaultLayerUuid =
          ExportLayerUuid(defaultLayerUuid, defaultLayerName);
      defaultLayerElem->SetAttribute("uuid", exportDefaultLayerUuid.c_str());
    }
    if (!defaultLayerName.empty())
      defaultLayerElem->SetAttribute("name", defaultLayerName.c_str());
    defaultLayerElem->InsertEndChild(rootChildList);
    layersNode->InsertEndChild(defaultLayerElem);
  }

  if (!fixtureTypeMetadata.empty()) {
    tinyxml2::XMLElement *rootPerastageDataForFixtures =
        FindOrCreatePerastageDataNode(doc, root);
    AppendFixtureTypeMetadata(doc, rootPerastageDataForFixtures,
                                      fixtureTypeMetadata);
  }

  if (trussInfoMap->FirstChild()) {
    tinyxml2::XMLElement *rootPerastageDataForTrusses =
        FindOrCreatePerastageDataNode(doc, root);
    rootPerastageDataForTrusses->InsertEndChild(trussInfoMap);
  } else {
    doc.DeleteNode(trussInfoMap);
  }

  if (hoistInfoMap->FirstChild()) {
    tinyxml2::XMLElement *rootPerastageDataForHoists =
        FindOrCreatePerastageDataNode(doc, root);
    rootPerastageDataForHoists->InsertEndChild(hoistInfoMap);
  } else {
    doc.DeleteNode(hoistInfoMap);
  }

  if (!primitiveGeometryMapEntries.empty()) {
    tinyxml2::XMLElement *data = FindOrCreatePerastageDataNode(doc, root);
    tinyxml2::XMLElement *map = doc.NewElement("PrimitiveGeometryMap");
    for (const auto &primitiveEntry : primitiveGeometryMapEntries) {
      tinyxml2::XMLElement *entry = doc.NewElement("Entry");
      entry->SetAttribute("sceneObjectUuid",
                          primitiveEntry.sceneObjectUuid.c_str());
      entry->SetAttribute("fileName", primitiveEntry.fileName.c_str());
      entry->SetAttribute("perastageModelRef",
                          primitiveEntry.perastageModelRef.c_str());
      entry->SetAttribute("geometryIndex",
                          static_cast<unsigned>(primitiveEntry.geometryIndex));
      map->InsertEndChild(entry);
    }
    data->InsertEndChild(map);
  }

  sceneNode->InsertEndChild(layersNode);

  if (!trussArchiveByTypeKey.empty()) {
    tinyxml2::XMLElement *data = FindOrCreatePerastageDataNode(doc, root);
    tinyxml2::XMLElement *manifest = doc.NewElement("TrussSidecarManifest");
    for (const auto &[typeKey, archivePath] : trussArchiveByTypeKey) {
      const auto exportTypeKeyIt = trussExportTypeKeyByTypeKey.find(typeKey);
      const std::string exportTypeKey =
          exportTypeKeyIt != trussExportTypeKeyByTypeKey.end()
                                           ? exportTypeKeyIt->second
                                           : SanitizeArchiveFileName(typeKey, "truss_type");
      tinyxml2::XMLElement *typeNode = doc.NewElement("Type");
      typeNode->SetAttribute("key", exportTypeKey.c_str());
      typeNode->SetAttribute("gdtf", archivePath.c_str());
      manifest->InsertEndChild(typeNode);
      Logger::Instance().Log(
          Logger::Level::Info,
          wxString::Format(
              "MVR export generated truss sidecar GDTF typeKey=%s archive=%s",
                           exportTypeKey.c_str(), archivePath.c_str())
              .ToStdString());
    }
    for (const auto &[uuid, typeKey] : trussInstanceToTypeKey) {
      tinyxml2::XMLElement *instNode = doc.NewElement("Instance");
      instNode->SetAttribute("uuid", uuid.c_str());
      instNode->SetAttribute("typeKey", typeKey.c_str());
      manifest->InsertEndChild(instNode);
      Logger::Instance().Log(
          Logger::Level::Info,
          wxString::Format(
              "MVR export linked truss instance to sidecar uuid=%s typeKey=%s",
                           uuid.c_str(), typeKey.c_str())
              .ToStdString());
    }
    data->InsertEndChild(manifest);
  }

  // Prunes non-referenced resources so deleted scene elements do not keep stale
  // payload files.
  const std::unordered_set<std::string> referencedArchivePaths =
      CollectReferencedArchivePaths(doc);
  std::ostringstream fixtureGdtfExportDiagnostics;
  fixtureGdtfExportDiagnostics
      << "MVR export fixture GDTF diagnostics: real="
                                << exportedRealFixtureGdtfCount
                                << ", dummyFallback=" << exportedDummyFixtureGdtfCount
                                << ", preservedOriginalRecoveries="
                                << preservedOriginalFixtureGdtfRecoveries;
  if (!dummyFallbackFixtureExamples.empty()) {
    fixtureGdtfExportDiagnostics << ", dummyExamples=";
    for (size_t i = 0; i < dummyFallbackFixtureExamples.size(); ++i) {
      if (i > 0)
        fixtureGdtfExportDiagnostics << "; ";
      fixtureGdtfExportDiagnostics << dummyFallbackFixtureExamples[i];
    }
  }
  Logger::Instance().Log(Logger::Level::Info,
                         fixtureGdtfExportDiagnostics.str());

  const std::vector<ResourceEntry> allResourceEntriesBeforePrune =
      resourceEntries;
  resourceEntries.erase(
      std::remove_if(resourceEntries.begin(), resourceEntries.end(),
                     [&](const ResourceEntry &entry) {
                       const std::string normalized =
                           NormalizeArchiveEntryPath(entry.archivePath);
                       return normalized.empty() ||
                              !referencedArchivePaths.contains(normalized);
                     }),
      resourceEntries.end());
  LogResourcePruneDiagnostics(referencedArchivePaths,
                              allResourceEntriesBeforePrune, resourceEntries);

  // Deduplicate archive resources and keep only the first entry for each
  // archive path.
  std::unordered_set<std::string> seenArchivePaths;
  std::vector<ResourceEntry> deduplicatedResources;
  deduplicatedResources.reserve(resourceEntries.size());
  for (const auto &entry : resourceEntries) {
    const std::string normalizedPath =
        NormalizeArchiveEntryPath(entry.archivePath);
    if (normalizedPath.empty())
      continue;
    if (!seenArchivePaths.insert(normalizedPath).second) {
      m_exportWarnings.push_back(
          "Referenced file '" + normalizedPath +
                                 "' appears multiple times; duplicates will be ignored.");
      continue;
    }
    deduplicatedResources.push_back(entry);
  }
  resourceEntries = std::move(deduplicatedResources);

  std::unordered_map<std::string, int> plannedArchiveEntries;
  plannedArchiveEntries["GeneralSceneDescription.xml"] = 1;

  for (auto &entry : resourceEntries) {
    if (!fs::exists(entry.sourcePath))
      continue;
    auto cit = gdtfOverrides.find(entry.archivePath);
    if (cit != gdtfOverrides.end()) {
      std::string tmp =
          CreatePatchedGdtf(entry.sourcePath.string(), cit->second);
      if (!tmp.empty())
        entry.sourcePath = fs::path(tmp);
    }
    if (ToLowerAscii(fs::path(entry.archivePath).extension().string()) == ".gdtf") {
      fs::path canonicalPath =
          fs::temp_directory_path() /
          ("perastage-canonical-" +
           SanitizeArchiveFileName(entry.archivePath, "fixture.gdtf"));
      GdtfCanonicalizer::Options canonicalOptions;
      canonicalOptions.allowFixtureTypeIdRepair = true;
      canonicalOptions.stableIdSeed = entry.archivePath + "|" + entry.sourcePath.string();
      canonicalOptions.sourceLabel = entry.archivePath + " from " + entry.sourcePath.string();
      const GdtfCanonicalizer::Result canonicalResult =
          GdtfCanonicalizer::CanonicalizeArchive(entry.sourcePath, canonicalPath,
                                                canonicalOptions);
      if (!canonicalResult.success) {
        for (const std::string &error : canonicalResult.errors)
          m_exportWarnings.push_back("GDTF canonicalization failed: " + error);
        zip.Close();
        return failExport("CanonicalizeGdtf", entry.archivePath,
                          entry.sourcePath.string(),
                          "canonicalizer reported errors");
      }
      entry.sourcePath = canonicalPath;
    }
    ++plannedArchiveEntries[entry.archivePath];
  }

  if (!ValidateMvr16Export(doc, gdtfArchiveByObjectUuid, plannedArchiveEntries,
                           &m_exportWarnings)) {
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
      zip.Close();
      return failExport("WriteXml", "GeneralSceneDescription.xml", {},
                        "duplicate ZIP entry");
    }
    auto *entry = new wxZipEntry("GeneralSceneDescription.xml");
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    if (!zip.PutNextEntry(entry)) {
      zip.Close();
      return failExport("WriteXml", "GeneralSceneDescription.xml", {},
                        "could not create ZIP entry");
    }
    zip.Write(xmlData.c_str(), xmlData.size());
    if (!zip.IsOk()) {
      zip.CloseEntry();
      zip.Close();
      return failExport("WriteXml", "GeneralSceneDescription.xml", {},
                        "could not write XML bytes");
    }
    if (!zip.CloseEntry()) {
      zip.Close();
      return failExport("WriteXml", "GeneralSceneDescription.xml", {},
                        "could not close ZIP entry");
    }
  }

  for (const auto &resource : resourceEntries) {
    if (!fs::exists(resource.sourcePath) || resource.archivePath.empty())
      continue;
    if (!writtenArchiveEntries.insert(resource.archivePath).second) {
      zip.Close();
      return failExport("WriteResource", resource.archivePath,
                        resource.sourcePath.string(), "duplicate ZIP entry");
    }
    auto *e = new wxZipEntry(resource.archivePath);
    e->SetMethod(wxZIP_METHOD_DEFLATE);
    if (!zip.PutNextEntry(e)) {
      zip.Close();
      return failExport("WriteResource", resource.archivePath,
                        resource.sourcePath.string(),
                        "could not create ZIP entry");
    }
    std::ifstream in(resource.sourcePath, std::ios::binary);
    if (!in.is_open()) {
      zip.CloseEntry();
      zip.Close();
      return failExport("WriteResource", resource.archivePath,
                        resource.sourcePath.string(),
                        "could not open source file");
    }
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0) {
        zip.Write(buf, s);
        if (!zip.IsOk()) {
          zip.CloseEntry();
          zip.Close();
          return failExport("WriteResource", resource.archivePath,
                            resource.sourcePath.string(),
                            "could not write resource bytes");
        }
      }
    }
    if (in.bad()) {
      zip.CloseEntry();
      zip.Close();
      return failExport("WriteResource", resource.archivePath,
                        resource.sourcePath.string(),
                        "could not read source file");
    }
    if (!zip.CloseEntry()) {
      zip.Close();
      return failExport("WriteResource", resource.archivePath,
                        resource.sourcePath.string(),
                        "could not close ZIP entry");
    }
  }

  if (!zip.Close())
    return failExport("FinalizeArchive", {}, filePath, "could not close ZIP");
  return true;
}

// Export an MVR into memory by writing a temporary file and reading it back.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes) {
  return ExportToBuffer(
      outBytes, mvr::preferences::LoadExportOptions(ConfigManager::Get()));
}

// Export an MVR into memory with explicit options by writing a temporary file
// and reading it back.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes,
                                 const MvrExportOptions &options) {
  outBytes.clear();
  wxFileName tempFile =
      wxFileName::CreateTempFileName("perastage_mvr_export_buffer");
  if (!tempFile.IsOk())
    return false;

  const std::string tempPath = tempFile.GetFullPath().ToStdString();
  const bool exported = ExportToFile(tempPath, options);
  if (!exported) {
    Logger::Instance().Log(Logger::Level::Error,
                           "MVR export-to-buffer failed while writing " +
                               tempPath);
    wxRemoveFile(tempFile.GetFullPath());
    return false;
  }
  std::error_code sizeEc;
  const auto tempSize = fs::exists(tempPath, sizeEc) && !sizeEc
                            ? fs::file_size(tempPath, sizeEc)
                            : 0;
  if (sizeEc || tempSize == 0) {
    Logger::Instance().Log(
        Logger::Level::Error,
        "MVR export-to-buffer produced an empty or unreadable file '" +
            tempPath + "' size=" + std::to_string(tempSize));
    wxRemoveFile(tempFile.GetFullPath());
    return false;
  }

  std::ifstream input(tempPath, std::ios::binary);
  if (!input.is_open()) {
    Logger::Instance().Log(Logger::Level::Error,
                           "MVR export-to-buffer could not open " + tempPath);
    wxRemoveFile(tempFile.GetFullPath());
    return false;
  }

  input.seekg(0, std::ios::end);
  const std::streampos size = input.tellg();
  input.seekg(0, std::ios::beg);
  if (size <= 0) {
    input.close();
    wxRemoveFile(tempFile.GetFullPath());
    Logger::Instance().Log(Logger::Level::Error,
                           "MVR export-to-buffer read zero bytes from " +
                               tempPath);
    return false;
  }
  outBytes.resize(static_cast<size_t>(size));
  input.read(reinterpret_cast<char *>(outBytes.data()), size);

  const bool readOk = input.good() || input.eof();
  input.close();
  wxRemoveFile(tempFile.GetFullPath());
  if (!readOk || outBytes.empty()) {
    Logger::Instance().Log(Logger::Level::Error,
                           "MVR export-to-buffer failed to read payload from " +
                               tempPath);
    return false;
  }
  return true;
}

// Return non-fatal validation and packaging warnings captured during export.
const std::vector<std::string> &MvrExporter::GetExportWarnings() const {
  return m_exportWarnings;
}
