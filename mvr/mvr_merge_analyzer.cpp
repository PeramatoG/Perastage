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

#include "uuidutils.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace mvr {
namespace {

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

} // namespace

// Analyzes imported scene UUIDs and prepares collision-safe reference remaps.
MvrMergeAnalysis AnalyzeImportedSceneMerge(const MvrScene &target,
                                           const MvrScene &imported,
                                           const MvrMergeOptions &options) {
  MvrMergeAnalysis analysis;
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
