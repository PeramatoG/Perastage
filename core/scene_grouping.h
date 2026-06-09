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

#include <string>
#include <vector>

namespace scene_grouping {

struct ObjectSelection {
  std::vector<std::string> fixtures;
  std::vector<std::string> trusses;
  std::vector<std::string> supports;
  std::vector<std::string> sceneObjects;
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
OperationResult GroupSelection(MvrScene &scene, const ObjectSelection &selection);

// Removes selected scene entities from their direct parent groups.
OperationResult UngroupSelection(MvrScene &scene, const ObjectSelection &selection);

// Expands UUID highlights so selecting one group member highlights its direct siblings.
std::vector<std::string> ExpandSelectionForGroupHighlights(
    const MvrScene &scene, const ObjectSelection &selection);

} // namespace scene_grouping
