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

#include "mvr_merge_resource_rewriter.h"

#include <algorithm>
#include <cctype>
#include <utility>

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

// Normalizes fixture type names for merge decision lookup.
std::string NormalizeFixtureTypeName(const std::string &typeName) {
  return ToLowerAscii(TrimAscii(typeName));
}

// Adds imported lookup-table entries with prepared UUID remaps.
template <typename T>
void MergeLookupTable(std::unordered_map<std::string, T> &target,
                      const std::unordered_map<std::string, T> &imported,
                      const MvrMergeAnalysis &analysis) {
  for (const auto &[uuid, value] : imported) {
    if (ShouldSkipImportedUuid(uuid, analysis))
      continue;
    target[RemapImportedUuidReference(uuid, analysis)] = value;
  }
}

// Adds imported objects after updating each object UUID.
template <typename T>
std::size_t MergeObjectTable(std::unordered_map<std::string, T> &target,
                             const std::unordered_map<std::string, T> &imported,
                             const MvrMergeAnalysis &analysis) {
  std::size_t added = 0;
  for (const auto &[uuid, object] : imported) {
    if (ShouldSkipImportedUuid(uuid, analysis))
      continue;
    const std::string resolvedUuid = RemapImportedUuidReference(uuid, analysis);
    T merged = object;
    merged.uuid = resolvedUuid;
    target[resolvedUuid] = std::move(merged);
    ++added;
  }
  return added;
}

// Applies imported layer name remaps to object layer references.
template <typename T>
void RemapObjectLayerNames(std::unordered_map<std::string, T> &objects,
                           const MvrMergeAnalysis &analysis) {
  for (auto &[uuid, object] : objects) {
    const auto renameIt = analysis.incomingLayerNameRenames.find(object.layer);
    if (renameIt != analysis.incomingLayerNameRenames.end())
      object.layer = renameIt->second;
  }
}

// Adds imported layers after applying layer-specific UUID and name rules.
std::size_t
MergeLayerTable(std::unordered_map<std::string, Layer> &target,
                const std::unordered_map<std::string, Layer> &imported,
                const MvrMergeAnalysis &analysis) {
  std::size_t added = 0;
  for (const auto &[uuid, layer] : imported) {
    if (analysis.skippedIncomingLayerUuids.contains(uuid))
      continue;
    const auto uuidIt = analysis.layerUuidMap.find(uuid);
    const std::string resolvedUuid = uuidIt == analysis.layerUuidMap.end()
                                         ? uuid
                                         : uuidIt->second;
    Layer merged = layer;
    merged.uuid = resolvedUuid;
    const auto renameIt = analysis.incomingLayerNameRenames.find(merged.name);
    if (renameIt != analysis.incomingLayerNameRenames.end())
      merged.name = renameIt->second;
    target[resolvedUuid] = std::move(merged);
    ++added;
  }
  return added;
}

// Applies selected fixture type conflict decisions to imported fixtures.
void ApplyFixtureTypeDecisions(MvrScene &scene,
                               const MvrMergeAnalysis &analysis) {
  for (auto &[uuid, fixture] : scene.fixtures) {
    const std::string normalized = NormalizeFixtureTypeName(fixture.typeName);
    const auto decisionIt = analysis.fixtureTypeDecisions.find(normalized);
    if (decisionIt == analysis.fixtureTypeDecisions.end())
      continue;
    if (decisionIt->second ==
        MvrMergeFixtureTypeDecision::UseCurrentDefinition) {
      const auto currentIt = analysis.currentFixtureTypes.find(normalized);
      if (currentIt == analysis.currentFixtureTypes.end())
        continue;
      fixture.typeName = currentIt->second.typeName;
      fixture.gdtfSpec = currentIt->second.gdtfSpec;
      fixture.gdtfMode = currentIt->second.gdtfMode;
    } else if (decisionIt->second ==
               MvrMergeFixtureTypeDecision::RenameIncomingType) {
      const auto renameIt =
          analysis.incomingFixtureTypeRenames.find(normalized);
      if (renameIt != analysis.incomingFixtureTypeRenames.end())
        fixture.typeName = renameIt->second;
    }
  }
}

// Updates imported scene references after building the UUID remap table.
void RemapImportedReferences(MvrScene &scene,
                             const MvrMergeAnalysis &analysis) {
  for (auto &[uuid, fixture] : scene.fixtures) {
    fixture.position = RemapImportedUuidReference(fixture.position, analysis);
    fixture.focus = RemapImportedUuidReference(fixture.focus, analysis);
    fixture.parentGroupUuid =
        RemapImportedUuidReference(fixture.parentGroupUuid, analysis);
  }

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
    support.parentGroupUuid =
        RemapImportedUuidReference(support.parentGroupUuid, analysis);
  }

  for (auto &[uuid, object] : scene.sceneObjects) {
    object.parentGroupUuid =
        RemapImportedUuidReference(object.parentGroupUuid, analysis);
  }

  for (auto &[uuid, group] : scene.groupObjects) {
    group.parentGroupUuid =
        RemapImportedUuidReference(group.parentGroupUuid, analysis);
    for (auto &child : group.children)
      child.uuid = RemapImportedUuidReference(child.uuid, analysis);
  }

  RemapObjectLayerNames(scene.fixtures, analysis);
  RemapObjectLayerNames(scene.trusses, analysis);
  RemapObjectLayerNames(scene.supports, analysis);
  RemapObjectLayerNames(scene.sceneObjects, analysis);
  RemapObjectLayerNames(scene.groupObjects, analysis);

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
  result.nonObjectLookupConflictsResolved =
      analysis.nonObjectLookupConflictsResolved;
  if (HasBlockingFixtureTypeConflicts(analysis)) {
    result.fixtureTypeConflictsBlocked = analysis.fixtureTypeConflicts.size();
    return result;
  }
  result.fixtureUuidRemap = analysis.fixtureUuidRemap;

  MvrScene importedCopy = imported;
  RewriteImportedSceneResourceReferences(target, importedCopy);
  ApplyFixtureTypeDecisions(importedCopy, analysis);
  RemapImportedReferences(importedCopy, analysis);

  MergeLookupTable(target.positions, importedCopy.positions, analysis);
  MergeLookupTable(target.symdefFiles, importedCopy.symdefFiles, analysis);
  MergeLookupTable(target.symdefTypes, importedCopy.symdefTypes, analysis);
  MergeLookupTable(target.symdefMatrices, importedCopy.symdefMatrices,
                   analysis);
  MergeLookupTable(target.symdefGeometries, importedCopy.symdefGeometries,
                   analysis);

  result.fixturesAdded =
      MergeObjectTable(target.fixtures, importedCopy.fixtures, analysis);
  result.trussesAdded =
      MergeObjectTable(target.trusses, importedCopy.trusses, analysis);
  result.supportsAdded =
      MergeObjectTable(target.supports, importedCopy.supports, analysis);
  result.sceneObjectsAdded = MergeObjectTable(
      target.sceneObjects, importedCopy.sceneObjects, analysis);
  result.groupObjectsAdded = MergeObjectTable(
      target.groupObjects, importedCopy.groupObjects, analysis);
  result.layersAdded =
      MergeLayerTable(target.layers, importedCopy.layers, analysis);

  if (target.provider.empty())
    target.provider = imported.provider;
  if (target.providerVersion.empty())
    target.providerVersion = imported.providerVersion;
  return result;
}

} // namespace mvr
