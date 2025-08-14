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

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>
#include "mvrscene.h"

inline constexpr const char* DEFAULT_LAYER_NAME = "No Layer";

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

    // Layer visibility management
    std::unordered_set<std::string> GetHiddenLayers() const;
    void SetHiddenLayers(const std::unordered_set<std::string>& layers);
    bool IsLayerVisible(const std::string& layer) const;

    // Retrieve all existing layer names
    std::vector<std::string> GetLayerNames() const;

    // Currently selected layer name
    const std::string& GetCurrentLayer() const;
    void SetCurrentLayer(const std::string& name);

    // Clear everything (scene + config)
    void Reset();

    // --- Undo/Redo support ---
    void PushUndoState(const std::string& description = "");
    bool CanUndo() const;
    bool CanRedo() const;
    std::string Undo();
    std::string Redo();
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
        std::string description;
    };

    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    size_t maxHistory = 20;

    std::string currentLayer = DEFAULT_LAYER_NAME;
};
