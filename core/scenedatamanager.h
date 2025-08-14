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

#include <unordered_map>
#include <string>
#include "fixture.h"
#include "truss.h"
#include "sceneobject.h"

// Simple singleton wrapper providing access to the scene data
class SceneDataManager {
public:
    static SceneDataManager& Instance();

    const std::unordered_map<std::string, Fixture>& GetFixtures() const;
    const std::unordered_map<std::string, Truss>& GetTrusses() const;
    const std::unordered_map<std::string, SceneObject>& GetSceneObjects() const;
    // Currently no group objects are stored, return empty map
    const std::unordered_map<std::string, SceneObject>& GetGroupObjects() const;

private:
    SceneDataManager() = default;
    SceneDataManager(const SceneDataManager&) = delete;
    SceneDataManager& operator=(const SceneDataManager&) = delete;
};
