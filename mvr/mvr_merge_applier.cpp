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
#include "mvr_merge_applier.h"

#include <utility>

namespace mvr {
namespace {

// Adds imported lookup-table entries to the target scene using prepared UUID remaps.
template <typename T>
void MergeLookupTable(std::unordered_map<std::string, T> &target,
                      const std::unordered_map<std::string, T> &imported,
                      const MvrMergeAnalysis &analysis) {
  for (const auto &[uuid, value] : imported)
    target[RemapImportedUuidReference(uuid, analysis)] = value;
}

// Adds imported objects with UUID fields to the target after updating each object's UUID.
template <typename T>
std::size_t MergeObjectTable(std::unordered_map<std::string, T> &target,
                             const std::unordered_map<std::string, T> &imported,
                             const MvrMergeAnalysis &analysis) {
  std::size_t added = 0;
  for (const auto &[uuid, object] : imported) {
    const std::string resolvedUuid = RemapImportedUuidReference(uuid, analysis);
    T merged = object;
    merged.uuid = resolvedUuid;
    target[resolvedUuid] = std::move(merged);
    ++added;
  }
  return added;
}

// Updates references inside an imported scene copy after the full UUID remap table has been built.
void RemapImportedReferences(MvrScene &scene, const MvrMergeAnalysis &analysis) {
  for (auto &[uuid, fixture] : scene.fixtures)
    fixture.position = RemapImportedUuidReference(fixture.position, analysis);

  for (auto &[uuid, truss] : scene.trusses) {
    truss.position = RemapImportedUuidReference(truss.position, analysis);
    truss.sourceSymdefUuid =
        RemapImportedUuidReference(truss.sourceSymdefUuid, analysis);
    truss.parentGroupUuid =
        RemapImportedUuidReference(truss.parentGroupUuid, analysis);
  }

  for (auto &[uuid, support] : scene.supports) {
    support.position = RemapImportedUuidReference(support.position, analysis);
    support.motorFixtureUuid =
        RemapImportedUuidReference(support.motorFixtureUuid, analysis);
  }

  for (auto &[uuid, group] : scene.groupObjects) {
    group.parentGroupUuid =
        RemapImportedUuidReference(group.parentGroupUuid, analysis);
    for (auto &child : group.children)
      child.uuid = RemapImportedUuidReference(child.uuid, analysis);
  }

  for (auto &[uuid, layer] : scene.layers) {
    for (auto &childUuid : layer.childUUIDs)
      childUuid = RemapImportedUuidReference(childUuid, analysis);
  }
}

} // namespace

// Applies an analyzed imported scene merge into the target scene.
MvrSceneMergeResult ApplyImportedSceneMerge(MvrScene &target,
                                            const MvrScene &imported,
                                            const MvrMergeAnalysis &analysis) {
  MvrSceneMergeResult result;
  result.uuidCollisionsResolved = analysis.uuidCollisionsResolved;

  MvrScene importedCopy = imported;
  RemapImportedReferences(importedCopy, analysis);

  MergeLookupTable(target.positions, importedCopy.positions, analysis);
  MergeLookupTable(target.symdefFiles, importedCopy.symdefFiles, analysis);
  MergeLookupTable(target.symdefTypes, importedCopy.symdefTypes, analysis);
  MergeLookupTable(target.symdefMatrices, importedCopy.symdefMatrices, analysis);
  MergeLookupTable(target.symdefGeometries, importedCopy.symdefGeometries,
                   analysis);

  result.fixturesAdded =
      MergeObjectTable(target.fixtures, importedCopy.fixtures, analysis);
  result.trussesAdded =
      MergeObjectTable(target.trusses, importedCopy.trusses, analysis);
  result.supportsAdded =
      MergeObjectTable(target.supports, importedCopy.supports, analysis);
  result.sceneObjectsAdded =
      MergeObjectTable(target.sceneObjects, importedCopy.sceneObjects, analysis);
  result.groupObjectsAdded =
      MergeObjectTable(target.groupObjects, importedCopy.groupObjects, analysis);
  result.layersAdded =
      MergeObjectTable(target.layers, importedCopy.layers, analysis);

  if (target.basePath.empty())
    target.basePath = imported.basePath;
  if (target.provider.empty())
    target.provider = imported.provider;
  if (target.providerVersion.empty())
    target.providerVersion = imported.providerVersion;
  return result;
}

} // namespace mvr
