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
#include "LayoutManager.h"

#include "../configmanager.h"
#include "../../external/json.hpp"

namespace layouts {
namespace {
constexpr const char *kLayoutsConfigKey = "layouts_collection";

std::string PageSizeToString(print::PageSize size) {
  switch (size) {
  case print::PageSize::A3:
    return "A3";
  case print::PageSize::A4:
  default:
    return "A4";
  }
}

print::PageSize PageSizeFromString(const std::string &value) {
  if (value == "A3")
    return print::PageSize::A3;
  return print::PageSize::A4;
}

nlohmann::json ToJson(const LayoutDefinition &layout) {
  return nlohmann::json{{"name", layout.name},
                        {"pageSize", PageSizeToString(layout.pageSetup.pageSize)},
                        {"landscape", layout.pageSetup.landscape},
                        {"viewId", layout.viewId}};
}

bool ParseLayout(const nlohmann::json &value, LayoutDefinition &out) {
  if (!value.is_object())
    return false;
  const auto nameIt = value.find("name");
  if (nameIt == value.end() || !nameIt->is_string())
    return false;
  out.name = nameIt->get<std::string>();
  if (out.name.empty())
    return false;

  const auto sizeIt = value.find("pageSize");
  if (sizeIt != value.end() && sizeIt->is_string())
    out.pageSetup.pageSize = PageSizeFromString(sizeIt->get<std::string>());
  else
    out.pageSetup.pageSize = print::PageSize::A4;

  const auto landscapeIt = value.find("landscape");
  out.pageSetup.landscape =
      landscapeIt != value.end() && landscapeIt->is_boolean()
          ? landscapeIt->get<bool>()
          : false;

  const auto viewIdIt = value.find("viewId");
  out.viewId = viewIdIt != value.end() && viewIdIt->is_string()
                   ? viewIdIt->get<std::string>()
                   : std::string{};
  return true;
}
} // namespace

LayoutManager::LayoutManager() = default;

LayoutManager &LayoutManager::Get() {
  static LayoutManager instance;
  return instance;
}

const LayoutCollection &LayoutManager::GetLayouts() const { return layouts; }

bool LayoutManager::AddLayout(const LayoutDefinition &layout) {
  if (!layouts.AddLayout(layout))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RenameLayout(const std::string &currentName,
                                 const std::string &newName) {
  if (!layouts.RenameLayout(currentName, newName))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayout(const std::string &name) {
  if (!layouts.RemoveLayout(name))
    return false;
  SyncToConfig();
  return true;
}

void LayoutManager::LoadFromConfig(ConfigManager &cfg) {
  auto value = cfg.GetValue(kLayoutsConfigKey);
  if (!value.has_value()) {
    SaveToConfig(cfg);
    return;
  }
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(*value);
  } catch (...) {
    return;
  }
  if (!parsed.is_array())
    return;

  std::vector<LayoutDefinition> loaded;
  for (const auto &entry : parsed) {
    LayoutDefinition layout;
    if (!ParseLayout(entry, layout))
      continue;
    bool duplicate = false;
    for (const auto &existing : loaded) {
      if (existing.name == layout.name) {
        duplicate = true;
        break;
      }
    }
    if (duplicate)
      continue;
    loaded.push_back(std::move(layout));
  }

  layouts.ReplaceAll(std::move(loaded));
}

void LayoutManager::SaveToConfig(ConfigManager &cfg) const {
  nlohmann::json data = nlohmann::json::array();
  for (const auto &layout : layouts.Items())
    data.push_back(ToJson(layout));
  cfg.SetValue(kLayoutsConfigKey, data.dump());
}

void LayoutManager::ResetToDefault(ConfigManager &cfg) {
  layouts = LayoutCollection();
  SaveToConfig(cfg);
}

void LayoutManager::SyncToConfig() { SaveToConfig(ConfigManager::Get()); }

} // namespace layouts
