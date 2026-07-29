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
#include "scene_grouping.h"

#include "matrixutils.h"
#include "uuidutils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <unordered_set>

namespace scene_grouping {
namespace {

struct SelectedObjectRef {
  MvrNodeType type = MvrNodeType::Fixture;
  std::string uuid;
  std::string layer;
  std::string parentGroupUuid;
  Matrix worldTransform{};
};

// Returns the determinant of the linear 3x3 part of a Matrix.
float Determinant3x3(const Matrix &m) {
  return m.u[0] * (m.v[1] * m.w[2] - m.w[1] * m.v[2]) -
         m.v[0] * (m.u[1] * m.w[2] - m.w[1] * m.u[2]) +
         m.w[0] * (m.u[1] * m.v[2] - m.v[1] * m.u[2]);
}

// Builds the inverse transform used to preserve world placement during
// reparenting.
Matrix InverseMatrix(const Matrix &m) {
  const float det = Determinant3x3(m);
  if (std::fabs(det) < 1e-8f)
    return MatrixUtils::Identity();

  const float invDet = 1.0f / det;
  const float a00 = m.u[0], a01 = m.v[0], a02 = m.w[0];
  const float a10 = m.u[1], a11 = m.v[1], a12 = m.w[1];
  const float a20 = m.u[2], a21 = m.v[2], a22 = m.w[2];

  Matrix inv;
  inv.u = {(a11 * a22 - a12 * a21) * invDet, (a12 * a20 - a10 * a22) * invDet,
           (a10 * a21 - a11 * a20) * invDet};
  inv.v = {(a02 * a21 - a01 * a22) * invDet, (a00 * a22 - a02 * a20) * invDet,
           (a01 * a20 - a00 * a21) * invDet};
  inv.w = {(a01 * a12 - a02 * a11) * invDet, (a02 * a10 - a00 * a12) * invDet,
           (a00 * a11 - a01 * a10) * invDet};

  for (int i = 0; i < 3; ++i) {
    inv.o[i] = -(inv.u[i] * m.o[0] + inv.v[i] * m.o[1] + inv.w[i] * m.o[2]);
  }
  return inv;
}

// Appends a UUID to a vector while preserving insertion order and uniqueness.
void AppendUnique(std::vector<std::string> &values,
                  std::unordered_set<std::string> &seen,
                  const std::string &uuid) {
  if (!uuid.empty() && seen.insert(uuid).second)
    values.push_back(uuid);
}

// Removes a child reference from every group that currently contains it.
void RemoveChildFromGroups(MvrScene &scene, const MvrNodeType type,
                           const std::string &uuid) {
  for (auto &[groupUuid, group] : scene.groupObjects) {
    (void)groupUuid;
    group.children.erase(
        std::remove_if(group.children.begin(), group.children.end(),
                       [&](const GroupObjectChildRef &ref) {
                         return ref.type == type && ref.uuid == uuid;
                       }),
        group.children.end());
  }
}

// Returns true when a group has an empty child list after cleanup.
bool IsEmptyGroup(const MvrScene &scene, const std::string &groupUuid) {
  const auto it = scene.groupObjects.find(groupUuid);
  return it != scene.groupObjects.end() && it->second.children.empty();
}

// Removes an empty group and clears its reference from its own parent group.
void RemoveEmptyGroup(MvrScene &scene, const std::string &groupUuid) {
  auto it = scene.groupObjects.find(groupUuid);
  if (it == scene.groupObjects.end() || !it->second.children.empty())
    return;
  const std::string parentUuid = it->second.parentGroupUuid;
  scene.groupObjects.erase(it);
  if (!parentUuid.empty())
    RemoveChildFromGroups(scene, MvrNodeType::GroupObject, groupUuid);
}

// Reads a parent-group UUID from the selected object type.
std::string ParentGroupUuidFor(const MvrScene &scene, const MvrNodeType type,
                               const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(uuid);
    return it == scene.fixtures.end() ? std::string{}
                                      : it->second.parentGroupUuid;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(uuid);
    return it == scene.trusses.end() ? std::string{}
                                     : it->second.parentGroupUuid;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(uuid);
    return it == scene.supports.end() ? std::string{}
                                      : it->second.parentGroupUuid;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(uuid);
    return it == scene.sceneObjects.end() ? std::string{}
                                          : it->second.parentGroupUuid;
  }
  case MvrNodeType::GroupObject:
    break;
  }
  return {};
}

// Adds existing selected objects to a normalized reference list.
std::vector<SelectedObjectRef>
CollectSelectedObjects(const MvrScene &scene,
                       const ObjectSelection &selection) {
  std::vector<SelectedObjectRef> refs;
  std::set<std::pair<MvrNodeType, std::string>> seen;

  auto append = [&](const auto &table, const std::vector<std::string> &uuids,
                    MvrNodeType type) {
    for (const auto &uuid : uuids) {
      auto it = table.find(uuid);
      if (it == table.end() || !seen.insert({type, uuid}).second)
        continue;
      refs.push_back({type, uuid, it->second.layer, it->second.parentGroupUuid,
                      it->second.transform});
    }
  };

  append(scene.fixtures, selection.fixtures, MvrNodeType::Fixture);
  append(scene.trusses, selection.trusses, MvrNodeType::Truss);
  append(scene.supports, selection.supports, MvrNodeType::Support);
  append(scene.sceneObjects, selection.sceneObjects, MvrNodeType::SceneObject);
  return refs;
}

// Computes a transform located at the selected objects' world-space center.
Matrix BuildGroupWorldTransform(const std::vector<SelectedObjectRef> &objects) {
  Matrix transform = MatrixUtils::Identity();
  if (objects.empty())
    return transform;

  for (const auto &object : objects) {
    for (int axis = 0; axis < 3; ++axis)
      transform.o[axis] += object.worldTransform.o[axis];
  }
  for (float &component : transform.o)
    component /= static_cast<float>(objects.size());
  return transform;
}

// Returns a common direct parent if every selected object has the same one.
std::string
CommonParentGroupUuid(const std::vector<SelectedObjectRef> &objects) {
  if (objects.empty())
    return {};
  const std::string parent = objects.front().parentGroupUuid;
  for (const auto &object : objects) {
    if (object.parentGroupUuid != parent)
      return {};
  }
  return parent;
}

// Returns the layer owned by a scene node when the node exists.
std::optional<std::string> LayerForObject(const MvrScene &scene,
                                          const MvrNodeType type,
                                          const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(uuid);
    return it == scene.fixtures.end() ? std::nullopt
                                      : std::optional<std::string>(it->second.layer);
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(uuid);
    return it == scene.trusses.end() ? std::nullopt
                                     : std::optional<std::string>(it->second.layer);
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(uuid);
    return it == scene.supports.end() ? std::nullopt
                                      : std::optional<std::string>(it->second.layer);
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(uuid);
    return it == scene.sceneObjects.end()
               ? std::nullopt
               : std::optional<std::string>(it->second.layer);
  }
  case MvrNodeType::GroupObject: {
    auto it = scene.groupObjects.find(uuid);
    return it == scene.groupObjects.end() ? std::nullopt
                                          : std::optional<std::string>(it->second.layer);
  }
  }
  return std::nullopt;
}

// Returns the authoritative layer for a GroupObject-based grouping operation.
std::string AuthoritativeGroupLayer(const MvrScene &scene,
                                    const std::vector<SelectedObjectRef> &objects,
                                    const std::string &parentGroupUuid) {
  if (!parentGroupUuid.empty()) {
    auto parentIt = scene.groupObjects.find(parentGroupUuid);
    if (parentIt != scene.groupObjects.end())
      return parentIt->second.layer;
  }
  for (const auto &object : objects) {
    if (object.type == MvrNodeType::Truss)
      return object.layer;
  }
  for (const auto &object : objects) {
    if (object.type == MvrNodeType::GroupObject) {
      auto layer = LayerForObject(scene, object.type, object.uuid);
      if (layer)
        return *layer;
    }
  }
  return objects.empty() ? std::string{} : objects.front().layer;
}

// Applies the authoritative parent GroupObject layer to one child node.
bool ApplyGroupLayerToChild(MvrScene &scene, const GroupObjectChildRef &child,
                            const std::string &layer) {
  switch (child.type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(child.uuid);
    if (it == scene.fixtures.end() || it->second.layer == layer)
      return false;
    it->second.layer = layer;
    return true;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(child.uuid);
    if (it == scene.trusses.end() || it->second.layer == layer)
      return false;
    it->second.layer = layer;
    return true;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(child.uuid);
    if (it == scene.supports.end() || it->second.layer == layer)
      return false;
    it->second.layer = layer;
    return true;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(child.uuid);
    if (it == scene.sceneObjects.end() || it->second.layer == layer)
      return false;
    it->second.layer = layer;
    return true;
  }
  case MvrNodeType::GroupObject: {
    auto it = scene.groupObjects.find(child.uuid);
    if (it == scene.groupObjects.end() || it->second.layer == layer)
      return false;
    it->second.layer = layer;
    return true;
  }
  }
  return false;
}

// Updates an object's parent group and local transform fields.
void ApplyParentAndLocalTransform(MvrScene &scene,
                                  const SelectedObjectRef &object,
                                  const std::string &parentGroupUuid,
                                  const Matrix &localTransform) {
  switch (object.type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(object.uuid);
    if (it != scene.fixtures.end()) {
      it->second.parentGroupUuid = parentGroupUuid;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(object.uuid);
    if (it != scene.trusses.end()) {
      it->second.parentGroupUuid = parentGroupUuid;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(object.uuid);
    if (it != scene.supports.end()) {
      it->second.parentGroupUuid = parentGroupUuid;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(object.uuid);
    if (it != scene.sceneObjects.end()) {
      it->second.parentGroupUuid = parentGroupUuid;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::GroupObject: {
    auto it = scene.groupObjects.find(object.uuid);
    if (it != scene.groupObjects.end()) {
      it->second.parentGroupUuid = parentGroupUuid;
      it->second.localTransform = localTransform;
    }
    break;
  }
  }
}

// Moves the newly grouped object to the parent GroupObject layer.
void ApplyParentAndLayer(MvrScene &scene, const SelectedObjectRef &object,
                         const std::string &parentGroupUuid,
                         const Matrix &localTransform,
                         const std::string &parentLayer) {
  ApplyParentAndLocalTransform(scene, object, parentGroupUuid, localTransform);
  ApplyGroupLayerToChild(scene, {object.type, object.uuid}, parentLayer);
}

// Recursively applies parent group layer ownership through nested groups.
std::size_t SynchronizeGroupObjectLayerOwnershipFrom(MvrScene &scene,
                                                     const std::string &groupUuid) {
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return 0;

  std::size_t repairedCount = 0;
  const std::string groupLayer = groupIt->second.layer;
  const std::vector<GroupObjectChildRef> children = groupIt->second.children;
  for (const auto &child : children) {
    if (ApplyGroupLayerToChild(scene, child, groupLayer))
      ++repairedCount;
    if (child.type == MvrNodeType::GroupObject)
      repairedCount += SynchronizeGroupObjectLayerOwnershipFrom(scene, child.uuid);
  }
  return repairedCount;
}

// Adds an affected child UUID to the operation result for selection
// restoration.
void AppendAffected(OperationResult &result, const MvrNodeType type,
                    const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture:
    if (std::find(result.affectedFixtures.begin(),
                  result.affectedFixtures.end(),
                  uuid) == result.affectedFixtures.end())
      result.affectedFixtures.push_back(uuid);
    break;
  case MvrNodeType::Truss:
    if (std::find(result.affectedTrusses.begin(), result.affectedTrusses.end(),
                  uuid) == result.affectedTrusses.end())
      result.affectedTrusses.push_back(uuid);
    break;
  case MvrNodeType::Support:
    if (std::find(result.affectedSupports.begin(),
                  result.affectedSupports.end(),
                  uuid) == result.affectedSupports.end())
      result.affectedSupports.push_back(uuid);
    break;
  case MvrNodeType::SceneObject:
    if (std::find(result.affectedSceneObjects.begin(),
                  result.affectedSceneObjects.end(),
                  uuid) == result.affectedSceneObjects.end())
      result.affectedSceneObjects.push_back(uuid);
    break;
  case MvrNodeType::GroupObject:
    break;
  }
}

// Multiplies a matrix point by the transform without applying translation scale
// changes.
std::array<float, 3> TransformPoint(const Matrix &m,
                                    const std::array<float, 3> &point) {
  return {m.u[0] * point[0] + m.v[0] * point[1] + m.w[0] * point[2] + m.o[0],
          m.u[1] * point[0] + m.v[1] * point[1] + m.w[1] * point[2] + m.o[1],
          m.u[2] * point[0] + m.v[2] * point[1] + m.w[2] * point[2] + m.o[2]};
}

// Returns the group parent UUID for a direct scene child reference.
std::string ParentGroupUuidForTarget(const MvrScene &scene,
                                     const SceneTransformTarget &target) {
  if (target.type == MvrNodeType::GroupObject) {
    auto it = scene.groupObjects.find(target.uuid);
    return it == scene.groupObjects.end() ? std::string{}
                                          : it->second.parentGroupUuid;
  }
  return ParentGroupUuidFor(scene, target.type, target.uuid);
}

// Returns the highest group UUID for a selected object when one exists.
std::string ResolveRootGroupUuid(const MvrScene &scene,
                                 const SelectedObjectRef &object) {
  std::string groupUuid = object.parentGroupUuid;
  if (groupUuid.empty())
    return {};

  std::unordered_set<std::string> visited;
  std::string rootUuid;
  while (!groupUuid.empty() && visited.insert(groupUuid).second) {
    auto it = scene.groupObjects.find(groupUuid);
    if (it == scene.groupObjects.end())
      break;
    rootUuid = groupUuid;
    groupUuid = it->second.parentGroupUuid;
  }
  return rootUuid;
}

// Returns the highest group ancestor for a selected object when one exists.
SceneTransformTarget ResolveTransformRoot(const MvrScene &scene,
                                          const SelectedObjectRef &object) {
  if (object.type == MvrNodeType::Fixture)
    return {object.type, object.uuid};

  const std::string rootUuid = ResolveRootGroupUuid(scene, object);
  if (rootUuid.empty())
    return {object.type, object.uuid};

  return {MvrNodeType::GroupObject, rootUuid};
}

// Writes local grouping metadata after a world transform changes.
void UpdateLocalTransformForTarget(MvrScene &scene,
                                   const SceneTransformTarget &target,
                                   const Matrix &worldTransform) {
  const std::string parentUuid = ParentGroupUuidForTarget(scene, target);
  Matrix localTransform = worldTransform;
  if (!parentUuid.empty()) {
    auto parentIt = scene.groupObjects.find(parentUuid);
    if (parentIt != scene.groupObjects.end())
      localTransform = MatrixUtils::Multiply(
          InverseMatrix(parentIt->second.transform), worldTransform);
  }

  switch (target.type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(target.uuid);
    if (it != scene.fixtures.end()) {
      it->second.transform = worldTransform;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(target.uuid);
    if (it != scene.trusses.end()) {
      it->second.transform = worldTransform;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(target.uuid);
    if (it != scene.supports.end()) {
      it->second.transform = worldTransform;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(target.uuid);
    if (it != scene.sceneObjects.end()) {
      it->second.transform = worldTransform;
      it->second.localTransform = localTransform;
      it->second.hasLocalTransform = true;
    }
    break;
  }
  case MvrNodeType::GroupObject: {
    auto it = scene.groupObjects.find(target.uuid);
    if (it != scene.groupObjects.end()) {
      it->second.transform = worldTransform;
      it->second.localTransform = localTransform;
    }
    break;
  }
  }
}

// Recursively updates child world transforms from a parent group transform.
void SynchronizeGroupChildrenWorldTransforms(MvrScene &scene,
                                             const GroupObject &group) {
  for (const auto &child : group.children) {
    if (child.type == MvrNodeType::GroupObject) {
      auto childIt = scene.groupObjects.find(child.uuid);
      if (childIt == scene.groupObjects.end())
        continue;
      childIt->second.transform = MatrixUtils::Multiply(
          group.transform, childIt->second.localTransform);
      SynchronizeGroupChildrenWorldTransforms(scene, childIt->second);
    } else if (child.type == MvrNodeType::Fixture) {
      auto it = scene.fixtures.find(child.uuid);
      if (it != scene.fixtures.end())
        it->second.transform =
            MatrixUtils::Multiply(group.transform, it->second.localTransform);
    } else if (child.type == MvrNodeType::Truss) {
      auto it = scene.trusses.find(child.uuid);
      if (it != scene.trusses.end())
        it->second.transform =
            MatrixUtils::Multiply(group.transform, it->second.localTransform);
    } else if (child.type == MvrNodeType::Support) {
      auto it = scene.supports.find(child.uuid);
      if (it != scene.supports.end())
        it->second.transform =
            MatrixUtils::Multiply(group.transform, it->second.localTransform);
    } else if (child.type == MvrNodeType::SceneObject) {
      auto it = scene.sceneObjects.find(child.uuid);
      if (it != scene.sceneObjects.end())
        it->second.transform =
            MatrixUtils::Multiply(group.transform, it->second.localTransform);
    }
  }
}

// Rotates a target world transform around a pivot by applying a rotation
// matrix.
Matrix RotateTransformAroundPivot(const Matrix &source, const Matrix &rotation,
                                  const std::array<float, 3> &pivotMm) {
  Matrix rotated = transform_space::ApplyIncrementalRotation(
      source, rotation, transform_space::TransformSpace::World);
  const std::array<float, 3> relative = {source.o[0] - pivotMm[0],
                                         source.o[1] - pivotMm[1],
                                         source.o[2] - pivotMm[2]};
  const std::array<float, 3> rotatedPosition =
      TransformPoint(rotation, relative);
  rotated.o = {rotatedPosition[0] + pivotMm[0], rotatedPosition[1] + pivotMm[1],
               rotatedPosition[2] + pivotMm[2]};
  return rotated;
}

// Adds all descendant leaf UUIDs from a group to an operation result.
void AppendAffectedGroupChildren(OperationResult &result, const MvrScene &scene,
                                 const std::string &groupUuid) {
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::GroupObject)
      AppendAffectedGroupChildren(result, scene, child.uuid);
    else
      AppendAffected(result, child.type, child.uuid);
  }
}

// Adds affected leaf UUIDs from either an object or a whole group.
void AppendAffectedTarget(OperationResult &result, const MvrScene &scene,
                          const MvrNodeType type, const std::string &uuid) {
  if (type == MvrNodeType::GroupObject)
    AppendAffectedGroupChildren(result, scene, uuid);
  else
    AppendAffected(result, type, uuid);
}

// Converts an effective transform target to a grouping child reference.
SelectedObjectRef
BuildSelectedObjectRefForTarget(const MvrScene &scene,
                                const SceneTransformTarget &target) {
  if (target.type == MvrNodeType::GroupObject) {
    auto it = scene.groupObjects.find(target.uuid);
    if (it != scene.groupObjects.end())
      return {target.type, target.uuid, it->second.layer,
              it->second.parentGroupUuid, it->second.transform};
  }
  switch (target.type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(target.uuid);
    if (it != scene.fixtures.end())
      return {target.type, target.uuid, it->second.layer,
              it->second.parentGroupUuid, it->second.transform};
    break;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(target.uuid);
    if (it != scene.trusses.end())
      return {target.type, target.uuid, it->second.layer,
              it->second.parentGroupUuid, it->second.transform};
    break;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(target.uuid);
    if (it != scene.supports.end())
      return {target.type, target.uuid, it->second.layer,
              it->second.parentGroupUuid, it->second.transform};
    break;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(target.uuid);
    if (it != scene.sceneObjects.end())
      return {target.type, target.uuid, it->second.layer,
              it->second.parentGroupUuid, it->second.transform};
    break;
  }
  case MvrNodeType::GroupObject:
    break;
  }
  return {target.type, target.uuid, {}, {}, MatrixUtils::Identity()};
}

// Collects direct children from a group into a highlight UUID sequence.
void AppendGroupChildrenForHighlights(const MvrScene &scene,
                                      const std::string &groupUuid,
                                      std::vector<std::string> &expanded,
                                      std::unordered_set<std::string> &seen) {
  const auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::GroupObject)
      AppendGroupChildrenForHighlights(scene, child.uuid, expanded, seen);
    else
      AppendUnique(expanded, seen, child.uuid);
  }
}

} // namespace

// Reparents one child reference to a new parent group while preserving world
// placement.
void ReparentChildPreservingWorld(MvrScene &scene,
                                  const GroupObjectChildRef &child,
                                  const std::string &newParentUuid) {
  const SceneTransformTarget target{child.type, child.uuid};
  const Matrix worldTransform = GetTargetWorldTransform(scene, target);
  if (child.type == MvrNodeType::GroupObject) {
    auto childGroupIt = scene.groupObjects.find(child.uuid);
    if (childGroupIt != scene.groupObjects.end()) {
      childGroupIt->second.parentGroupUuid = newParentUuid;
      childGroupIt->second.localTransform =
          newParentUuid.empty()
              ? worldTransform
              : MatrixUtils::Multiply(
                    InverseMatrix(scene.groupObjects[newParentUuid].transform),
                    worldTransform);
    }
    return;
  }

  SelectedObjectRef ref = BuildSelectedObjectRefForTarget(scene, target);
  ApplyParentAndLocalTransform(
      scene, ref, newParentUuid,
      newParentUuid.empty()
          ? worldTransform
          : MatrixUtils::Multiply(
                InverseMatrix(scene.groupObjects[newParentUuid].transform),
                worldTransform));
}

// Removes a group while promoting its direct children to the group's parent.
void UngroupRootTarget(MvrScene &scene, const std::string &groupUuid,
                       OperationResult &result) {
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;

  const std::string parentUuid = groupIt->second.parentGroupUuid;
  const std::vector<GroupObjectChildRef> children = groupIt->second.children;
  AppendAffectedGroupChildren(result, scene, groupUuid);
  RemoveChildFromGroups(scene, MvrNodeType::GroupObject, groupUuid);

  for (const auto &child : children) {
    ReparentChildPreservingWorld(scene, child, parentUuid);
    if (!parentUuid.empty())
      scene.groupObjects[parentUuid].children.push_back(child);
  }

  scene.groupObjects.erase(groupUuid);
  result.changed = true;
}

// Repairs GroupObject child layers so parent group ownership stays authoritative.
std::size_t SynchronizeGroupObjectLayerOwnership(MvrScene &scene) {
  std::size_t repairedCount = 0;
  for (auto &[groupUuid, group] : scene.groupObjects) {
    if (group.parentGroupUuid.empty()) {
      repairedCount +=
          SynchronizeGroupObjectLayerOwnershipFrom(scene, groupUuid);
    }
  }
  return repairedCount;
}

// Creates a new GroupObject and reparents the selected scene entities.
OperationResult GroupSelection(MvrScene &scene,
                               const ObjectSelection &selection) {
  OperationResult result;
  std::vector<SelectedObjectRef> objects =
      CollectSelectedObjects(scene, selection);
  const bool hasCommonDirectParent =
      objects.size() >= 2 && !CommonParentGroupUuid(objects).empty();
  if (!hasCommonDirectParent) {
    const auto targets = BuildTransformTargets(scene, selection);
    objects.clear();
    objects.reserve(targets.size());
    for (const auto &target : targets)
      objects.push_back(BuildSelectedObjectRefForTarget(scene, target));
  }
  if (objects.size() < 2)
    return result;

  const Matrix groupWorldTransform = BuildGroupWorldTransform(objects);
  const Matrix inverseGroupWorldTransform = InverseMatrix(groupWorldTransform);
  std::string parentGroupUuid = CommonParentGroupUuid(objects);
  if (!parentGroupUuid.empty() && !scene.groupObjects.contains(parentGroupUuid))
    parentGroupUuid.clear();

  GroupObject group;
  group.uuid = GenerateUuid();
  group.name = "Group " + std::to_string(scene.groupObjects.size() + 1);
  group.layer = AuthoritativeGroupLayer(scene, objects, parentGroupUuid);
  group.parentGroupUuid = parentGroupUuid;
  group.transform = groupWorldTransform;
  group.localTransform =
      parentGroupUuid.empty()
          ? groupWorldTransform
          : MatrixUtils::Multiply(
                InverseMatrix(scene.groupObjects[parentGroupUuid].transform),
                groupWorldTransform);

  std::unordered_set<std::string> previousGroupsToCheck;
  for (const auto &object : objects) {
    if (!object.parentGroupUuid.empty())
      previousGroupsToCheck.insert(object.parentGroupUuid);
    RemoveChildFromGroups(scene, object.type, object.uuid);
    const Matrix localTransform = MatrixUtils::Multiply(
        inverseGroupWorldTransform, object.worldTransform);
    ApplyParentAndLayer(scene, object, group.uuid, localTransform, group.layer);
    group.children.push_back({object.type, object.uuid});
    AppendAffectedTarget(result, scene, object.type, object.uuid);
  }

  for (const auto &groupUuid : previousGroupsToCheck) {
    if (groupUuid != parentGroupUuid && IsEmptyGroup(scene, groupUuid))
      RemoveEmptyGroup(scene, groupUuid);
  }

  scene.groupObjects[group.uuid] = group;
  if (!parentGroupUuid.empty())
    scene.groupObjects[parentGroupUuid].children.push_back(
        {MvrNodeType::GroupObject, group.uuid});

  result.changed = true;
  result.groupUuid = group.uuid;
  return result;
}


// Adds selected scene entities to an existing GroupObject while preserving world placement.
OperationResult AddSelectionToGroup(MvrScene &scene,
                                    const ObjectSelection &selection,
                                    const std::string &groupUuid) {
  OperationResult result;
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return result;

  std::vector<SelectedObjectRef> objects = CollectSelectedObjects(scene, selection);
  for (const auto &object : objects) {
    if (object.parentGroupUuid == groupUuid)
      continue;
    RemoveChildFromGroups(scene, object.type, object.uuid);
    const Matrix localTransform = MatrixUtils::Multiply(
        InverseMatrix(groupIt->second.transform), object.worldTransform);
    ApplyParentAndLayer(scene, object, groupUuid, localTransform,
                        groupIt->second.layer);
    groupIt->second.children.push_back({object.type, object.uuid});
    AppendAffectedTarget(result, scene, object.type, object.uuid);
    result.changed = true;
  }
  result.groupUuid = groupUuid;
  return result;
}


// Removes selected scene entities from their direct GroupObject parents while preserving world placement.
OperationResult RemoveSelectionFromGroup(MvrScene &scene,
                                         const ObjectSelection &selection) {
  OperationResult result;
  std::vector<SelectedObjectRef> objects = CollectSelectedObjects(scene, selection);
  for (const auto &object : objects) {
    if (object.parentGroupUuid.empty())
      continue;
    RemoveChildFromGroups(scene, object.type, object.uuid);
    ReparentChildPreservingWorld(scene, {object.type, object.uuid}, {});
    AppendAffectedTarget(result, scene, object.type, object.uuid);
    result.changed = true;
  }
  return result;
}

// Removes selected effective groups and promotes their direct children.
OperationResult UngroupSelection(MvrScene &scene,
                                 const ObjectSelection &selection) {
  OperationResult result;
  const auto targets = BuildTransformTargets(scene, selection);
  for (const auto &target : targets) {
    if (target.type == MvrNodeType::GroupObject)
      UngroupRootTarget(scene, target.uuid, result);
  }
  return result;
}

// Builds effective transform roots so group members move as one item.
std::vector<SceneTransformTarget>
BuildTransformTargets(const MvrScene &scene, const ObjectSelection &selection) {
  struct Candidate {
    SceneTransformTarget target;
    std::string rootGroupUuid;
  };

  std::vector<Candidate> candidates;
  std::set<std::string> selectedGroupRoots;
  const std::vector<SelectedObjectRef> objects =
      CollectSelectedObjects(scene, selection);
  for (const auto &object : objects) {
    Candidate candidate{ResolveTransformRoot(scene, object),
                        ResolveRootGroupUuid(scene, object)};
    if (candidate.target.type == MvrNodeType::GroupObject)
      selectedGroupRoots.insert(candidate.target.uuid);
    candidates.push_back(std::move(candidate));
  }

  std::vector<SceneTransformTarget> targets;
  std::set<std::pair<MvrNodeType, std::string>> seen;
  for (const auto &candidate : candidates) {
    if (candidate.target.type != MvrNodeType::GroupObject &&
        !candidate.rootGroupUuid.empty() &&
        selectedGroupRoots.find(candidate.rootGroupUuid) !=
            selectedGroupRoots.end())
      continue;
    if (seen.insert({candidate.target.type, candidate.target.uuid}).second)
      targets.push_back(candidate.target);
  }
  return targets;
}

// Builds exact transform targets in deterministic selection order.
std::vector<SceneTransformTarget>
BuildExactTransformTargets(const MvrScene &scene,
                           const ObjectSelection &selection) {
  std::vector<SceneTransformTarget> targets;
  std::set<std::pair<MvrNodeType, std::string>> seen;
  for (const auto &object : CollectSelectedObjects(scene, selection)) {
    if (seen.insert({object.type, object.uuid}).second)
      targets.push_back({object.type, object.uuid});
  }
  return targets;
}

// Builds interactive targets while promoting grouped trusses only.
std::vector<SceneTransformTarget>
BuildInteractiveTransformTargets(const MvrScene &scene,
                                 const ObjectSelection &selection) {
  struct Candidate {
    SceneTransformTarget target;
    std::string rootGroupUuid;
  };

  std::vector<Candidate> candidates;
  std::set<std::string> promotedGroups;
  for (const auto &object : CollectSelectedObjects(scene, selection)) {
    const std::string rootUuid = ResolveRootGroupUuid(scene, object);
    SceneTransformTarget target{object.type, object.uuid};
    if (object.type == MvrNodeType::Truss && !rootUuid.empty()) {
      target = {MvrNodeType::GroupObject, rootUuid};
      promotedGroups.insert(rootUuid);
    }
    candidates.push_back({std::move(target), rootUuid});
  }

  std::vector<SceneTransformTarget> targets;
  std::set<std::pair<MvrNodeType, std::string>> seen;
  for (const auto &candidate : candidates) {
    if (candidate.target.type != MvrNodeType::GroupObject &&
        !candidate.rootGroupUuid.empty() &&
        promotedGroups.find(candidate.rootGroupUuid) != promotedGroups.end())
      continue;
    if (seen.insert({candidate.target.type, candidate.target.uuid}).second)
      targets.push_back(candidate.target);
  }
  return targets;
}

// Returns the current world transform for one effective transform target.
Matrix GetTargetWorldTransform(const MvrScene &scene,
                               const SceneTransformTarget &target) {
  switch (target.type) {
  case MvrNodeType::Fixture: {
    auto it = scene.fixtures.find(target.uuid);
    return it == scene.fixtures.end() ? MatrixUtils::Identity()
                                      : it->second.transform;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(target.uuid);
    return it == scene.trusses.end() ? MatrixUtils::Identity()
                                     : it->second.transform;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(target.uuid);
    return it == scene.supports.end() ? MatrixUtils::Identity()
                                      : it->second.transform;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(target.uuid);
    return it == scene.sceneObjects.end() ? MatrixUtils::Identity()
                                          : it->second.transform;
  }
  case MvrNodeType::GroupObject: {
    auto it = scene.groupObjects.find(target.uuid);
    return it == scene.groupObjects.end() ? MatrixUtils::Identity()
                                          : it->second.transform;
  }
  }
  return MatrixUtils::Identity();
}

// Applies a world transform to a target and recursively synchronizes group
// descendants.
void SetTargetWorldTransform(MvrScene &scene,
                             const SceneTransformTarget &target,
                             const Matrix &worldTransform) {
  UpdateLocalTransformForTarget(scene, target, worldTransform);
  if (target.type != MvrNodeType::GroupObject)
    return;
  auto groupIt = scene.groupObjects.find(target.uuid);
  if (groupIt != scene.groupObjects.end())
    SynchronizeGroupChildrenWorldTransforms(scene, groupIt->second);
}

// Translates selected effective targets by the supplied millimeter delta.
void TranslateSelection(MvrScene &scene, const ObjectSelection &selection,
                        const std::array<float, 3> &deltaMm,
                        transform_space::TransformSpace space) {
  const auto targets = BuildInteractiveTransformTargets(scene, selection);
  for (const auto &target : targets) {
    const Matrix transform = GetTargetWorldTransform(scene, target);
    SetTargetWorldTransform(
        scene, target,
        transform_space::ApplyIncrementalTranslation(transform, deltaMm, space));
  }
}

// Rotates selected effective targets around a shared millimeter pivot.
void RotateSelectionAroundPivot(MvrScene &scene,
                                const ObjectSelection &selection, int axis,
                                float angleDeg,
                                const std::array<float, 3> &pivotMm,
                                transform_space::TransformSpace space) {
  Matrix rotation = MatrixUtils::Identity();
  if (axis == 0)
    rotation = MatrixUtils::EulerToMatrix(0.0f, 0.0f, angleDeg);
  else if (axis == 1)
    rotation = MatrixUtils::EulerToMatrix(0.0f, angleDeg, 0.0f);
  else
    rotation = MatrixUtils::EulerToMatrix(angleDeg, 0.0f, 0.0f);

  const auto targets = BuildInteractiveTransformTargets(scene, selection);
  Matrix effectiveRotation = rotation;
  if (space == transform_space::TransformSpace::Local && !targets.empty()) {
    const Matrix reference = transform_space::ExtractOrientation(
        GetTargetWorldTransform(scene, targets.front()));
    effectiveRotation = MatrixUtils::Multiply(
        MatrixUtils::Multiply(reference, rotation), InverseMatrix(reference));
  }
  for (const auto &target : targets) {
    const Matrix transform = GetTargetWorldTransform(scene, target);
    SetTargetWorldTransform(
        scene, target,
        RotateTransformAroundPivot(transform, effectiveRotation, pivotMm));
  }
}

// Returns selected UUIDs expanded with direct group siblings for viewport
// highlights.
std::vector<std::string>
ExpandSelectionForGroupHighlights(const MvrScene &scene,
                                  const ObjectSelection &selection) {
  std::vector<std::string> expanded;
  std::unordered_set<std::string> seen;
  for (const auto &target : BuildTransformTargets(scene, selection)) {
    if (target.type == MvrNodeType::GroupObject) {
      AppendGroupChildrenForHighlights(scene, target.uuid, expanded, seen);
    } else {
      AppendUnique(expanded, seen, target.uuid);
    }
  }
  return expanded;
}

// Returns sibling UUIDs that share the hovered object's effective root group.
std::vector<std::string>
ExpandHoverForGroupHighlights(const MvrScene &scene, const std::string &uuid) {
  std::vector<std::string> expanded;
  if (uuid.empty())
    return expanded;

  ObjectSelection selection;
  if (scene.fixtures.find(uuid) != scene.fixtures.end())
    selection.fixtures.push_back(uuid);
  else if (scene.trusses.find(uuid) != scene.trusses.end())
    selection.trusses.push_back(uuid);
  else if (scene.supports.find(uuid) != scene.supports.end())
    selection.supports.push_back(uuid);
  else if (scene.sceneObjects.find(uuid) != scene.sceneObjects.end())
    selection.sceneObjects.push_back(uuid);
  else
    return expanded;

  std::unordered_set<std::string> seen;
  for (const auto &target : BuildTransformTargets(scene, selection)) {
    if (target.type != MvrNodeType::GroupObject)
      continue;
    AppendGroupChildrenForHighlights(scene, target.uuid, expanded, seen);
  }
  expanded.erase(std::remove(expanded.begin(), expanded.end(), uuid),
                 expanded.end());
  return expanded;
}

} // namespace scene_grouping
