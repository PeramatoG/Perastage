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
