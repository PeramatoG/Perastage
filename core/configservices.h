#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mvrscene.h"

inline constexpr const char *DEFAULT_LAYER_NAME = "No Layer";

class UserPreferencesStore {
public:
  struct VariableInfo {
    std::string type;
    float defaultValue = 0.0f;
    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    std::vector<std::string> legacyNames;
  };

  void SetValue(const std::string &key, const std::string &value);
  std::optional<std::string> GetValue(const std::string &key) const;
  bool HasKey(const std::string &key) const;
  void RemoveKey(const std::string &key);
  void ClearValues();

  bool LoadFromFile(const std::string &path);
  bool SaveToFile(const std::string &path) const;
  static std::string GetUserConfigFile();
  bool LoadUserConfig();
  bool SaveUserConfig() const;

  void RegisterVariable(const std::string &name, const std::string &type,
                        float defVal, float minVal, float maxVal,
                        std::vector<std::string> legacyNames = {});
  float GetFloat(const std::string &name) const;
  void SetFloat(const std::string &name, float v);
  void ApplyDefaults();

  std::vector<std::string> GetFixturePrintColumns() const;
  void SetFixturePrintColumns(const std::vector<std::string> &cols);
  std::vector<std::string> GetTrussPrintColumns() const;
  void SetTrussPrintColumns(const std::vector<std::string> &cols);
  std::vector<std::string> GetSupportPrintColumns() const;
  void SetSupportPrintColumns(const std::vector<std::string> &cols);
  std::vector<std::string> GetSceneObjectPrintColumns() const;
  void SetSceneObjectPrintColumns(const std::vector<std::string> &cols);

private:
  void ApplyColumnDefaults();

  std::unordered_map<std::string, std::string> configData;
  std::unordered_map<std::string, VariableInfo> variables;
};

class SelectionState {
public:
  const std::vector<std::string> &GetSelectedFixtures() const;
  void SetSelectedFixtures(const std::vector<std::string> &uuids);
  const std::vector<std::string> &GetSelectedTrusses() const;
  void SetSelectedTrusses(const std::vector<std::string> &uuids);
  const std::vector<std::string> &GetSelectedSupports() const;
  void SetSelectedSupports(const std::vector<std::string> &uuids);
  const std::vector<std::string> &GetSelectedSceneObjects() const;
  void SetSelectedSceneObjects(const std::vector<std::string> &uuids);
  void Clear();

private:
  std::vector<std::string> selectedFixtures;
  std::vector<std::string> selectedTrusses;
  std::vector<std::string> selectedSupports;
  std::vector<std::string> selectedSceneObjects;
};

class HistoryManager {
public:
  struct Snapshot {
    MvrScene scene;
    std::vector<std::string> selFixtures;
    std::vector<std::string> selTrusses;
    std::vector<std::string> selSupports;
    std::vector<std::string> selSceneObjects;
    std::string description;
  };

  void PushUndoState(const MvrScene &scene, const SelectionState &selection,
                     const std::string &description = "");
  bool CanUndo() const;
  bool CanRedo() const;
  std::string Undo(MvrScene &scene, SelectionState &selection);
  std::string Redo(MvrScene &scene, SelectionState &selection);
  void ClearHistory();

private:
  std::vector<Snapshot> undoStack;
  std::vector<Snapshot> redoStack;
  size_t maxHistory = 20;
};

class LayerVisibilityState {
public:
  std::unordered_set<std::string> GetHiddenLayers() const;
  void SetHiddenLayers(const std::unordered_set<std::string> &layers);
  bool IsLayerVisible(const std::string &layer) const;

  void SetLayerColor(MvrScene &scene, const std::string &layer,
                     const std::string &color);
  std::optional<std::string> GetLayerColor(const MvrScene &scene,
                                           const std::string &layer) const;
  std::vector<std::string> GetLayerNames(const MvrScene &scene) const;

  const std::string &GetCurrentLayer() const;
  void SetCurrentLayer(const std::string &name);

private:
  std::unordered_set<std::string> hiddenLayers;
  std::string currentLayer = DEFAULT_LAYER_NAME;
};

class ProjectSession {
public:
  MvrScene &GetScene();
  const MvrScene &GetScene() const;

  bool IsDirty() const;
  void Touch();
  void MarkSaved();
  void ResetDirty();

private:
  MvrScene scene;
  size_t revision = 0;
  size_t savedRevision = 0;
};
