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

#include <algorithm>
#include <unordered_set>

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

nlohmann::json ToJson(const Layout2DViewDefinition &view) {
  const auto &frame = view.frame;
  const auto &camera = view.camera;
  const auto &options = view.renderOptions;
  const auto &layers = view.layers;
  return {
      {"id", view.id},
      {"frame",
       {{"x", frame.x}, {"y", frame.y}, {"width", frame.width}, {"height", frame.height}}},
      {"camera",
       {{"offsetPixelsX", camera.offsetPixelsX},
        {"offsetPixelsY", camera.offsetPixelsY},
        {"zoom", camera.zoom},
        {"viewportWidth", camera.viewportWidth},
        {"viewportHeight", camera.viewportHeight},
        {"view", camera.view}}},
      {"renderOptions",
       {{"renderMode", options.renderMode},
        {"darkMode", options.darkMode},
        {"showGrid", options.showGrid},
        {"gridStyle", options.gridStyle},
        {"gridColorR", options.gridColorR},
        {"gridColorG", options.gridColorG},
        {"gridColorB", options.gridColorB},
        {"gridDrawAbove", options.gridDrawAbove},
        {"showLabelName", options.showLabelName},
        {"showLabelId", options.showLabelId},
        {"showLabelDmx", options.showLabelDmx},
        {"labelFontSizeName", options.labelFontSizeName},
        {"labelFontSizeId", options.labelFontSizeId},
        {"labelFontSizeDmx", options.labelFontSizeDmx},
        {"labelOffsetDistance", options.labelOffsetDistance},
        {"labelOffsetAngle", options.labelOffsetAngle}}},
      {"layers", {{"hiddenLayers", layers.hiddenLayers}}},
  };
}

nlohmann::json ToJson(const LayoutLegendDefinition &legend) {
  const auto &frame = legend.frame;
  return {{"id", legend.id},
          {"frame",
           {{"x", frame.x},
            {"y", frame.y},
            {"width", frame.width},
            {"height", frame.height}}}};
}

nlohmann::json ToJson(const LayoutDefinition &layout) {
  nlohmann::json data{{"name", layout.name},
                      {"pageSize", PageSizeToString(layout.pageSetup.pageSize)},
                      {"landscape", layout.pageSetup.landscape}};
  if (!layout.view2dViews.empty()) {
    data["view2dViews"] = nlohmann::json::array();
    for (const auto &view : layout.view2dViews)
      data["view2dViews"].push_back(ToJson(view));
  }
  if (!layout.legendViews.empty()) {
    data["legendViews"] = nlohmann::json::array();
    for (const auto &legend : layout.legendViews)
      data["legendViews"].push_back(ToJson(legend));
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

void ReadFrame(const nlohmann::json &obj, Layout2DViewFrame &frame) {
  if (auto it = obj.find("x"); it != obj.end() && it->is_number_integer())
    frame.x = it->get<int>();
  if (auto it = obj.find("y"); it != obj.end() && it->is_number_integer())
    frame.y = it->get<int>();
  if (auto it = obj.find("width"); it != obj.end() && it->is_number_integer())
    frame.width = it->get<int>();
  if (auto it = obj.find("height"); it != obj.end() && it->is_number_integer())
    frame.height = it->get<int>();
}

void ReadCamera(const nlohmann::json &obj, Layout2DViewCameraState &camera) {
  if (auto it = obj.find("offsetPixelsX"); it != obj.end() && it->is_number())
    camera.offsetPixelsX = it->get<float>();
  if (auto it = obj.find("offsetPixelsY"); it != obj.end() && it->is_number())
    camera.offsetPixelsY = it->get<float>();
  if (auto it = obj.find("zoom"); it != obj.end() && it->is_number())
    camera.zoom = it->get<float>();
  if (auto it = obj.find("viewportWidth");
      it != obj.end() && it->is_number_integer())
    camera.viewportWidth = it->get<int>();
  if (auto it = obj.find("viewportHeight");
      it != obj.end() && it->is_number_integer())
    camera.viewportHeight = it->get<int>();
  if (auto it = obj.find("view"); it != obj.end() && it->is_number())
    camera.view = it->get<int>();
}

void ReadRenderOptions(const nlohmann::json &obj,
                       Layout2DViewRenderOptions &options) {
  if (auto it = obj.find("renderMode"); it != obj.end() && it->is_number())
    options.renderMode = it->get<int>();
  if (auto it = obj.find("darkMode"); it != obj.end() && it->is_boolean())
    options.darkMode = it->get<bool>();
  if (auto it = obj.find("showGrid"); it != obj.end() && it->is_boolean())
    options.showGrid = it->get<bool>();
  if (auto it = obj.find("gridStyle"); it != obj.end() && it->is_number())
    options.gridStyle = it->get<int>();
  if (auto it = obj.find("gridColorR"); it != obj.end() && it->is_number())
    options.gridColorR = it->get<float>();
  if (auto it = obj.find("gridColorG"); it != obj.end() && it->is_number())
    options.gridColorG = it->get<float>();
  if (auto it = obj.find("gridColorB"); it != obj.end() && it->is_number())
    options.gridColorB = it->get<float>();
  if (auto it = obj.find("gridDrawAbove"); it != obj.end() && it->is_boolean())
    options.gridDrawAbove = it->get<bool>();
  ReadBoolArray(obj, "showLabelName", options.showLabelName);
  ReadBoolArray(obj, "showLabelId", options.showLabelId);
  ReadBoolArray(obj, "showLabelDmx", options.showLabelDmx);
  if (auto it = obj.find("labelFontSizeName");
      it != obj.end() && it->is_number())
    options.labelFontSizeName = it->get<float>();
  if (auto it = obj.find("labelFontSizeId");
      it != obj.end() && it->is_number())
    options.labelFontSizeId = it->get<float>();
  if (auto it = obj.find("labelFontSizeDmx");
      it != obj.end() && it->is_number())
    options.labelFontSizeDmx = it->get<float>();
  ReadFloatArray(obj, "labelOffsetDistance", options.labelOffsetDistance);
  ReadFloatArray(obj, "labelOffsetAngle", options.labelOffsetAngle);
}

void ReadLayers(const nlohmann::json &obj, Layout2DViewLayers &layers) {
  if (auto it = obj.find("hiddenLayers"); it != obj.end() && it->is_array()) {
    layers.hiddenLayers.clear();
    for (const auto &entry : *it) {
      if (entry.is_string())
        layers.hiddenLayers.push_back(entry.get<std::string>());
    }
  }
}

bool ParseLayout2DView(const nlohmann::json &value,
                       Layout2DViewDefinition &out) {
  if (!value.is_object())
    return false;
  if (auto idIt = value.find("id");
      idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame);
  auto cameraIt = value.find("camera");
  if (cameraIt != value.end() && cameraIt->is_object())
    ReadCamera(*cameraIt, out.camera);
  auto renderIt = value.find("renderOptions");
  if (renderIt != value.end() && renderIt->is_object())
    ReadRenderOptions(*renderIt, out.renderOptions);
  auto layersIt = value.find("layers");
  if (layersIt != value.end() && layersIt->is_object())
    ReadLayers(*layersIt, out.layers);
  return true;
}

bool ParseLayoutLegend(const nlohmann::json &value,
                       LayoutLegendDefinition &out) {
  if (!value.is_object())
    return false;
  if (auto idIt = value.find("id");
      idIt != value.end() && idIt->is_number_integer())
    out.id = idIt->get<int>();
  auto frameIt = value.find("frame");
  if (frameIt != value.end() && frameIt->is_object())
    ReadFrame(*frameIt, out.frame);
  return true;
}

void EnsureUniqueViewIds(LayoutDefinition &layout) {
  std::unordered_set<int> used;
  int nextId = 1;
  for (auto &view : layout.view2dViews) {
    if (view.id > 0) {
      if (!used.insert(view.id).second)
        view.id = 0;
      else
        nextId = std::max(nextId, view.id + 1);
    }
  }
  for (auto &view : layout.view2dViews) {
    if (view.id > 0)
      continue;
    while (used.count(nextId))
      ++nextId;
    view.id = nextId;
    used.insert(nextId);
    ++nextId;
  }
}

void EnsureUniqueLegendIds(LayoutDefinition &layout) {
  std::unordered_set<int> used;
  int nextId = 1;
  for (auto &legend : layout.legendViews) {
    if (legend.id > 0) {
      if (!used.insert(legend.id).second)
        legend.id = 0;
      else
        nextId = std::max(nextId, legend.id + 1);
    }
  }
  for (auto &legend : layout.legendViews) {
    if (legend.id > 0)
      continue;
    while (used.count(nextId))
      ++nextId;
    legend.id = nextId;
    used.insert(nextId);
    ++nextId;
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

  out.view2dViews.clear();
  if (auto viewsIt = value.find("view2dViews");
      viewsIt != value.end() && viewsIt->is_array()) {
    for (const auto &entry : *viewsIt) {
      Layout2DViewDefinition view;
      if (!ParseLayout2DView(entry, view))
        continue;
      bool replaced = false;
      if (view.id > 0) {
        for (auto &existing : out.view2dViews) {
          if (existing.id == view.id) {
            existing = view;
            replaced = true;
            break;
          }
        }
      }
      if (!replaced)
        out.view2dViews.push_back(std::move(view));
    }
  }

  out.legendViews.clear();
  if (auto legendsIt = value.find("legendViews");
      legendsIt != value.end() && legendsIt->is_array()) {
    for (const auto &entry : *legendsIt) {
      LayoutLegendDefinition legend;
      if (!ParseLayoutLegend(entry, legend))
        continue;
      bool replaced = false;
      if (legend.id > 0) {
        for (auto &existing : out.legendViews) {
          if (existing.id == legend.id) {
            existing = legend;
            replaced = true;
            break;
          }
        }
      }
      if (!replaced)
        out.legendViews.push_back(std::move(legend));
    }
  }

  if (out.view2dViews.empty()) {
    const auto viewStateIt = value.find("view2dState");
    if (viewStateIt != value.end() && viewStateIt->is_object()) {
      const auto &viewObj = *viewStateIt;
      Layout2DViewDefinition view;
      Layout2DViewCameraState camera;
      Layout2DViewRenderOptions options;
      Layout2DViewLayers layers;
      if (auto it = viewObj.find("offsetPixelsX");
          it != viewObj.end() && it->is_number())
        camera.offsetPixelsX = it->get<float>();
      if (auto it = viewObj.find("offsetPixelsY");
          it != viewObj.end() && it->is_number())
        camera.offsetPixelsY = it->get<float>();
      if (auto it = viewObj.find("zoom"); it != viewObj.end() && it->is_number())
        camera.zoom = it->get<float>();
      if (auto it = viewObj.find("viewportWidth");
          it != viewObj.end() && it->is_number_integer())
        camera.viewportWidth = it->get<int>();
      if (auto it = viewObj.find("viewportHeight");
          it != viewObj.end() && it->is_number_integer())
        camera.viewportHeight = it->get<int>();
      if (auto it = viewObj.find("view"); it != viewObj.end() && it->is_number())
        camera.view = it->get<int>();
      if (auto it = viewObj.find("renderMode");
          it != viewObj.end() && it->is_number())
        options.renderMode = it->get<int>();
      if (auto it = viewObj.find("darkMode");
          it != viewObj.end() && it->is_boolean())
        options.darkMode = it->get<bool>();
      if (auto it = viewObj.find("showGrid");
          it != viewObj.end() && it->is_boolean())
        options.showGrid = it->get<bool>();
      if (auto it = viewObj.find("gridStyle");
          it != viewObj.end() && it->is_number())
        options.gridStyle = it->get<int>();
      if (auto it = viewObj.find("gridColorR");
          it != viewObj.end() && it->is_number())
        options.gridColorR = it->get<float>();
      if (auto it = viewObj.find("gridColorG");
          it != viewObj.end() && it->is_number())
        options.gridColorG = it->get<float>();
      if (auto it = viewObj.find("gridColorB");
          it != viewObj.end() && it->is_number())
        options.gridColorB = it->get<float>();
      if (auto it = viewObj.find("gridDrawAbove");
          it != viewObj.end() && it->is_boolean())
        options.gridDrawAbove = it->get<bool>();
      ReadBoolArray(viewObj, "showLabelName", options.showLabelName);
      ReadBoolArray(viewObj, "showLabelId", options.showLabelId);
      ReadBoolArray(viewObj, "showLabelDmx", options.showLabelDmx);
      if (auto it = viewObj.find("labelFontSizeName");
          it != viewObj.end() && it->is_number())
        options.labelFontSizeName = it->get<float>();
      if (auto it = viewObj.find("labelFontSizeId");
          it != viewObj.end() && it->is_number())
        options.labelFontSizeId = it->get<float>();
      if (auto it = viewObj.find("labelFontSizeDmx");
          it != viewObj.end() && it->is_number())
        options.labelFontSizeDmx = it->get<float>();
      ReadFloatArray(viewObj, "labelOffsetDistance", options.labelOffsetDistance);
      ReadFloatArray(viewObj, "labelOffsetAngle", options.labelOffsetAngle);
      if (auto it = viewObj.find("hiddenLayers");
          it != viewObj.end() && it->is_array()) {
        layers.hiddenLayers.clear();
        for (const auto &entry : *it) {
          if (entry.is_string())
            layers.hiddenLayers.push_back(entry.get<std::string>());
        }
      }
      Layout2DViewFrame frame;
      if (auto it = viewObj.find("frameWidth");
          it != viewObj.end() && it->is_number_integer())
        frame.width = it->get<int>();
      if (auto it = viewObj.find("frameHeight");
          it != viewObj.end() && it->is_number_integer())
        frame.height = it->get<int>();
      view.frame = frame;
      view.camera = camera;
      view.renderOptions = options;
      view.layers = std::move(layers);
      out.view2dViews.push_back(std::move(view));
    }
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

bool LayoutManager::UpdateLayout2DView(const std::string &name,
                                       const Layout2DViewDefinition &view) {
  if (!layouts.UpdateLayout2DView(name, view))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayout2DView(const std::string &name, int viewId) {
  if (!layouts.RemoveLayout2DView(name, viewId))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::UpdateLayoutLegend(const std::string &name,
                                       const LayoutLegendDefinition &legend) {
  if (!layouts.UpdateLayoutLegend(name, legend))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayoutLegend(const std::string &name, int legendId) {
  if (!layouts.RemoveLayoutLegend(name, legendId))
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
    EnsureUniqueViewIds(layout);
    EnsureUniqueLegendIds(layout);
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
