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

// Installs immutable scene data for the lifetime of an isolated render operation.
SceneDataManager::ScopedSnapshot::ScopedSnapshot(const SceneSnapshot& snapshot)
    : previous_(SceneDataManager::Instance().activeSnapshot_)
{
    SceneDataManager::Instance().activeSnapshot_ = &snapshot;
}

// Restores the scene-data source used before the isolated render operation.
SceneDataManager::ScopedSnapshot::~ScopedSnapshot()
{
    SceneDataManager::Instance().activeSnapshot_ = previous_;
}

SceneDataManager& SceneDataManager::Instance()
{
    static SceneDataManager instance;
    return instance;
}

const std::unordered_map<std::string, Fixture>& SceneDataManager::GetFixtures() const
{
    if (activeSnapshot_)
        return activeSnapshot_->fixtures;
    return ConfigManager::Get().GetScene().fixtures;
}

const std::unordered_map<std::string, Truss>& SceneDataManager::GetTrusses() const
{
    if (activeSnapshot_)
        return activeSnapshot_->trusses;
    return ConfigManager::Get().GetScene().trusses;
}

const std::unordered_map<std::string, SceneObject>& SceneDataManager::GetSceneObjects() const
{
    if (activeSnapshot_)
        return activeSnapshot_->sceneObjects;
    return ConfigManager::Get().GetScene().sceneObjects;
}

const std::unordered_map<std::string, GroupObject>& SceneDataManager::GetGroupObjects() const
{
    if (activeSnapshot_)
        return activeSnapshot_->groupObjects;
    return ConfigManager::Get().GetScene().groupObjects;
}
