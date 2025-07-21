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
