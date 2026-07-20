#include "configmanager.h"

#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> g_values;
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

// Destroys the lightweight ProjectSession test stub.
ProjectSession::~ProjectSession() = default;
