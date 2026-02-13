#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "mvrscene.h"

class ConfigManager;

class IGuiPreferencesService {
public:
  virtual ~IGuiPreferencesService() = default;
  virtual void SetValue(const std::string &key, const std::string &value) = 0;
  virtual std::optional<std::string> GetValue(const std::string &key) const = 0;
  virtual void RemoveKey(const std::string &key) = 0;
  virtual bool SaveUserConfig() const = 0;
  virtual float GetFloat(const std::string &name) const = 0;
  virtual void SetFloat(const std::string &name, float value) = 0;
};

class IGuiProjectSessionService {
public:
  virtual ~IGuiProjectSessionService() = default;
  virtual bool SaveProject(const std::string &path) = 0;
  virtual bool LoadProject(const std::string &path) = 0;
  virtual MvrScene &GetScene() = 0;
  virtual const MvrScene &GetScene() const = 0;
  virtual void Reset() = 0;
  virtual bool IsDirty() const = 0;
  virtual void MarkSaved() = 0;
};

class IGuiSelectionService {
public:
  virtual ~IGuiSelectionService() = default;
  virtual const std::vector<std::string> &GetSelectedFixtures() const = 0;
  virtual void SetSelectedFixtures(const std::vector<std::string> &uuids) = 0;
  virtual const std::vector<std::string> &GetSelectedTrusses() const = 0;
  virtual void SetSelectedTrusses(const std::vector<std::string> &uuids) = 0;
  virtual const std::vector<std::string> &GetSelectedSupports() const = 0;
  virtual void SetSelectedSupports(const std::vector<std::string> &uuids) = 0;
  virtual const std::vector<std::string> &GetSelectedSceneObjects() const = 0;
  virtual void SetSelectedSceneObjects(const std::vector<std::string> &uuids) = 0;
};

class IGuiHistoryService {
public:
  virtual ~IGuiHistoryService() = default;
  virtual void PushUndoState(const std::string &description = "") = 0;
  virtual bool CanUndo() const = 0;
  virtual bool CanRedo() const = 0;
  virtual std::string Undo() = 0;
  virtual std::string Redo() = 0;
  virtual void ClearHistory() = 0;
};

class IGuiLayerService {
public:
  virtual ~IGuiLayerService() = default;
  virtual std::unordered_set<std::string> GetHiddenLayers() const = 0;
  virtual void SetHiddenLayers(const std::unordered_set<std::string> &layers) = 0;
  virtual bool IsLayerVisible(const std::string &layer) const = 0;
  virtual void SetLayerColor(const std::string &layer, const std::string &color) = 0;
  virtual std::optional<std::string> GetLayerColor(const std::string &layer) const = 0;
  virtual std::vector<std::string> GetLayerNames() const = 0;
  virtual const std::string &GetCurrentLayer() const = 0;
  virtual void SetCurrentLayer(const std::string &name) = 0;
};

class IGuiConfigServices {
public:
  virtual ~IGuiConfigServices() = default;
  virtual IGuiPreferencesService &Preferences() = 0;
  virtual const IGuiPreferencesService &Preferences() const = 0;
  virtual IGuiProjectSessionService &Project() = 0;
  virtual const IGuiProjectSessionService &Project() const = 0;
  virtual IGuiSelectionService &Selection() = 0;
  virtual const IGuiSelectionService &Selection() const = 0;
  virtual IGuiHistoryService &History() = 0;
  virtual const IGuiHistoryService &History() const = 0;
  virtual IGuiLayerService &Layers() = 0;
  virtual const IGuiLayerService &Layers() const = 0;

  // Transitional bridge for non-migrated GUI code.
  virtual ConfigManager &LegacyConfigManager() = 0;
  virtual const ConfigManager &LegacyConfigManager() const = 0;
};

IGuiConfigServices &GetDefaultGuiConfigServices();
