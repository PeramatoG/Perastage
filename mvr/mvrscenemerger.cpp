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
#include "mvrscenemerger.h"

#include "uuidutils.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mvr {
namespace {

// Returns every UUID already owned by the target scene across all object tables.
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

// Returns a collision-free UUID for an imported item and records reference remaps.
std::string ResolveImportedUuid(
    const std::string &uuid, std::unordered_set<std::string> &usedUuids,
    std::unordered_map<std::string, std::string> &uuidMap,
    MvrSceneMergeResult &result) {
  if (uuid.empty())
    return uuid;

  if (usedUuids.insert(uuid).second) {
    uuidMap.emplace(uuid, uuid);
    return uuid;
  }

  std::string replacement;
  do {
    replacement = GenerateUuid();
  } while (!usedUuids.insert(replacement).second);

  uuidMap[uuid] = replacement;
  ++result.uuidCollisionsResolved;
  return replacement;
}

// Returns the remapped UUID when an imported reference points at a remapped object.
std::string RemapUuidReference(
    const std::string &uuid,
    const std::unordered_map<std::string, std::string> &uuidMap) {
  const auto it = uuidMap.find(uuid);
  if (it == uuidMap.end())
    return uuid;
  return it->second;
}

// Records UUID remaps for an imported object map without mutating scene content yet.
template <typename T>
void PrepareObjectTable(const std::unordered_map<std::string, T> &imported,
                        std::unordered_set<std::string> &usedUuids,
                        std::unordered_map<std::string, std::string> &uuidMap,
                        MvrSceneMergeResult &result) {
  for (const auto &entry : imported)
    (void)ResolveImportedUuid(entry.first, usedUuids, uuidMap, result);
}

// Adds imported lookup-table entries to the target scene using the prepared UUID remap table.
template <typename T>
void MergeLookupTable(std::unordered_map<std::string, T> &target,
                      const std::unordered_map<std::string, T> &imported,
                      const std::unordered_map<std::string, std::string>
                          &uuidMap) {
  for (const auto &[uuid, value] : imported)
    target[RemapUuidReference(uuid, uuidMap)] = value;
}

// Adds imported objects with a UUID field to the target object map after updating the object's UUID.
template <typename T>
std::size_t MergeObjectTable(std::unordered_map<std::string, T> &target,
                             const std::unordered_map<std::string, T> &imported,
                             const std::unordered_map<std::string, std::string>
                                 &uuidMap) {
  std::size_t added = 0;
  for (const auto &[uuid, object] : imported) {
    const std::string resolvedUuid = RemapUuidReference(uuid, uuidMap);
    T merged = object;
    merged.uuid = resolvedUuid;
    target[resolvedUuid] = std::move(merged);
    ++added;
  }
  return added;
}

// Updates references inside an imported scene copy after the full UUID remap table has been built.
void RemapImportedReferences(
    MvrScene &scene,
    const std::unordered_map<std::string, std::string> &uuidMap) {
  for (auto &[uuid, fixture] : scene.fixtures)
    fixture.position = RemapUuidReference(fixture.position, uuidMap);

  for (auto &[uuid, truss] : scene.trusses) {
    truss.position = RemapUuidReference(truss.position, uuidMap);
    truss.sourceSymdefUuid = RemapUuidReference(truss.sourceSymdefUuid, uuidMap);
    truss.parentGroupUuid = RemapUuidReference(truss.parentGroupUuid, uuidMap);
  }

  for (auto &[uuid, support] : scene.supports) {
    support.position = RemapUuidReference(support.position, uuidMap);
    support.motorFixtureUuid = RemapUuidReference(support.motorFixtureUuid, uuidMap);
  }

  for (auto &[uuid, group] : scene.groupObjects) {
    group.parentGroupUuid = RemapUuidReference(group.parentGroupUuid, uuidMap);
    for (auto &child : group.children)
      child.uuid = RemapUuidReference(child.uuid, uuidMap);
  }

  for (auto &[uuid, layer] : scene.layers) {
    for (auto &childUuid : layer.childUUIDs)
      childUuid = RemapUuidReference(childUuid, uuidMap);
  }
}

} // namespace

// Combines imported MVR scene content into the target while preserving existing objects.
MvrSceneMergeResult MergeImportedSceneIntoCurrent(MvrScene &target,
                                                  const MvrScene &imported) {
  MvrSceneMergeResult result;
  std::unordered_set<std::string> usedUuids = CollectUsedUuids(target);
  std::unordered_map<std::string, std::string> uuidMap;

  PrepareObjectTable(imported.positions, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.symdefFiles, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.symdefTypes, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.symdefMatrices, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.symdefGeometries, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.fixtures, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.trusses, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.supports, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.sceneObjects, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.groupObjects, usedUuids, uuidMap, result);
  PrepareObjectTable(imported.layers, usedUuids, uuidMap, result);

  MvrScene importedCopy = imported;
  RemapImportedReferences(importedCopy, uuidMap);

  MergeLookupTable(target.positions, importedCopy.positions, uuidMap);
  MergeLookupTable(target.symdefFiles, importedCopy.symdefFiles, uuidMap);
  MergeLookupTable(target.symdefTypes, importedCopy.symdefTypes, uuidMap);
  MergeLookupTable(target.symdefMatrices, importedCopy.symdefMatrices, uuidMap);
  MergeLookupTable(target.symdefGeometries, importedCopy.symdefGeometries, uuidMap);

  result.fixturesAdded =
      MergeObjectTable(target.fixtures, importedCopy.fixtures, uuidMap);
  result.trussesAdded =
      MergeObjectTable(target.trusses, importedCopy.trusses, uuidMap);
  result.supportsAdded =
      MergeObjectTable(target.supports, importedCopy.supports, uuidMap);
  result.sceneObjectsAdded = MergeObjectTable(
      target.sceneObjects, importedCopy.sceneObjects, uuidMap);
  result.groupObjectsAdded = MergeObjectTable(
      target.groupObjects, importedCopy.groupObjects, uuidMap);
  result.layersAdded =
      MergeObjectTable(target.layers, importedCopy.layers, uuidMap);

  if (target.basePath.empty())
    target.basePath = imported.basePath;
  if (target.provider.empty())
    target.provider = imported.provider;
  if (target.providerVersion.empty())
    target.providerVersion = imported.providerVersion;
  return result;
}

} // namespace mvr
