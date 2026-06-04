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

#include <unordered_set>

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
  for (const auto &entry : scene.positions)
    used.insert(entry.first);
  for (const auto &entry : scene.symdefFiles)
    used.insert(entry.first);
  for (const auto &entry : scene.symdefTypes)
    used.insert(entry.first);
  for (const auto &entry : scene.symdefMatrices)
    used.insert(entry.first);
  for (const auto &entry : scene.symdefGeometries)
    used.insert(entry.first);
  return used;
}

// Resolves a collision-free UUID for an imported item and records reference remaps.
std::string ResolveImportedUuid(const std::string &uuid,
                                std::unordered_set<std::string> &usedUuids,
                                MvrMergeAnalysis &analysis) {
  if (uuid.empty())
    return uuid;

  if (usedUuids.insert(uuid).second) {
    analysis.uuidMap.emplace(uuid, uuid);
    return uuid;
  }

  std::string replacement;
  do {
    replacement = GenerateUuid();
  } while (!usedUuids.insert(replacement).second);

  analysis.uuidMap[uuid] = replacement;
  ++analysis.uuidCollisionsResolved;
  return replacement;
}

// Records UUID remaps for an imported object map without mutating scene content.
template <typename T>
void PrepareObjectTable(const std::unordered_map<std::string, T> &imported,
                        std::unordered_set<std::string> &usedUuids,
                        MvrMergeAnalysis &analysis) {
  for (const auto &entry : imported)
    (void)ResolveImportedUuid(entry.first, usedUuids, analysis);
}

} // namespace

// Analyzes imported scene UUIDs and prepares collision-safe reference remaps.
MvrMergeAnalysis AnalyzeImportedSceneMerge(const MvrScene &target,
                                           const MvrScene &imported) {
  MvrMergeAnalysis analysis;
  std::unordered_set<std::string> usedUuids = CollectUsedUuids(target);

  PrepareObjectTable(imported.positions, usedUuids, analysis);
  PrepareObjectTable(imported.symdefFiles, usedUuids, analysis);
  PrepareObjectTable(imported.symdefTypes, usedUuids, analysis);
  PrepareObjectTable(imported.symdefMatrices, usedUuids, analysis);
  PrepareObjectTable(imported.symdefGeometries, usedUuids, analysis);
  PrepareObjectTable(imported.fixtures, usedUuids, analysis);
  PrepareObjectTable(imported.trusses, usedUuids, analysis);
  PrepareObjectTable(imported.supports, usedUuids, analysis);
  PrepareObjectTable(imported.sceneObjects, usedUuids, analysis);
  PrepareObjectTable(imported.groupObjects, usedUuids, analysis);
  PrepareObjectTable(imported.layers, usedUuids, analysis);
  return analysis;
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
