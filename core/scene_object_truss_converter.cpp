/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "scene_object_truss_converter.h"

#include "groupobject.h"
#include "matrixutils.h"
#include "mvrscene.h"
#include "sceneobject.h"
#include "truss.h"

#include <algorithm>

namespace {

// Returns the model identity used to group scene objects for conversion.
std::string SceneObjectModelKey(const SceneObject &object) {
  return object.GetPrimaryModel();
}

// Builds a truss from a scene object while preserving MVR placement metadata.
Truss BuildTrussFromSceneObject(const SceneObject &object,
                                const std::string &modelFile) {
  Truss truss;
  truss.uuid = object.uuid;
  truss.name = object.name;
  truss.classing = object.classing;
  truss.identifierSource = object.identifierSource;
  truss.fixtureIdText = object.fixtureIdText;
  truss.fixtureIdNumeric = object.fixtureIdNumeric;
  truss.unitNumber = object.unitNumber;
  truss.matrixSource = object.matrixSource;
  truss.symbolFile = modelFile;
  truss.modelFile = modelFile;
  truss.layer = object.layer;
  truss.layerUuid = object.layerUuid;
  truss.transform = object.transform;
  truss.localTransform = object.localTransform;
  truss.hasLocalTransform = object.hasLocalTransform;
  truss.parentGroupUuid = object.parentGroupUuid;
  truss.sourceRepresentation = Truss::GeometryRepresentation::Geometry3D;
  if (!object.geometries.empty()) {
    truss.sourceSymbolUuid = object.geometries.front().sourceSymbolUuid;
    truss.sourceSymdefUuid = object.geometries.front().sourceSymdefUuid;
    truss.sourceGeometryMatrix = object.geometries.front().localTransform;
  }
  return truss;
}

// Repoints existing group child references from SceneObject to Truss.
void RepointGroupChildrenToTrusses(MvrScene &scene,
                                   const std::vector<std::string> &uuids) {
  for (auto &[groupUuid, group] : scene.groupObjects) {
    (void)groupUuid;
    for (GroupObjectChildRef &child : group.children) {
      if (child.type == MvrNodeType::SceneObject &&
          std::find(uuids.begin(), uuids.end(), child.uuid) != uuids.end()) {
        child.type = MvrNodeType::Truss;
      }
    }
  }
}

} // namespace

// Resolves the deterministic same-model conversion scope for one scene object.
SceneObjectToTrussConversionScope ResolveSceneObjectsWithSameModelToTrusses(
    const MvrScene &scene, const std::string &sourceSceneObjectUuid) {
  SceneObjectToTrussConversionScope scope;
  const auto sourceIt = scene.sceneObjects.find(sourceSceneObjectUuid);
  if (sourceIt == scene.sceneObjects.end())
    return scope;

  scope.modelFile = SceneObjectModelKey(sourceIt->second);
  if (scope.modelFile.empty())
    return scope;

  for (const auto &[uuid, object] : scene.sceneObjects) {
    if (SceneObjectModelKey(object) == scope.modelFile)
      scope.sceneObjectUuids.push_back(uuid);
  }
  std::sort(scope.sceneObjectUuids.begin(), scope.sceneObjectUuids.end());
  // Opaque evidence keeps source UUIDs after merges; it cannot be reassigned safely.
  if (!scene.preservationIssues.empty())
    scope.error = "Resolve retained scene source conflicts before conversion: " + scene.preservationIssues.front().sourceLocation;
  const auto identity = MatrixUtils::Identity();
  for (const auto &uuid : scope.sceneObjectUuids) {
    const auto &object = scene.sceneObjects.at(uuid);
    const auto geometry = object.geometries.empty() ? identity : object.geometries.front().localTransform;
    const bool identityGeometry = geometry.u == identity.u && geometry.v == identity.v &&
        geometry.w == identity.w && geometry.o == identity.o;
    if (!identityGeometry || object.kind != SceneObjectKind::SceneObject || object.geometries.size() > 1 ||
        (!object.geometries.empty() && (!object.geometries.front().sourceSymdefUuid.empty() ||
                                       !object.geometries.front().sourceSymbolUuid.empty())))
      scope.error = "SceneObject " + uuid + ": conversion requires one direct model and cannot discard FocusPoint, Symbol/Symdef or nonidentity geometry ownership.";
  }
  return scope;
}

// Converts all scene objects sharing the selected object's model into trusses.
SceneObjectToTrussConversionResult ConvertSceneObjectsWithSameModelToTrusses(
    MvrScene &scene, const std::string &sourceSceneObjectUuid) {
  SceneObjectToTrussConversionResult result;
  const SceneObjectToTrussConversionScope scope =
      ResolveSceneObjectsWithSameModelToTrusses(scene, sourceSceneObjectUuid);
  result.modelFile = scope.modelFile;
  result.error = scope.error;
  if (!scope.error.empty()) return result;
  if (scope.sceneObjectUuids.empty())
    return result;
  if (std::any_of(scope.sceneObjectUuids.begin(), scope.sceneObjectUuids.end(),
                  [&](const std::string &uuid) {
                    return scene.trusses.contains(uuid);
                  }))
    return result;

  std::vector<SceneObject> objectsToConvert;
  objectsToConvert.reserve(scope.sceneObjectUuids.size());
  for (const std::string &uuid : scope.sceneObjectUuids)
    objectsToConvert.push_back(scene.sceneObjects.at(uuid));

  result.convertedUuids.reserve(objectsToConvert.size());
  for (const SceneObject &object : objectsToConvert) {
    scene.trusses[object.uuid] =
        BuildTrussFromSceneObject(object, result.modelFile);
    scene.sceneObjects.erase(object.uuid);
    result.convertedUuids.push_back(object.uuid);
  }

  RepointGroupChildrenToTrusses(scene, result.convertedUuids);
  return result;
}
