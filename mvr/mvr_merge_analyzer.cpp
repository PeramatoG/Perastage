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
#include "mvr_merge_analyzer.h"

#include "file_import_utils.h"
#include "uuidutils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <vector>

namespace mvr {
namespace {

// Trims leading and trailing ASCII whitespace from a value.
std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    value.pop_back();
  return value;
}

// Converts a string to lowercase ASCII for stable comparisons.
std::string ToLowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Normalizes fixture type names for merge identity comparisons.
std::string NormalizeFixtureTypeName(const std::string &typeName) {
  return ToLowerAscii(TrimAscii(typeName));
}

// Resolves a scene resource path to the best available local filesystem path.
std::filesystem::path
ResolveSceneResourcePath(const MvrScene &scene,
                         const std::string &resourcePath) {
  if (resourcePath.empty())
    return {};

  std::filesystem::path path = std::filesystem::path(resourcePath);
  if (path.is_absolute())
    return path;
  if (!scene.basePath.empty())
    return std::filesystem::path(scene.basePath) / path;
  return path;
}

// Normalizes a resolved path token for case-insensitive identity comparisons.
std::string NormalizeResolvedPathToken(const std::filesystem::path &path,
                                       const std::string &fallback) {
  std::error_code ec;
  const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
  if (!ec)
    return ToLowerAscii(absolute.lexically_normal().string());
  return ToLowerAscii(TrimAscii(fallback));
}

// Creates an identity descriptor for a fixture type in the supplied scene.
MvrFixtureTypeIdentity BuildFixtureTypeIdentity(const MvrScene &scene,
                                                const Fixture &fixture,
                                                const std::string &normalized) {
  MvrFixtureTypeIdentity identity;
  identity.normalizedTypeName = normalized;
  identity.typeName = fixture.typeName;
  identity.gdtfSpec = fixture.gdtfSpec;
  identity.gdtfMode = fixture.gdtfMode;

  const std::filesystem::path resolvedPath =
      ResolveSceneResourcePath(scene, fixture.gdtfSpec);
  if (!resolvedPath.empty()) {
    identity.resolvedGdtfSpec =
        NormalizeResolvedPathToken(resolvedPath, fixture.gdtfSpec);
    std::error_code ec;
    if (std::filesystem::is_regular_file(resolvedPath, ec) && !ec) {
      if (const auto hash = FileImportUtils::ComputeFileSha256(resolvedPath))
        identity.gdtfSha256 = *hash;
    }
  } else {
    identity.resolvedGdtfSpec = ToLowerAscii(TrimAscii(fixture.gdtfSpec));
  }
  return identity;
}

// Chooses a stable display name for an imported conflicting fixture type.
std::string BuildUniqueImportedTypeName(
    const MvrFixtureTypeIdentity &incoming,
    const std::unordered_set<std::string> &reservedNormalizedNames) {
  const std::string base = !TrimAscii(incoming.typeName).empty()
                               ? TrimAscii(incoming.typeName)
                               : "Imported fixture type";
  std::string candidate = base + " (Imported)";
  int suffix = 2;
  while (reservedNormalizedNames.contains(NormalizeFixtureTypeName(candidate)))
    candidate = base + " (Imported " + std::to_string(suffix++) + ")";
  return candidate;
}

// Collects every UUID already owned by a scene across object and lookup tables.
std::unordered_set<std::string> CollectUsedUuids(const MvrScene &scene) {
  std::unordered_set<std::string> used;
  auto addKeys = [&used](const auto &objects) {
    for (const auto &entry : objects)
      used.insert(entry.first);
  };
  addKeys(scene.fixtures);
  addKeys(scene.trusses);
  addKeys(scene.supports);
  addKeys(scene.sceneObjects);
  addKeys(scene.groupObjects);
  addKeys(scene.layers);
  addKeys(scene.positions);
  addKeys(scene.symdefFiles);
  addKeys(scene.symdefTypes);
  addKeys(scene.symdefMatrices);
  addKeys(scene.symdefGeometries);
  return used;
}

// Returns sorted UUID keys so merge analysis is deterministic across platforms.
template <typename T>
std::vector<std::string>
SortedUuidKeys(const std::unordered_map<std::string, T> &objects) {
  std::vector<std::string> keys;
  keys.reserve(objects.size());
  for (const auto &entry : objects)
    keys.push_back(entry.first);
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Derives an unused deterministic replacement UUID.
std::string
DeriveStableReplacementUuid(std::string_view tableName, const std::string &uuid,
                            std::unordered_set<std::string> &usedUuids) {
  std::string seed = "mvr:merge:" + std::string(tableName) + ":" + uuid;
  std::string replacement = DeriveDeterministicUuid(seed);
  size_t suffix = 1;
  while (!usedUuids.insert(replacement).second)
    replacement =
        DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
  return replacement;
}

// Resolves an imported UUID and records reference remaps.
std::string ResolveImportedUuid(const std::string &uuid,
                                std::string_view tableName,
                                std::unordered_set<std::string> &usedUuids,
                                MvrMergeAnalysis &analysis,
                                const MvrMergeOptions &options) {
  if (uuid.empty())
    return uuid;

  if (usedUuids.insert(uuid).second) {
    analysis.uuidMap.emplace(uuid, uuid);
    return uuid;
  }

  ++analysis.uuidCollisionsDetected;
  if (options.uuidCollisionBehavior ==
      MvrMergeUuidCollisionBehavior::ReplaceExisting) {
    analysis.uuidMap[uuid] = uuid;
    return uuid;
  }
  if (options.uuidCollisionBehavior ==
      MvrMergeUuidCollisionBehavior::SkipIncoming) {
    analysis.uuidMap[uuid] = uuid;
    analysis.skippedIncomingUuids.insert(uuid);
    return uuid;
  }

  const std::string replacement =
      DeriveStableReplacementUuid(tableName, uuid, usedUuids);
  analysis.uuidMap[uuid] = replacement;
  ++analysis.uuidCollisionsResolved;
  return replacement;
}

// Records UUID remaps without mutating imported scene content.
template <typename T>
void PrepareObjectTable(const std::unordered_map<std::string, T> &imported,
                        std::string_view tableName,
                        std::unordered_set<std::string> &usedUuids,
                        MvrMergeAnalysis &analysis,
                        const MvrMergeOptions &options) {
  for (const auto &uuid : SortedUuidKeys(imported))
    (void)ResolveImportedUuid(uuid, tableName, usedUuids, analysis, options);
}

// Adds fixture type conflict metadata and rename decisions to the analysis.
void AnalyzeFixtureTypeConflicts(const MvrMergeOptions &options,
                                 MvrMergeAnalysis &analysis) {
  std::unordered_set<std::string> reservedNames;
  for (const auto &[normalized, identity] : analysis.currentFixtureTypes)
    reservedNames.insert(normalized);
  for (const auto &[normalized, identity] : analysis.incomingFixtureTypes)
    reservedNames.insert(normalized);

  for (const auto &[normalized, incoming] : analysis.incomingFixtureTypes) {
    const auto currentIt = analysis.currentFixtureTypes.find(normalized);
    if (currentIt == analysis.currentFixtureTypes.end() ||
        !FixtureTypeIdentitiesConflict(currentIt->second, incoming))
      continue;

    MvrFixtureTypeConflict conflict;
    conflict.normalizedTypeName = normalized;
    conflict.currentIdentity = currentIt->second;
    conflict.incomingIdentity = incoming;
    conflict.suggestedIncomingTypeName =
        BuildUniqueImportedTypeName(incoming, reservedNames);
    reservedNames.insert(
        NormalizeFixtureTypeName(conflict.suggestedIncomingTypeName));
    analysis.fixtureTypeConflicts.push_back(conflict);

    const auto decisionIt = options.fixtureTypeDecisions.find(normalized);
    if (decisionIt == options.fixtureTypeDecisions.end())
      continue;
    analysis.fixtureTypeDecisions[normalized] = decisionIt->second;
    if (decisionIt->second == MvrMergeFixtureTypeDecision::RenameIncomingType)
      analysis.incomingFixtureTypeRenames[normalized] =
          conflict.suggestedIncomingTypeName;
  }
}

} // namespace

// Builds a fixture type identity map keyed by normalized fixture type name.
std::unordered_map<std::string, MvrFixtureTypeIdentity>
BuildFixtureTypeIdentityMap(const MvrScene &scene) {
  std::unordered_map<std::string, MvrFixtureTypeIdentity> types;
  std::vector<std::string> fixtureUuids;
  fixtureUuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures)
    fixtureUuids.push_back(uuid);
  std::sort(fixtureUuids.begin(), fixtureUuids.end());

  for (const auto &uuid : fixtureUuids) {
    const Fixture &fixture = scene.fixtures.at(uuid);
    const std::string normalized = NormalizeFixtureTypeName(fixture.typeName);
    if (normalized.empty())
      continue;
    const MvrFixtureTypeIdentity identity =
        BuildFixtureTypeIdentity(scene, fixture, normalized);
    const auto existingIt = types.find(normalized);
    if (existingIt == types.end()) {
      types.emplace(normalized, identity);
      continue;
    }
  }
  return types;
}

// Reports whether two fixture identities resolve to different definitions.
bool FixtureTypeIdentitiesConflict(const MvrFixtureTypeIdentity &current,
                                   const MvrFixtureTypeIdentity &incoming) {
  if (TrimAscii(current.gdtfMode) != TrimAscii(incoming.gdtfMode))
    return true;
  if (!current.gdtfSha256.empty() && !incoming.gdtfSha256.empty())
    return current.gdtfSha256 != incoming.gdtfSha256;
  if (!current.resolvedGdtfSpec.empty() || !incoming.resolvedGdtfSpec.empty())
    return current.resolvedGdtfSpec != incoming.resolvedGdtfSpec;
  return ToLowerAscii(TrimAscii(current.gdtfSpec)) !=
         ToLowerAscii(TrimAscii(incoming.gdtfSpec));
}

// Reports whether unresolved fixture type conflicts prevent merge application.
bool HasBlockingFixtureTypeConflicts(const MvrMergeAnalysis &analysis) {
  for (const auto &conflict : analysis.fixtureTypeConflicts) {
    const auto decisionIt =
        analysis.fixtureTypeDecisions.find(conflict.normalizedTypeName);
    if (decisionIt == analysis.fixtureTypeDecisions.end() ||
        decisionIt->second == MvrMergeFixtureTypeDecision::CancelMerge)
      return true;
  }
  return false;
}

// Analyzes imported scene UUIDs and prepares collision-safe reference remaps.
MvrMergeAnalysis AnalyzeImportedSceneMerge(const MvrScene &target,
                                           const MvrScene &imported,
                                           const MvrMergeOptions &options) {
  MvrMergeAnalysis analysis;
  analysis.currentFixtureTypes = BuildFixtureTypeIdentityMap(target);
  analysis.incomingFixtureTypes = BuildFixtureTypeIdentityMap(imported);
  AnalyzeFixtureTypeConflicts(options, analysis);

  std::unordered_set<std::string> usedUuids = CollectUsedUuids(target);
  PrepareObjectTable(imported.positions, "positions", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.symdefFiles, "symdefFiles", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.symdefTypes, "symdefTypes", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.symdefMatrices, "symdefMatrices", usedUuids,
                     analysis, options);
  PrepareObjectTable(imported.symdefGeometries, "symdefGeometries", usedUuids,
                     analysis, options);
  PrepareObjectTable(imported.fixtures, "fixtures", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.trusses, "trusses", usedUuids, analysis, options);
  PrepareObjectTable(imported.supports, "supports", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.sceneObjects, "sceneObjects", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.groupObjects, "groupObjects", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.layers, "layers", usedUuids, analysis, options);

  for (const auto &[oldUuid, newUuid] : analysis.uuidMap) {
    if (oldUuid != newUuid && imported.fixtures.contains(oldUuid))
      analysis.fixtureUuidRemap.emplace(oldUuid, newUuid);
  }
  return analysis;
}

// Reports whether an imported object UUID should be skipped during merge apply.
bool ShouldSkipImportedUuid(const std::string &uuid,
                            const MvrMergeAnalysis &analysis) {
  return analysis.skippedIncomingUuids.contains(uuid);
}

// Resolves an imported UUID reference using the prepared merge analysis.
std::string RemapImportedUuidReference(const std::string &uuid,
                                       const MvrMergeAnalysis &analysis) {
  const auto it = analysis.uuidMap.find(uuid);
  if (it == analysis.uuidMap.end())
    return uuid;
  return it->second;
}

} // namespace mvr
