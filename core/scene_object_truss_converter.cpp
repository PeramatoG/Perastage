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
  truss.symbolFile = modelFile;
  truss.modelFile = modelFile;
  truss.layer = object.layer;
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

// Converts all scene objects sharing the selected object's model into trusses.
SceneObjectToTrussConversionResult ConvertSceneObjectsWithSameModelToTrusses(
    MvrScene &scene, const std::string &sourceSceneObjectUuid) {
  SceneObjectToTrussConversionResult result;
  const auto sourceIt = scene.sceneObjects.find(sourceSceneObjectUuid);
  if (sourceIt == scene.sceneObjects.end())
    return result;

  result.modelFile = SceneObjectModelKey(sourceIt->second);
  if (result.modelFile.empty())
    return result;

  std::vector<SceneObject> objectsToConvert;
  for (const auto &[uuid, object] : scene.sceneObjects) {
    if (SceneObjectModelKey(object) == result.modelFile)
      objectsToConvert.push_back(object);
  }

  result.convertedUuids.reserve(objectsToConvert.size());
  for (const SceneObject &object : objectsToConvert) {
    scene.trusses[object.uuid] = BuildTrussFromSceneObject(object, result.modelFile);
    scene.sceneObjects.erase(object.uuid);
    result.convertedUuids.push_back(object.uuid);
  }

  RepointGroupChildrenToTrusses(scene, result.convertedUuids);
  return result;
}
