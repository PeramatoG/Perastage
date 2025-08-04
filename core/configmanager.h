#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
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
    // Default user configuration path helpers
    static std::string GetUserConfigFile();
    bool LoadUserConfig();
    bool SaveUserConfig() const;

    // Access to current MVR scene (modifiable)
    MvrScene& GetScene();
    const MvrScene& GetScene() const;

    // Current selections for different object types
    const std::vector<std::string>& GetSelectedFixtures() const;
    void SetSelectedFixtures(const std::vector<std::string>& uuids);
    const std::vector<std::string>& GetSelectedTrusses() const;
    void SetSelectedTrusses(const std::vector<std::string>& uuids);
    const std::vector<std::string>& GetSelectedSceneObjects() const;
    void SetSelectedSceneObjects(const std::vector<std::string>& uuids);

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

    // Column printing preferences
    std::vector<std::string> GetFixturePrintColumns() const;
    void SetFixturePrintColumns(const std::vector<std::string>& cols);
    std::vector<std::string> GetTrussPrintColumns() const;
    void SetTrussPrintColumns(const std::vector<std::string>& cols);
    std::vector<std::string> GetSceneObjectPrintColumns() const;
    void SetSceneObjectPrintColumns(const std::vector<std::string>& cols);

    // Clear everything (scene + config)
    void Reset();

    // --- Undo/Redo support ---
    void PushUndoState();
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();
    void ClearHistory();

private:
    ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void ApplyColumnDefaults();

    std::unordered_map<std::string, std::string> configData;
    std::unordered_map<std::string, VariableInfo> variables;
    MvrScene scene;

    // Selection state
    std::vector<std::string> selectedFixtures;
    std::vector<std::string> selectedTrusses;
    std::vector<std::string> selectedSceneObjects;

    struct Snapshot {
        MvrScene scene;
        std::vector<std::string> selFixtures;
        std::vector<std::string> selTrusses;
        std::vector<std::string> selSceneObjects;
    };

    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    size_t maxHistory = 20;
};
