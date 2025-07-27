#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include "mvrscene.h"

// Singleton to manage configuration and MVR scene data globally
class ConfigManager
{
public:
    // Access singleton instance
    static ConfigManager& Get();

    // Generic key-value storage for config (persistent or session)
    void SetValue(const std::string& key, const std::string& value);
    std::optional<std::string> GetValue(const std::string& key) const;
    bool HasKey(const std::string& key) const;
    void RemoveKey(const std::string& key);
    void ClearValues();

    bool SaveProject(const std::string& path);
    bool LoadProject(const std::string& path);
    // Save/load configuration file (e.g., JSON, INI, TXT…)
    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

    // Access to current MVR scene (modifiable)
    MvrScene& GetScene();
    const MvrScene& GetScene() const;

    struct VariableInfo {
        std::string type;
        float defaultValue = 0.0f;
        float value = 0.0f;
        float minValue = 0.0f;
        float maxValue = 0.0f;
    };

    void RegisterVariable(const std::string& name, const std::string& type,
                          float defVal, float minVal, float maxVal);
    float GetFloat(const std::string& name) const;
    void SetFloat(const std::string& name, float v);
    void ApplyDefaults();

    // Clear everything (scene + config)
    void Reset();

private:
    ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::unordered_map<std::string, std::string> configData;
    std::unordered_map<std::string, VariableInfo> variables;
    MvrScene scene;
};
