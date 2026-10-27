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
#include "fixture_patch_address.h"
#include "uuidutils.h"
#include "mvr_symdef_preservation.h"
#include "mvr_matrix_precision.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
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
  addKeys(scene.classes);
  addKeys(scene.symdefs);
  for (const auto &[uuid, truss] : scene.trusses)
    if (!truss.sourceSymbolUuid.empty()) used.insert(truss.sourceSymbolUuid);
  const auto addSymbols = [&](const auto &objects) {
    for (const auto &[uuid, object] : objects)
      for (const auto &geometry : object.geometries)
        if (!geometry.sourceSymbolUuid.empty()) used.insert(geometry.sourceSymbolUuid);
  };
  addSymbols(scene.sceneObjects); addSymbols(scene.supports);
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

// Builds a stable string key for a parsed DMX patch address.
std::string
BuildPatchAddressKey(const perastage::patch::FixturePatchAddress &address) {
  return std::to_string(address.universe) + "." +
         std::to_string(address.channel);
}

// Adds non-blocking warnings for incoming fixtures that reuse current patches.
void AnalyzePatchAddressWarnings(const MvrScene &target,
                                 const MvrScene &imported,
                                 MvrMergeAnalysis &analysis) {
  std::unordered_map<std::string, std::vector<std::string>> currentByAddress;
  const std::vector<std::string> currentFixtureUuids =
      SortedUuidKeys(target.fixtures);
  for (const std::string &uuid : currentFixtureUuids) {
    const auto parsedAddress =
        perastage::patch::ParseFixturePatchAddress(
            target.fixtures.at(uuid).address);
    if (!parsedAddress.has_value())
      continue;
    currentByAddress[BuildPatchAddressKey(*parsedAddress)].push_back(uuid);
  }

  const std::vector<std::string> incomingFixtureUuids =
      SortedUuidKeys(imported.fixtures);
  for (const std::string &incomingUuid : incomingFixtureUuids) {
    if (analysis.skippedIncomingUuids.contains(incomingUuid))
      continue;
    const auto parsedAddress =
        perastage::patch::ParseFixturePatchAddress(
            imported.fixtures.at(incomingUuid).address);
    if (!parsedAddress.has_value())
      continue;

    const auto currentIt =
        currentByAddress.find(BuildPatchAddressKey(*parsedAddress));
    if (currentIt == currentByAddress.end())
      continue;
    const std::string resolvedIncomingUuid =
        RemapImportedUuidReference(incomingUuid, analysis);
    for (const std::string &currentUuid : currentIt->second) {
      if (currentUuid == resolvedIncomingUuid)
        continue;
      analysis.patchAddressWarnings.push_back(MvrMergePatchAddressWarning{
          parsedAddress->universe, parsedAddress->channel, currentUuid,
          incomingUuid});
    }
  }
}

using ResourceHashes = std::unordered_map<std::string, std::optional<std::string>>;

// Only proven byte-equivalent self-contained definitions share an identity.
bool SymdefDefinitionsEqual(const MvrScene &target, const MvrScene &imported,
                            const std::string &uuid, ResourceHashes &hashes) {
  const auto &left = target.symdefs.at(uuid);
  const auto &right = imported.symdefs.at(uuid);
  if (left.name.value_or("") != right.name.value_or("") ||
      left.geometries.size() != right.geometries.size()) return false;
  const auto hash = [&](const MvrScene &scene, const std::string &file) {
    const auto path = ResolveSymdefResource(scene, file);
    const auto key = path.lexically_normal().string();
    auto found = hashes.find(key);
    if (found == hashes.end()) found = hashes.emplace(key, FileImportUtils::ComputeFileSha256(path)).first;
    return found->second;
  };
  for (size_t i = 0; i < left.geometries.size(); ++i) {
    const auto &a = left.geometries[i];
    const auto &b = right.geometries[i];
    const auto firstMatrix = ParseCanonicalMatrix(FormatCanonicalMatrix(a.transform, a.matrixSource));
    const auto secondMatrix = ParseCanonicalMatrix(FormatCanonicalMatrix(b.transform, b.matrixSource));
    if (!firstMatrix || !secondMatrix || firstMatrix->values != secondMatrix->values) return false;
    const auto first = hash(target, left.geometries[i].file);
    const auto second = hash(imported, right.geometries[i].file);
    if (!first || !second || first != second) return false;
  }
  return true;
}

// Reports whether two layer entries share the same merge-visible content.
bool LayerValuesEqual(const Layer &current, const Layer &incoming) {
  return current.name == incoming.name && current.color == incoming.color &&
         current.childUUIDs == incoming.childUUIDs &&
         FormatCanonicalMatrix(current.transform, current.matrixSource) ==
             FormatCanonicalMatrix(incoming.transform, incoming.matrixSource);
}


// Derives an unused deterministic replacement UUID.
std::string
DeriveStableReplacementUuid(std::string_view tableName, const std::string &uuid,
                            std::unordered_set<std::string> &usedUuids,
                            const std::unordered_set<std::string> &reserved) {
  std::string seed = "mvr:merge:" + std::string(tableName) + ":" + uuid;
  std::string replacement = DeriveDeterministicUuid(seed);
  size_t suffix = 1;
  while (reserved.contains(replacement) || !usedUuids.insert(replacement).second)
    replacement =
        DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
  return replacement;
}

// Resolves an imported UUID and records reference remaps.
std::string ResolveImportedUuid(const std::string &uuid,
                                std::string_view tableName,
                                std::unordered_set<std::string> &usedUuids,
                                MvrMergeAnalysis &analysis,
                                const MvrMergeOptions &options,
                                bool definitionCollision = false) {
  if (uuid.empty())
    return uuid;

  if (usedUuids.insert(uuid).second) {
    analysis.uuidMap.emplace(uuid, uuid);
    return uuid;
  }

  ++analysis.uuidCollisionsDetected;
  if (!definitionCollision && options.uuidCollisionBehavior ==
      MvrMergeUuidCollisionBehavior::ReplaceExisting) {
    analysis.uuidMap[uuid] = uuid;
    return uuid;
  }
  if (!definitionCollision && options.uuidCollisionBehavior ==
      MvrMergeUuidCollisionBehavior::SkipIncoming) {
    analysis.uuidMap[uuid] = uuid;
    analysis.skippedIncomingUuids.insert(uuid);
    return uuid;
  }

  const std::string replacement =
      DeriveStableReplacementUuid(tableName, uuid, usedUuids, analysis.reservedIncomingUuids);
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
                        const MvrMergeOptions &options,
                        const MvrScene &target) {
  for (const auto &uuid : SortedUuidKeys(imported))
    (void)ResolveImportedUuid(uuid, tableName, usedUuids, analysis, options,
                              target.classes.contains(uuid) || target.symdefs.contains(uuid));
}

// Only identical definitions with the same UUID share identity. Names never coalesce.
void PrepareClassLookupTable(const MvrScene &target, const MvrScene &imported,
                             std::unordered_set<std::string> &usedUuids,
                             MvrMergeAnalysis &analysis) {
  for (const auto &uuid : SortedUuidKeys(imported.classes)) {
    const auto existing = target.classes.find(uuid);
    if (existing != target.classes.end() && existing->second.name.value_or("") == imported.classes.at(uuid).name.value_or("")) {
      analysis.uuidMap[uuid] = uuid;
    } else if (usedUuids.insert(uuid).second) {
      analysis.uuidMap[uuid] = uuid;
    } else {
      ++analysis.uuidCollisionsDetected;
      analysis.uuidMap[uuid] = DeriveStableReplacementUuid("classes", uuid, usedUuids, analysis.reservedIncomingUuids);
      ++analysis.uuidCollisionsResolved;
      ++analysis.nonObjectLookupConflictsResolved;
    }
  }
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
          DeriveStableReplacementUuid("positions", uuid, usedUuids, analysis.reservedIncomingUuids);
      analysis.uuidMap[uuid] = replacement;
      ++analysis.uuidCollisionsResolved;
      ++analysis.nonObjectLookupConflictsResolved;
      continue;
    }
    (void)ResolveImportedUuid(uuid, "positions", usedUuids, analysis, options,
                              target.classes.contains(uuid) || target.symdefs.contains(uuid));
  }
}

// Prepares Symdef UUID remaps as one logical definition across lookup maps.
void PrepareSymdefLookupTables(const MvrScene &target, const MvrScene &imported,
                               std::unordered_set<std::string> &usedUuids,
                               MvrMergeAnalysis &analysis,
                               const MvrMergeOptions &options) {
  ResourceHashes hashes;
  for (const auto &uuid : SortedUuidKeys(imported.symdefs)) {
    const bool targetHasSymdef = target.symdefs.contains(uuid);
    if (targetHasSymdef && SymdefDefinitionsEqual(target, imported, uuid, hashes)) {
      analysis.uuidMap[uuid] = uuid;
      continue;
    }
    if (targetHasSymdef && !SymdefDefinitionsEqual(target, imported, uuid, hashes)) {
      ++analysis.uuidCollisionsDetected;
      const std::string replacement =
          DeriveStableReplacementUuid("symdefs", uuid, usedUuids, analysis.reservedIncomingUuids);
      analysis.uuidMap[uuid] = replacement;
      ++analysis.uuidCollisionsResolved;
      ++analysis.nonObjectLookupConflictsResolved;
      continue;
    }
    (void)ResolveImportedUuid(uuid, "symdefs", usedUuids, analysis, options, true);
  }
}

// Prepares Layer UUID remaps; equal authored names remain independent.
void PrepareLayerTable(const MvrScene &target, const MvrScene &imported,
                       std::unordered_set<std::string> &usedUuids,
                       MvrMergeAnalysis &analysis,
                       const MvrMergeOptions &options) {
  for (const auto &uuid : SortedUuidKeys(imported.layers)) {
    const Layer &incoming = imported.layers.at(uuid);
    std::string resolvedUuid = uuid;
    bool keepIncoming = true;
    const auto targetIt = target.layers.find(uuid);
    if (uuid.empty()) {
      resolvedUuid = DeriveStableReplacementUuid("layers", incoming.name,
                                                 usedUuids, analysis.reservedIncomingUuids);
      ++analysis.uuidCollisionsResolved;
    } else if (targetIt != target.layers.end()) {
      ++analysis.uuidCollisionsDetected;
      if (LayerValuesEqual(targetIt->second, incoming)) {
        analysis.skippedIncomingLayerUuids.insert(uuid);
        keepIncoming = false;
      } else {
        resolvedUuid = DeriveStableReplacementUuid("layers", uuid, usedUuids, analysis.reservedIncomingUuids);
        ++analysis.uuidCollisionsResolved;
        ++analysis.nonObjectLookupConflictsResolved;
      }
    } else if (!usedUuids.insert(uuid).second) {
      resolvedUuid = ResolveImportedUuid(uuid, "layers", usedUuids, analysis,
                                         options, target.classes.contains(uuid) || target.symdefs.contains(uuid));
    }

    if (keepIncoming)
      analysis.layerUuidMap[uuid] = resolvedUuid;
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

  analysis.reservedIncomingUuids = CollectUsedUuids(imported);
  std::unordered_set<std::string> usedUuids = CollectUsedUuids(target);
  PrepareClassLookupTable(target, imported, usedUuids, analysis);
  PreparePositionLookupTable(target, imported, usedUuids, analysis, options);
  PrepareSymdefLookupTables(target, imported, usedUuids, analysis, options);
  PrepareObjectTable(imported.fixtures, "fixtures", usedUuids, analysis,
                     options, target);
  PrepareObjectTable(imported.trusses, "trusses", usedUuids, analysis, options, target);
  PrepareObjectTable(imported.supports, "supports", usedUuids, analysis,
                     options, target);
  PrepareObjectTable(imported.sceneObjects, "sceneObjects", usedUuids, analysis,
                     options, target);
  PrepareObjectTable(imported.groupObjects, "groupObjects", usedUuids, analysis,
                     options, target);
  PrepareLayerTable(target, imported, usedUuids, analysis, options);
  // Symbol identities are separate instances, not definition or owner UUIDs.
  for (const auto &uuid : SortedUuidKeys(imported.trusses)) {
    const auto &symbol = imported.trusses.at(uuid).sourceSymbolUuid;
    if (!symbol.empty()) {
      MvrMergeOptions preserveInstances;
      (void)ResolveImportedUuid(symbol, "symbols", usedUuids, analysis, preserveInstances);
    }
  }
  AnalyzePatchAddressWarnings(target, imported, analysis);

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
