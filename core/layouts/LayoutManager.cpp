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

#include "LayoutTemplateSerializer.h"
#include "configmanager.h"
#include "json.hpp"

#include <algorithm>
#include <unordered_set>

namespace layouts {
namespace {
constexpr const char *kLayoutsConfigKey = "layouts_collection";

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

void EnsureUniqueEventTableIds(LayoutDefinition &layout) {
  std::unordered_set<int> used;
  int nextId = 1;
  for (auto &table : layout.eventTables) {
    if (table.id > 0) {
      if (!used.insert(table.id).second)
        table.id = 0;
      else
        nextId = std::max(nextId, table.id + 1);
    }
  }
  for (auto &table : layout.eventTables) {
    if (table.id > 0)
      continue;
    while (used.count(nextId))
      ++nextId;
    table.id = nextId;
    used.insert(nextId);
    ++nextId;
  }
}

void EnsureUniqueTextIds(LayoutDefinition &layout) {
  std::unordered_set<int> used;
  int nextId = 1;
  for (auto &text : layout.textViews) {
    if (text.id > 0) {
      if (!used.insert(text.id).second)
        text.id = 0;
      else
        nextId = std::max(nextId, text.id + 1);
    }
  }
  for (auto &text : layout.textViews) {
    if (text.id > 0)
      continue;
    while (used.count(nextId))
      ++nextId;
    text.id = nextId;
    used.insert(nextId);
    ++nextId;
  }
}

void EnsureUniqueImageIds(LayoutDefinition &layout) {
  std::unordered_set<int> used;
  int nextId = 1;
  for (auto &image : layout.imageViews) {
    if (image.id > 0) {
      if (!used.insert(image.id).second)
        image.id = 0;
      else
        nextId = std::max(nextId, image.id + 1);
    }
  }
  for (auto &image : layout.imageViews) {
    if (image.id > 0)
      continue;
    while (used.count(nextId))
      ++nextId;
    image.id = nextId;
    used.insert(nextId);
    ++nextId;
  }
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

bool LayoutManager::MoveLayout2DView(const std::string &name, int viewId,
                                     bool toFront) {
  if (!layouts.MoveLayout2DView(name, viewId, toFront))
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

bool LayoutManager::MoveLayoutLegend(const std::string &name, int legendId,
                                     bool toFront) {
  if (!layouts.MoveLayoutLegend(name, legendId, toFront))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::UpdateLayoutEventTable(const std::string &name,
                                           const LayoutEventTableDefinition &table) {
  if (!layouts.UpdateLayoutEventTable(name, table))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayoutEventTable(const std::string &name, int tableId) {
  if (!layouts.RemoveLayoutEventTable(name, tableId))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::MoveLayoutEventTable(const std::string &name, int tableId,
                                         bool toFront) {
  if (!layouts.MoveLayoutEventTable(name, tableId, toFront))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::UpdateLayoutText(const std::string &name,
                                     const LayoutTextDefinition &text) {
  if (!layouts.UpdateLayoutText(name, text))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayoutText(const std::string &name, int textId) {
  if (!layouts.RemoveLayoutText(name, textId))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::MoveLayoutText(const std::string &name, int textId,
                                   bool toFront) {
  if (!layouts.MoveLayoutText(name, textId, toFront))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::UpdateLayoutImage(const std::string &name,
                                      const LayoutImageDefinition &image) {
  if (!layouts.UpdateLayoutImage(name, image))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::RemoveLayoutImage(const std::string &name, int imageId) {
  if (!layouts.RemoveLayoutImage(name, imageId))
    return false;
  SyncToConfig();
  return true;
}

bool LayoutManager::MoveLayoutImage(const std::string &name, int imageId,
                                    bool toFront) {
  if (!layouts.MoveLayoutImage(name, imageId, toFront))
    return false;
  SyncToConfig();
  return true;
}

void LayoutManager::BeginBatchUpdate() { ++batchDepth; }

void LayoutManager::EndBatchUpdate() {
  if (batchDepth <= 0)
    return;
  --batchDepth;
  if (batchDepth == 0 && pendingSync) {
    pendingSync = false;
    SaveToConfig(ConfigManager::Get());
  }
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
  std::vector<LayoutDefinition> loaded;
  std::string parseError;
  if (!FromTemplateDocument(parsed, loaded, &parseError))
    return;

  for (auto &layout : loaded) {
    EnsureUniqueViewIds(layout);
    EnsureUniqueLegendIds(layout);
    EnsureUniqueEventTableIds(layout);
    EnsureUniqueTextIds(layout);
    EnsureUniqueImageIds(layout);
  }

  layouts.ReplaceAll(std::move(loaded));
}

void LayoutManager::SaveToConfig(ConfigManager &cfg) const {
  cfg.SetValue(kLayoutsConfigKey, ToTemplateDocument(layouts.Items()).dump());
}

void LayoutManager::ResetToDefault(ConfigManager &cfg) {
  layouts = LayoutCollection();
  SaveToConfig(cfg);
}

void LayoutManager::SyncToConfig() {
  if (batchDepth > 0) {
    pendingSync = true;
    return;
  }
  pendingSync = false;
  SaveToConfig(ConfigManager::Get());
}

} // namespace layouts
