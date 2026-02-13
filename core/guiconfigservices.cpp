#include "guiconfigservices.h"

#include "configmanager.h"

namespace {

class ConfigManagerGuiConfigServices final : public IGuiConfigServices,
                                             public IGuiPreferencesService,
                                             public IGuiProjectSessionService,
                                             public IGuiSelectionService,
                                             public IGuiHistoryService,
                                             public IGuiLayerService {
public:
  explicit ConfigManagerGuiConfigServices(ConfigManager &config)
      : configManager(config) {}

  IGuiPreferencesService &Preferences() override { return *this; }
  const IGuiPreferencesService &Preferences() const override { return *this; }
  IGuiProjectSessionService &Project() override { return *this; }
  const IGuiProjectSessionService &Project() const override { return *this; }
  IGuiSelectionService &Selection() override { return *this; }
  const IGuiSelectionService &Selection() const override { return *this; }
  IGuiHistoryService &History() override { return *this; }
  const IGuiHistoryService &History() const override { return *this; }
  IGuiLayerService &Layers() override { return *this; }
  const IGuiLayerService &Layers() const override { return *this; }

  ConfigManager &LegacyConfigManager() override { return configManager; }
  const ConfigManager &LegacyConfigManager() const override { return configManager; }

  void SetValue(const std::string &key, const std::string &value) override { configManager.SetValue(key, value); }
  std::optional<std::string> GetValue(const std::string &key) const override { return configManager.GetValue(key); }
  void RemoveKey(const std::string &key) override { configManager.RemoveKey(key); }
  bool SaveUserConfig() const override { return configManager.SaveUserConfig(); }
  float GetFloat(const std::string &name) const override { return configManager.GetFloat(name); }
  void SetFloat(const std::string &name, float value) override { configManager.SetFloat(name, value); }

  bool SaveProject(const std::string &path) override { return configManager.SaveProject(path); }
  bool LoadProject(const std::string &path) override { return configManager.LoadProject(path); }
  MvrScene &GetScene() override { return configManager.GetScene(); }
  const MvrScene &GetScene() const override { return configManager.GetScene(); }
  void Reset() override { configManager.Reset(); }
  bool IsDirty() const override { return configManager.IsDirty(); }
  void MarkSaved() override { configManager.MarkSaved(); }

  const std::vector<std::string> &GetSelectedFixtures() const override { return configManager.GetSelectedFixtures(); }
  void SetSelectedFixtures(const std::vector<std::string> &uuids) override { configManager.SetSelectedFixtures(uuids); }
  const std::vector<std::string> &GetSelectedTrusses() const override { return configManager.GetSelectedTrusses(); }
  void SetSelectedTrusses(const std::vector<std::string> &uuids) override { configManager.SetSelectedTrusses(uuids); }
  const std::vector<std::string> &GetSelectedSupports() const override { return configManager.GetSelectedSupports(); }
  void SetSelectedSupports(const std::vector<std::string> &uuids) override { configManager.SetSelectedSupports(uuids); }
  const std::vector<std::string> &GetSelectedSceneObjects() const override { return configManager.GetSelectedSceneObjects(); }
  void SetSelectedSceneObjects(const std::vector<std::string> &uuids) override { configManager.SetSelectedSceneObjects(uuids); }

  void PushUndoState(const std::string &description) override { configManager.PushUndoState(description); }
  bool CanUndo() const override { return configManager.CanUndo(); }
  bool CanRedo() const override { return configManager.CanRedo(); }
  std::string Undo() override { return configManager.Undo(); }
  std::string Redo() override { return configManager.Redo(); }
  void ClearHistory() override { configManager.ClearHistory(); }

  std::unordered_set<std::string> GetHiddenLayers() const override { return configManager.GetHiddenLayers(); }
  void SetHiddenLayers(const std::unordered_set<std::string> &layers) override { configManager.SetHiddenLayers(layers); }
  bool IsLayerVisible(const std::string &layer) const override { return configManager.IsLayerVisible(layer); }
  void SetLayerColor(const std::string &layer, const std::string &color) override { configManager.SetLayerColor(layer, color); }
  std::optional<std::string> GetLayerColor(const std::string &layer) const override { return configManager.GetLayerColor(layer); }
  std::vector<std::string> GetLayerNames() const override { return configManager.GetLayerNames(); }
  const std::string &GetCurrentLayer() const override { return configManager.GetCurrentLayer(); }
  void SetCurrentLayer(const std::string &name) override { configManager.SetCurrentLayer(name); }

private:
  ConfigManager &configManager;
};

} // namespace

IGuiConfigServices &GetDefaultGuiConfigServices() {
  static ConfigManagerGuiConfigServices services(ConfigManager::Get());
  return services;
}
