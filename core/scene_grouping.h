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
#pragma once

#include "mvrscene.h"

#include <array>
#include <string>
#include <vector>

namespace scene_grouping {

struct ObjectSelection {
  std::vector<std::string> fixtures;
  std::vector<std::string> trusses;
  std::vector<std::string> supports;
  std::vector<std::string> sceneObjects;
};

struct SceneTransformTarget {
  MvrNodeType type = MvrNodeType::Fixture;
  std::string uuid;
};

struct OperationResult {
  bool changed = false;
  std::string groupUuid;
  std::vector<std::string> affectedFixtures;
  std::vector<std::string> affectedTrusses;
  std::vector<std::string> affectedSupports;
  std::vector<std::string> affectedSceneObjects;
};

// Creates one MVR-compatible GroupObject from the selected scene entities.
OperationResult GroupSelection(MvrScene &scene,
                               const ObjectSelection &selection);

// Adds selected scene entities to an existing MVR-compatible GroupObject.
OperationResult AddSelectionToGroup(MvrScene &scene,
                                    const ObjectSelection &selection,
                                    const std::string &groupUuid);

// Removes selected scene entities from their direct parent groups without
// deleting or flattening those groups.
OperationResult RemoveSelectionFromGroup(MvrScene &scene,
                                         const ObjectSelection &selection);

// Removes selected scene entities from their direct parent groups.
OperationResult UngroupSelection(MvrScene &scene,
                                 const ObjectSelection &selection);

// Builds effective transform roots so grouped objects behave as single units.
std::vector<SceneTransformTarget>
BuildTransformTargets(const MvrScene &scene, const ObjectSelection &selection);

// Returns the current world transform for one transform target.
Matrix GetTargetWorldTransform(const MvrScene &scene,
                               const SceneTransformTarget &target);

// Applies a world transform to a target and recursively synchronizes group
// children.
void SetTargetWorldTransform(MvrScene &scene,
                             const SceneTransformTarget &target,
                             const Matrix &worldTransform);

// Translates effective selection targets by a millimeter delta.
void TranslateSelection(MvrScene &scene, const ObjectSelection &selection,
                        const std::array<float, 3> &deltaMm);

// Rotates effective selection targets around a millimeter pivot.
void RotateSelectionAroundPivot(MvrScene &scene,
                                const ObjectSelection &selection, int axis,
                                float angleDeg,
                                const std::array<float, 3> &pivotMm);

// Expands UUID highlights so selecting one group member highlights its full
// root group.
std::vector<std::string>
ExpandSelectionForGroupHighlights(const MvrScene &scene,
                                  const ObjectSelection &selection);

// Returns sibling UUIDs that share the hovered object's effective root group.
std::vector<std::string> ExpandHoverForGroupHighlights(const MvrScene &scene,
                                                       const std::string &uuid);

} // namespace scene_grouping
