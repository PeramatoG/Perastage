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
  nlohmann::json data{{"name", layout.name},
                      {"pageSize", PageSizeToString(layout.pageSetup.pageSize)},
                      {"landscape", layout.pageSetup.landscape}};
  if (layout.hasView2dState) {
    const auto &state = layout.view2dState;
    data["view2dState"] = {
        {"offsetPixelsX", state.offsetPixelsX},
        {"offsetPixelsY", state.offsetPixelsY},
        {"zoom", state.zoom},
        {"viewportWidth", state.viewportWidth},
        {"viewportHeight", state.viewportHeight},
        {"view", state.view},
        {"renderMode", state.renderMode},
        {"darkMode", state.darkMode},
        {"showGrid", state.showGrid},
        {"gridStyle", state.gridStyle},
        {"gridColorR", state.gridColorR},
        {"gridColorG", state.gridColorG},
        {"gridColorB", state.gridColorB},
        {"gridDrawAbove", state.gridDrawAbove},
        {"showLabelName", state.showLabelName},
        {"showLabelId", state.showLabelId},
        {"showLabelDmx", state.showLabelDmx},
        {"labelFontSizeName", state.labelFontSizeName},
        {"labelFontSizeId", state.labelFontSizeId},
        {"labelFontSizeDmx", state.labelFontSizeDmx},
        {"labelOffsetDistance", state.labelOffsetDistance},
        {"labelOffsetAngle", state.labelOffsetAngle},
        {"hiddenLayers", state.hiddenLayers},
        {"frameWidth", state.frameWidth},
        {"frameHeight", state.frameHeight},
    };
  }
  return data;
}

void ReadBoolArray(const nlohmann::json &obj, const char *key,
                   std::array<bool, 3> &out) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_array())
    return;
  for (size_t idx = 0; idx < out.size() && idx < it->size(); ++idx) {
    const auto &entry = (*it)[idx];
    if (entry.is_boolean())
      out[idx] = entry.get<bool>();
  }
}

void ReadFloatArray(const nlohmann::json &obj, const char *key,
                    std::array<float, 3> &out) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_array())
    return;
  for (size_t idx = 0; idx < out.size() && idx < it->size(); ++idx) {
    const auto &entry = (*it)[idx];
    if (entry.is_number())
      out[idx] = entry.get<float>();
  }
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

  out.hasView2dState = false;
  const auto viewStateIt = value.find("view2dState");
  if (viewStateIt != value.end() && viewStateIt->is_object()) {
    const auto &viewObj = *viewStateIt;
    Layout2DViewState state;
    if (auto it = viewObj.find("offsetPixelsX"); it != viewObj.end() && it->is_number())
      state.offsetPixelsX = it->get<float>();
    if (auto it = viewObj.find("offsetPixelsY"); it != viewObj.end() && it->is_number())
      state.offsetPixelsY = it->get<float>();
    if (auto it = viewObj.find("zoom"); it != viewObj.end() && it->is_number())
      state.zoom = it->get<float>();
    if (auto it = viewObj.find("viewportWidth");
        it != viewObj.end() && it->is_number_integer())
      state.viewportWidth = it->get<int>();
    if (auto it = viewObj.find("viewportHeight");
        it != viewObj.end() && it->is_number_integer())
      state.viewportHeight = it->get<int>();
    if (auto it = viewObj.find("view"); it != viewObj.end() && it->is_number())
      state.view = it->get<int>();
    if (auto it = viewObj.find("renderMode");
        it != viewObj.end() && it->is_number())
      state.renderMode = it->get<int>();
    if (auto it = viewObj.find("darkMode");
        it != viewObj.end() && it->is_boolean())
      state.darkMode = it->get<bool>();
    if (auto it = viewObj.find("showGrid");
        it != viewObj.end() && it->is_boolean())
      state.showGrid = it->get<bool>();
    if (auto it = viewObj.find("gridStyle");
        it != viewObj.end() && it->is_number())
      state.gridStyle = it->get<int>();
    if (auto it = viewObj.find("gridColorR");
        it != viewObj.end() && it->is_number())
      state.gridColorR = it->get<float>();
    if (auto it = viewObj.find("gridColorG");
        it != viewObj.end() && it->is_number())
      state.gridColorG = it->get<float>();
    if (auto it = viewObj.find("gridColorB");
        it != viewObj.end() && it->is_number())
      state.gridColorB = it->get<float>();
    if (auto it = viewObj.find("gridDrawAbove");
        it != viewObj.end() && it->is_boolean())
      state.gridDrawAbove = it->get<bool>();
    ReadBoolArray(viewObj, "showLabelName", state.showLabelName);
    ReadBoolArray(viewObj, "showLabelId", state.showLabelId);
    ReadBoolArray(viewObj, "showLabelDmx", state.showLabelDmx);
    if (auto it = viewObj.find("labelFontSizeName");
        it != viewObj.end() && it->is_number())
      state.labelFontSizeName = it->get<float>();
    if (auto it = viewObj.find("labelFontSizeId");
        it != viewObj.end() && it->is_number())
      state.labelFontSizeId = it->get<float>();
    if (auto it = viewObj.find("labelFontSizeDmx");
        it != viewObj.end() && it->is_number())
      state.labelFontSizeDmx = it->get<float>();
    ReadFloatArray(viewObj, "labelOffsetDistance", state.labelOffsetDistance);
    ReadFloatArray(viewObj, "labelOffsetAngle", state.labelOffsetAngle);
    if (auto it = viewObj.find("hiddenLayers");
        it != viewObj.end() && it->is_array()) {
      state.hiddenLayers.clear();
      for (const auto &entry : *it) {
        if (entry.is_string())
          state.hiddenLayers.push_back(entry.get<std::string>());
      }
    }
    if (auto it = viewObj.find("frameWidth");
        it != viewObj.end() && it->is_number_integer())
      state.frameWidth = it->get<int>();
    if (auto it = viewObj.find("frameHeight");
        it != viewObj.end() && it->is_number_integer())
      state.frameHeight = it->get<int>();
    out.view2dState = std::move(state);
    out.hasView2dState = true;
  }

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

bool LayoutManager::SetLayoutOrientation(const std::string &name,
                                         bool landscape) {
  if (!layouts.SetLayoutOrientation(name, landscape))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::UpdateLayout2DViewState(const std::string &name,
                                            const Layout2DViewState &state) {
  if (!layouts.UpdateLayout2DViewState(name, state))
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
