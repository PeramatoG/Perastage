#include "configmanager.h"
#include <fstream>
#include <sstream>

ConfigManager& ConfigManager::Get()
{
    static ConfigManager instance;
    return instance;
}

// -- Config key-value access --

void ConfigManager::SetValue(const std::string& key, const std::string& value)
{
    configData[key] = value;
}

std::optional<std::string> ConfigManager::GetValue(const std::string& key) const
{
    auto it = configData.find(key);
    if (it != configData.end())
        return it->second;
    return std::nullopt;
}

bool ConfigManager::HasKey(const std::string& key) const
{
    return configData.find(key) != configData.end();
}

void ConfigManager::RemoveKey(const std::string& key)
{
    configData.erase(key);
}

void ConfigManager::ClearValues()
{
    configData.clear();
}

// -- Scene access --

MvrScene& ConfigManager::GetScene()
{
    return scene;
}

const MvrScene& ConfigManager::GetScene() const
{
    return scene;
}

// -- Persistence --

bool ConfigManager::LoadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    configData.clear();
    std::string line;
    while (std::getline(file, line))
    {
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        configData[key] = value;
    }

    return true;
}

bool ConfigManager::SaveToFile(const std::string& path) const
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;

    for (const auto& [key, value] : configData)
    {
        file << key << "=" << value << "\n";
    }

    return true;
}

void ConfigManager::Reset()
{
    configData.clear();
    scene.Clear();
}
