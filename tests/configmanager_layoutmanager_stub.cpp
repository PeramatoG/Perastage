#include "configmanager.h"

#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> g_values;
}

ConfigManager::ConfigManager() = default;

ConfigManager &ConfigManager::Get() {
  static ConfigManager instance;
  return instance;
}

void ConfigManager::SetValue(const std::string &key, const std::string &value) {
  g_values[key] = value;
}

std::optional<std::string> ConfigManager::GetValue(const std::string &key) const {
  auto it = g_values.find(key);
  if (it == g_values.end())
    return std::nullopt;
  return it->second;
}

bool ConfigManager::HasKey(const std::string &key) const {
  return g_values.find(key) != g_values.end();
}

void ConfigManager::RemoveKey(const std::string &key) { g_values.erase(key); }

void ConfigManager::ClearValues() { g_values.clear(); }
