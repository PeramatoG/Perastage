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

// Builds the inverse transform used to preserve world placement during reparenting.
Matrix InverseMatrix(const Matrix &m) {
  const float det = Determinant3x3(m);
  if (std::fabs(det) < 1e-8f)
    return MatrixUtils::Identity();

  const float invDet = 1.0f / det;
  const float a00 = m.u[0], a01 = m.v[0], a02 = m.w[0];
  const float a10 = m.u[1], a11 = m.v[1], a12 = m.w[1];
  const float a20 = m.u[2], a21 = m.v[2], a22 = m.w[2];

  Matrix inv;
  inv.u = { (a11 * a22 - a12 * a21) * invDet,
            (a12 * a20 - a10 * a22) * invDet,
            (a10 * a21 - a11 * a20) * invDet };
  inv.v = { (a02 * a21 - a01 * a22) * invDet,
            (a00 * a22 - a02 * a20) * invDet,
            (a01 * a20 - a00 * a21) * invDet };
  inv.w = { (a01 * a12 - a02 * a11) * invDet,
            (a02 * a10 - a00 * a12) * invDet,
            (a00 * a11 - a01 * a10) * invDet };

  for (int i = 0; i < 3; ++i) {
    inv.o[i] = -(inv.u[i] * m.o[0] + inv.v[i] * m.o[1] +
                 inv.w[i] * m.o[2]);
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
    return it == scene.fixtures.end() ? std::string{} : it->second.parentGroupUuid;
  }
  case MvrNodeType::Truss: {
    auto it = scene.trusses.find(uuid);
    return it == scene.trusses.end() ? std::string{} : it->second.parentGroupUuid;
  }
  case MvrNodeType::Support: {
    auto it = scene.supports.find(uuid);
    return it == scene.supports.end() ? std::string{} : it->second.parentGroupUuid;
  }
  case MvrNodeType::SceneObject: {
    auto it = scene.sceneObjects.find(uuid);
    return it == scene.sceneObjects.end() ? std::string{} : it->second.parentGroupUuid;
  }
  case MvrNodeType::GroupObject:
    break;
  }
  return {};
}

// Adds existing selected objects to a normalized reference list.
std::vector<SelectedObjectRef> CollectSelectedObjects(
    const MvrScene &scene, const ObjectSelection &selection) {
  std::vector<SelectedObjectRef> refs;
  std::set<std::pair<MvrNodeType, std::string>> seen;

  auto append = [&](const auto &table, const std::vector<std::string> &uuids,
                    MvrNodeType type) {
    for (const auto &uuid : uuids) {
      auto it = table.find(uuid);
      if (it == table.end() || !seen.insert({type, uuid}).second)
        continue;
      refs.push_back({type, uuid, it->second.layer,
                      it->second.parentGroupUuid, it->second.transform});
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
std::string CommonParentGroupUuid(const std::vector<SelectedObjectRef> &objects) {
  if (objects.empty())
    return {};
  const std::string parent = objects.front().parentGroupUuid;
  for (const auto &object : objects) {
    if (object.parentGroupUuid != parent)
      return {};
  }
  return parent;
}

// Updates an object's parent group and local transform fields.
void ApplyParentAndLocalTransform(MvrScene &scene, const SelectedObjectRef &object,
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
  case MvrNodeType::GroupObject:
    break;
  }
}

// Adds an affected child UUID to the operation result for selection restoration.
void AppendAffected(OperationResult &result, const MvrNodeType type,
                    const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture:
    if (std::find(result.affectedFixtures.begin(), result.affectedFixtures.end(),
                  uuid) == result.affectedFixtures.end())
      result.affectedFixtures.push_back(uuid);
    break;
  case MvrNodeType::Truss:
    if (std::find(result.affectedTrusses.begin(), result.affectedTrusses.end(),
                  uuid) == result.affectedTrusses.end())
      result.affectedTrusses.push_back(uuid);
    break;
  case MvrNodeType::Support:
    if (std::find(result.affectedSupports.begin(), result.affectedSupports.end(),
                  uuid) == result.affectedSupports.end())
      result.affectedSupports.push_back(uuid);
    break;
  case MvrNodeType::SceneObject:
    if (std::find(result.affectedSceneObjects.begin(),
                  result.affectedSceneObjects.end(), uuid) ==
        result.affectedSceneObjects.end())
      result.affectedSceneObjects.push_back(uuid);
    break;
  case MvrNodeType::GroupObject:
    break;
  }
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

// Creates a new GroupObject and reparents the selected scene entities.
OperationResult GroupSelection(MvrScene &scene, const ObjectSelection &selection) {
  OperationResult result;
  const std::vector<SelectedObjectRef> objects = CollectSelectedObjects(scene, selection);
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
  group.layer = objects.front().layer;
  group.parentGroupUuid = parentGroupUuid;
  group.transform = groupWorldTransform;
  group.localTransform = parentGroupUuid.empty()
                             ? groupWorldTransform
                             : MatrixUtils::Multiply(
                                   InverseMatrix(scene.groupObjects[parentGroupUuid].transform),
                                   groupWorldTransform);

  std::unordered_set<std::string> previousGroupsToCheck;
  for (const auto &object : objects) {
    if (!object.parentGroupUuid.empty())
      previousGroupsToCheck.insert(object.parentGroupUuid);
    RemoveChildFromGroups(scene, object.type, object.uuid);
    const Matrix localTransform = MatrixUtils::Multiply(inverseGroupWorldTransform,
                                                        object.worldTransform);
    ApplyParentAndLocalTransform(scene, object, group.uuid, localTransform);
    group.children.push_back({object.type, object.uuid});
    AppendAffected(result, object.type, object.uuid);
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

// Removes selected scene entities from their direct parent groups.
OperationResult UngroupSelection(MvrScene &scene, const ObjectSelection &selection) {
  OperationResult result;
  const std::vector<SelectedObjectRef> objects = CollectSelectedObjects(scene, selection);
  std::unordered_set<std::string> groupsToCheck;

  for (const auto &object : objects) {
    const std::string parentUuid = ParentGroupUuidFor(scene, object.type, object.uuid);
    if (parentUuid.empty())
      continue;

    RemoveChildFromGroups(scene, object.type, object.uuid);
    ApplyParentAndLocalTransform(scene, object, {}, object.worldTransform);
    groupsToCheck.insert(parentUuid);
    AppendAffected(result, object.type, object.uuid);
    result.changed = true;
  }

  for (const auto &groupUuid : groupsToCheck) {
    if (IsEmptyGroup(scene, groupUuid))
      RemoveEmptyGroup(scene, groupUuid);
  }

  return result;
}

// Returns selected UUIDs expanded with direct group siblings for viewport highlights.
std::vector<std::string> ExpandSelectionForGroupHighlights(
    const MvrScene &scene, const ObjectSelection &selection) {
  std::vector<std::string> expanded;
  std::unordered_set<std::string> seen;
  const std::vector<SelectedObjectRef> objects = CollectSelectedObjects(scene, selection);

  for (const auto &object : objects) {
    AppendUnique(expanded, seen, object.uuid);
    const std::string parentUuid = ParentGroupUuidFor(scene, object.type, object.uuid);
    if (!parentUuid.empty())
      AppendGroupChildrenForHighlights(scene, parentUuid, expanded, seen);
  }
  return expanded;
}

} // namespace scene_grouping
