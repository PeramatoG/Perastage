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

// Returns sorted UUID keys from all maps in a lookup family.
template <typename... Maps>
std::vector<std::string> SortedUnionUuidKeys(const Maps &...maps) {
  std::unordered_set<std::string> seen;
  auto collect = [&seen](const auto &map) {
    for (const auto &entry : map)
      seen.insert(entry.first);
  };
  (collect(maps), ...);
  std::vector<std::string> keys(seen.begin(), seen.end());
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Reports whether two matrices contain identical transform components.
bool MatrixValuesEqual(const Matrix &current, const Matrix &incoming) {
  return current.u == incoming.u && current.v == incoming.v &&
         current.w == incoming.w && current.o == incoming.o;
}

// Reports whether two Symdef geometry entries describe the same geometry.
bool SymdefGeometryValuesEqual(const SymdefGeometry &current,
                               const SymdefGeometry &incoming) {
  return current.file == incoming.file &&
         current.geometryType == incoming.geometryType &&
         MatrixValuesEqual(current.transform, incoming.transform);
}

// Reports whether two Symdef geometry lists have identical entries.
bool SymdefGeometryListsEqual(const std::vector<SymdefGeometry> &current,
                              const std::vector<SymdefGeometry> &incoming) {
  if (current.size() != incoming.size())
    return false;
  for (std::size_t i = 0; i < current.size(); ++i) {
    if (!SymdefGeometryValuesEqual(current[i], incoming[i]))
      return false;
  }
  return true;
}

// Looks up a value from a map or returns the supplied fallback.
template <typename T>
T LookupOrDefault(const std::unordered_map<std::string, T> &values,
                  const std::string &uuid, const T &fallback = T{}) {
  const auto it = values.find(uuid);
  if (it == values.end())
    return fallback;
  return it->second;
}

// Reports whether two Symdef UUID entries describe the same definition.
bool SymdefDefinitionsEqual(const MvrScene &target, const MvrScene &imported,
                            const std::string &uuid) {
  return LookupOrDefault(target.symdefFiles, uuid) ==
             LookupOrDefault(imported.symdefFiles, uuid) &&
         LookupOrDefault(target.symdefTypes, uuid) ==
             LookupOrDefault(imported.symdefTypes, uuid) &&
         MatrixValuesEqual(LookupOrDefault(target.symdefMatrices, uuid),
                           LookupOrDefault(imported.symdefMatrices, uuid)) &&
         SymdefGeometryListsEqual(
             LookupOrDefault(target.symdefGeometries, uuid),
             LookupOrDefault(imported.symdefGeometries, uuid));
}

// Reports whether two layer entries share the same merge-visible content.
bool LayerValuesEqual(const Layer &current, const Layer &incoming) {
  return current.name == incoming.name && current.color == incoming.color &&
         current.childUUIDs == incoming.childUUIDs;
}

// Builds an imported layer name that does not shadow current layer state.
std::string BuildUniqueImportedLayerName(
    const std::string &layerName,
    const std::unordered_set<std::string> &reservedLayerNames) {
  const std::string base = !TrimAscii(layerName).empty()
                               ? TrimAscii(layerName)
                               : "Imported layer";
  std::string candidate = base + " (Imported)";
  int suffix = 2;
  while (reservedLayerNames.contains(candidate))
    candidate = base + " (Imported " + std::to_string(suffix++) + ")";
  return candidate;
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

// Prepares position UUID remaps while preserving current names on conflicts.
void PreparePositionLookupTable(const MvrScene &target, const MvrScene &imported,
                                std::unordered_set<std::string> &usedUuids,
                                MvrMergeAnalysis &analysis,
                                const MvrMergeOptions &options) {
  for (const auto &uuid : SortedUuidKeys(imported.positions)) {
    const auto targetIt = target.positions.find(uuid);
    const auto importedIt = imported.positions.find(uuid);
    if (targetIt != target.positions.end() &&
        importedIt != imported.positions.end() &&
        targetIt->second == importedIt->second) {
      analysis.uuidMap[uuid] = uuid;
      continue;
    }
    if (targetIt != target.positions.end() &&
        importedIt != imported.positions.end() &&
        targetIt->second != importedIt->second) {
      ++analysis.uuidCollisionsDetected;
      const std::string replacement =
          DeriveStableReplacementUuid("positions", uuid, usedUuids);
      analysis.uuidMap[uuid] = replacement;
      ++analysis.uuidCollisionsResolved;
      ++analysis.nonObjectLookupConflictsResolved;
      continue;
    }
    (void)ResolveImportedUuid(uuid, "positions", usedUuids, analysis, options);
  }
}

// Prepares Symdef UUID remaps as one logical definition across lookup maps.
void PrepareSymdefLookupTables(const MvrScene &target, const MvrScene &imported,
                               std::unordered_set<std::string> &usedUuids,
                               MvrMergeAnalysis &analysis,
                               const MvrMergeOptions &options) {
  for (const auto &uuid : SortedUnionUuidKeys(
           imported.symdefFiles, imported.symdefTypes, imported.symdefMatrices,
           imported.symdefGeometries)) {
    const bool targetHasSymdef = target.symdefFiles.contains(uuid) ||
                                 target.symdefTypes.contains(uuid) ||
                                 target.symdefMatrices.contains(uuid) ||
                                 target.symdefGeometries.contains(uuid);
    if (targetHasSymdef && SymdefDefinitionsEqual(target, imported, uuid)) {
      analysis.uuidMap[uuid] = uuid;
      continue;
    }
    if (targetHasSymdef && !SymdefDefinitionsEqual(target, imported, uuid)) {
      ++analysis.uuidCollisionsDetected;
      const std::string replacement =
          DeriveStableReplacementUuid("symdefs", uuid, usedUuids);
      analysis.uuidMap[uuid] = replacement;
      ++analysis.uuidCollisionsResolved;
      ++analysis.nonObjectLookupConflictsResolved;
      continue;
    }
    (void)ResolveImportedUuid(uuid, "symdefs", usedUuids, analysis, options);
  }
}

// Prepares layer UUID and name remaps without changing current layer names.
void PrepareLayerTable(const MvrScene &target, const MvrScene &imported,
                       std::unordered_set<std::string> &usedUuids,
                       MvrMergeAnalysis &analysis,
                       const MvrMergeOptions &options) {
  std::unordered_set<std::string> reservedLayerNames;
  for (const auto &[uuid, layer] : target.layers)
    reservedLayerNames.insert(layer.name);

  for (const auto &uuid : SortedUuidKeys(imported.layers)) {
    const Layer &incoming = imported.layers.at(uuid);
    std::string resolvedUuid = uuid;
    bool keepIncoming = true;
    const auto targetIt = target.layers.find(uuid);
    if (uuid.empty()) {
      resolvedUuid = DeriveStableReplacementUuid("layers", incoming.name,
                                                 usedUuids);
      ++analysis.uuidCollisionsResolved;
    } else if (targetIt != target.layers.end()) {
      ++analysis.uuidCollisionsDetected;
      if (LayerValuesEqual(targetIt->second, incoming)) {
        analysis.skippedIncomingLayerUuids.insert(uuid);
        keepIncoming = false;
      } else {
        resolvedUuid = DeriveStableReplacementUuid("layers", uuid, usedUuids);
        ++analysis.uuidCollisionsResolved;
        ++analysis.nonObjectLookupConflictsResolved;
      }
    } else if (!usedUuids.insert(uuid).second) {
      resolvedUuid = ResolveImportedUuid(uuid, "layers", usedUuids, analysis,
                                         options);
    }

    if (keepIncoming)
      analysis.layerUuidMap[uuid] = resolvedUuid;

    if (!keepIncoming)
      continue;
    if (reservedLayerNames.contains(incoming.name) &&
        (targetIt == target.layers.end() || resolvedUuid != uuid)) {
      const std::string renamed =
          BuildUniqueImportedLayerName(incoming.name, reservedLayerNames);
      analysis.incomingLayerNameRenames[incoming.name] = renamed;
      reservedLayerNames.insert(renamed);
      ++analysis.nonObjectLookupConflictsResolved;
    } else {
      reservedLayerNames.insert(incoming.name);
    }
  }
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
  PreparePositionLookupTable(target, imported, usedUuids, analysis, options);
  PrepareSymdefLookupTables(target, imported, usedUuids, analysis, options);
  PrepareObjectTable(imported.fixtures, "fixtures", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.trusses, "trusses", usedUuids, analysis, options);
  PrepareObjectTable(imported.supports, "supports", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.sceneObjects, "sceneObjects", usedUuids, analysis,
                     options);
  PrepareObjectTable(imported.groupObjects, "groupObjects", usedUuids, analysis,
                     options);
  PrepareLayerTable(target, imported, usedUuids, analysis, options);

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
