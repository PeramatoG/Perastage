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
#include "configservices.h"
#include <algorithm>

// Deletion/project replacement never leaves stale alignment reference identities.
void SelectionState::Prune(const MvrScene &scene) {
  auto retain = [](auto values, const auto &table) {
    std::erase_if(values, [&](const std::string &uuid) { return !table.contains(uuid); });
    return values;
  };
  SetSelectedFixtures(retain(selectedFixtures,scene.fixtures));
  SetSelectedTrusses(retain(selectedTrusses,scene.trusses));
  SetSelectedSupports(retain(selectedSupports,scene.supports));
  SetSelectedSceneObjects(retain(selectedSceneObjects,scene.sceneObjects));
  SetSelectedGroupObjects(retain(selectedGroupObjects,scene.groupObjects));
}
