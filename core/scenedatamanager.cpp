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
#include "scenedatamanager.h"
#include "configmanager.h"

SceneDataManager& SceneDataManager::Instance()
{
    static SceneDataManager instance;
    return instance;
}

const std::unordered_map<std::string, Fixture>& SceneDataManager::GetFixtures() const
{
    return ConfigManager::Get().GetScene().fixtures;
}

const std::unordered_map<std::string, Truss>& SceneDataManager::GetTrusses() const
{
    return ConfigManager::Get().GetScene().trusses;
}

const std::unordered_map<std::string, SceneObject>& SceneDataManager::GetSceneObjects() const
{
    return ConfigManager::Get().GetScene().sceneObjects;
}

const std::unordered_map<std::string, SceneObject>& SceneDataManager::GetGroupObjects() const
{
    static std::unordered_map<std::string, SceneObject> empty;
    return empty;
}
