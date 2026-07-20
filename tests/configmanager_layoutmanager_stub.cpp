#include "configmanager.h"
#include "mvrscene.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
std::unordered_map<std::string, std::string> g_values;
MvrScene g_scene;
std::vector<std::string> g_selectedFixtures;
std::vector<std::string> g_selectedTrusses;
std::vector<std::string> g_selectedSupports;
std::vector<std::string> g_selectedSceneObjects;
std::unordered_set<std::string> g_hiddenLayers;
std::string g_currentLayer;
}

// Constructs the lightweight ConfigManager test stub.
ConfigManager::ConfigManager() = default;

// Returns the singleton ConfigManager test stub.
ConfigManager &ConfigManager::Get() {
  static ConfigManager instance;
  return instance;
}

// Stores a string configuration value for tests.
void ConfigManager::SetValue(const std::string &key, const std::string &value) {
  g_values[key] = value;
}

// Looks up a string configuration value for tests.
std::optional<std::string> ConfigManager::GetValue(const std::string &key) const {
  auto it = g_values.find(key);
  if (it == g_values.end())
    return std::nullopt;
  return it->second;
}

// Reports whether a test configuration key is present.
bool ConfigManager::HasKey(const std::string &key) const {
  return g_values.find(key) != g_values.end();
}

// Removes a test configuration key.
void ConfigManager::RemoveKey(const std::string &key) { g_values.erase(key); }

// Clears all test configuration values.
void ConfigManager::ClearValues() { g_values.clear(); }

// Pretends to load user configuration in tests.
bool ConfigManager::LoadUserConfig() { return true; }

// Pretends to save user configuration in tests.
bool ConfigManager::SaveUserConfig() const { return true; }

// Returns the mutable scene stored by the lightweight ConfigManager test stub.
MvrScene &ConfigManager::GetScene() { return g_scene; }

// Returns the immutable scene stored by the lightweight ConfigManager test stub.
const MvrScene &ConfigManager::GetScene() const { return g_scene; }

// Returns selected fixture UUIDs tracked by the lightweight ConfigManager test stub.
const std::vector<std::string> &ConfigManager::GetSelectedFixtures() const {
  return g_selectedFixtures;
}

// Stores selected fixture UUIDs in the lightweight ConfigManager test stub.
void ConfigManager::SetSelectedFixtures(const std::vector<std::string> &uuids) {
  g_selectedFixtures = uuids;
}

// Returns selected truss UUIDs tracked by the lightweight ConfigManager test stub.
const std::vector<std::string> &ConfigManager::GetSelectedTrusses() const {
  return g_selectedTrusses;
}

// Stores selected truss UUIDs in the lightweight ConfigManager test stub.
void ConfigManager::SetSelectedTrusses(const std::vector<std::string> &uuids) {
  g_selectedTrusses = uuids;
}

// Returns selected support UUIDs tracked by the lightweight ConfigManager test stub.
const std::vector<std::string> &ConfigManager::GetSelectedSupports() const {
  return g_selectedSupports;
}

// Stores selected support UUIDs in the lightweight ConfigManager test stub.
void ConfigManager::SetSelectedSupports(const std::vector<std::string> &uuids) {
  g_selectedSupports = uuids;
}

// Returns selected scene-object UUIDs tracked by the lightweight ConfigManager test stub.
const std::vector<std::string> &ConfigManager::GetSelectedSceneObjects() const {
  return g_selectedSceneObjects;
}

// Stores selected scene-object UUIDs in the lightweight ConfigManager test stub.
void ConfigManager::SetSelectedSceneObjects(const std::vector<std::string> &uuids) {
  g_selectedSceneObjects = uuids;
}

// Returns hidden layer names tracked by the lightweight ConfigManager test stub.
std::unordered_set<std::string> ConfigManager::GetHiddenLayers() const {
  return g_hiddenLayers;
}

// Stores hidden layer names in the lightweight ConfigManager test stub.
void ConfigManager::SetHiddenLayers(const std::unordered_set<std::string> &layers) {
  g_hiddenLayers = layers;
}

// Returns the current layer tracked by the lightweight ConfigManager test stub.
const std::string &ConfigManager::GetCurrentLayer() const { return g_currentLayer; }

// Stores the current layer in the lightweight ConfigManager test stub.
void ConfigManager::SetCurrentLayer(const std::string &name) { g_currentLayer = name; }

// Ignores undo snapshots for lightweight ConfigManager test stub callers.
void ConfigManager::PushUndoState(const std::string &) {}

// Destroys the lightweight ProjectSession test stub.
ProjectSession::~ProjectSession() = default;
