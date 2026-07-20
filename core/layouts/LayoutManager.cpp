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
#include "LayoutImageResourceRegistry.h"

#include "LayoutDefaultsLoader.h"
#include "LayoutTemplatePackageService.h"
#include "LayoutTemplateSerializer.h"
#include "configmanager.h"
#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_map>
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

// Ensures every image element in a layout has a stable unique identifier.
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

// Finds an existing image element by layout name and element identifier.
const LayoutImageDefinition *FindImage(const LayoutCollection &layouts,
                                       const std::string &layoutName,
                                       int imageId) {
  for (const auto &layout : layouts.Items()) {
    if (layout.name != layoutName)
      continue;
    for (const auto &image : layout.imageViews) {
      if (image.id == imageId && imageId > 0)
        return &image;
    }
  }
  return nullptr;
}

// Captures or refreshes packaged resource metadata before an image is stored.
void PrepareImageResource(LayoutImageDefinition &image,
                          const LayoutImageDefinition *existingImage) {
  if (existingImage != nullptr && image.imagePath != existingImage->imagePath) {
    image.originalImagePath.clear();
    image.projectResourcePath.clear();
  }
  LayoutImageResourceRegistry::Get().AttachResource(image);
}

} // namespace

LayoutManager::LayoutManager() = default;

LayoutManager &LayoutManager::Get() {
  static LayoutManager instance;
  return instance;
}

const LayoutCollection &LayoutManager::GetLayouts() const { return layouts; }

// Adds a layout after capturing any image resources it references.
bool LayoutManager::AddLayout(const LayoutDefinition &layout) {
  LayoutDefinition preparedLayout = layout;
  for (auto &image : preparedLayout.imageViews)
    PrepareImageResource(image, nullptr);
  if (!layouts.AddLayout(preparedLayout))
    return false;
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
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

// Removes a layout and refreshes image resource usage counters.
bool LayoutManager::RemoveLayout(const std::string &name) {
  if (!layouts.RemoveLayout(name))
    return false;
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
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

// Updates or adds an image element while capturing its packaged resource.
bool LayoutManager::UpdateLayoutImage(const std::string &name,
                                      const LayoutImageDefinition &image) {
  LayoutImageDefinition preparedImage = image;
  PrepareImageResource(preparedImage, FindImage(layouts, name, image.id));
  if (!layouts.UpdateLayoutImage(name, preparedImage))
    return false;
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
  SyncToConfig();
  return true;
}

// Removes an image element and refreshes packaged resource usage counters.
bool LayoutManager::RemoveLayoutImage(const std::string &name, int imageId) {
  if (!layouts.RemoveLayoutImage(name, imageId))
    return false;
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
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

// Exports the named layout as a portable layout package.
bool LayoutManager::ExportLayoutTemplate(const std::string &name,
                                         const std::string &filePath,
                                         std::string *error) const {
  const auto &items = layouts.Items();
  auto it = std::find_if(items.begin(), items.end(),
                         [&name](const LayoutDefinition &layout) {
                           return layout.name == name;
                         });
  if (it == items.end()) {
    if (error)
      *error = "Layout not found.";
    return false;
  }
  return LayoutTemplatePackageService::ExportPackage(*it, filePath, error);
}

// Imports a portable package or legacy JSON template and adds it as a layout.
bool LayoutManager::ImportLayoutTemplate(const std::string &filePath,
                                         const std::string &preferredName,
                                         std::string *createdLayoutName,
                                         std::string *error) {
  LayoutTemplateImportResult importResult;
  if (!LayoutTemplatePackageService::ImportFile(filePath, importResult, error))
    return false;

  LayoutDefinition imported = importResult.layout;
  EnsureUniqueViewIds(imported);
  EnsureUniqueLegendIds(imported);
  EnsureUniqueEventTableIds(imported);
  EnsureUniqueTextIds(imported);
  EnsureUniqueImageIds(imported);
  if (!preferredName.empty())
    imported.name = preferredName;
  imported.name = MakeUniqueLayoutName(imported.name);

  if (!AddLayout(imported)) {
    if (error)
      *error = "Could not add imported layout.";
    return false;
  }

  if (createdLayoutName)
    *createdLayoutName = imported.name;
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

// Loads layout state from configuration and restores image resource tracking.
void LayoutManager::LoadFromConfig(ConfigManager &cfg) {
  auto loadDefaultsOrFallback = [&cfg, this]() {
    if (!LoadDefaultsForNewProject(cfg))
      SaveToConfig(cfg);
  };

  auto value = cfg.GetValue(kLayoutsConfigKey);
  if (!value.has_value()) {
    loadDefaultsOrFallback();
    return;
  }
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(*value);
  } catch (...) {
    loadDefaultsOrFallback();
    return;
  }
  std::vector<LayoutDefinition> loaded;
  std::string parseError;
  if (!FromTemplateDocument(parsed, loaded, &parseError) || loaded.empty()) {
    loadDefaultsOrFallback();
    return;
  }

  for (auto &layout : loaded) {
    EnsureUniqueViewIds(layout);
    EnsureUniqueLegendIds(layout);
    EnsureUniqueEventTableIds(layout);
    EnsureUniqueTextIds(layout);
    EnsureUniqueImageIds(layout);
  }

  layouts.ReplaceAll(std::move(loaded));
  PrepareImageResourcesForSave();
}

// Saves the current layout collection to configuration metadata.
void LayoutManager::SaveToConfig(ConfigManager &cfg) const {
  cfg.SetValue(kLayoutsConfigKey, ToTemplateDocument(layouts.Items()).dump());
}

// Ensures current layout image resources are captured and usage counters are current.
void LayoutManager::PrepareImageResourcesForSave() {
  for (auto &layout : layouts.Items()) {
    for (auto &image : layout.imageViews)
      PrepareImageResource(image, nullptr);
  }
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
}

// Resets layouts to defaults and clears unused image resource references.
void LayoutManager::ResetToDefault(ConfigManager &cfg) {
  layouts = LayoutCollection();
  LayoutImageResourceRegistry::Get().SynchronizeWithLayouts(layouts);
  SaveToConfig(cfg);
}

// Loads bundled default layouts and prepares their image resources.
bool LayoutManager::LoadDefaultsForNewProject(ConfigManager &cfg) {
  LayoutDefaultsLoadResult loadedDefaults = LoadLayoutDefaultsFromLibrary();
  if (loadedDefaults.layouts.empty())
    return false;

  std::unordered_map<std::string, int> suffixByBaseName;
  std::unordered_set<std::string> usedNames;
  for (auto &layout : loadedDefaults.layouts) {
    EnsureUniqueViewIds(layout);
    EnsureUniqueLegendIds(layout);
    EnsureUniqueEventTableIds(layout);
    EnsureUniqueTextIds(layout);
    EnsureUniqueImageIds(layout);

    std::string baseName = layout.name.empty() ? "Layout" : layout.name;
    int suffix = suffixByBaseName[baseName];
    std::string candidate = baseName;
    if (suffix > 1)
      candidate = baseName + " (" + std::to_string(suffix) + ")";
    while (usedNames.count(candidate) > 0) {
      ++suffix;
      candidate = baseName + " (" + std::to_string(suffix) + ")";
    }
    if (suffix <= 1)
      suffixByBaseName[baseName] = 2;
    else
      suffixByBaseName[baseName] = suffix + 1;
    usedNames.insert(candidate);
    layout.name = candidate;
  }

  layouts.ReplaceAll(std::move(loadedDefaults.layouts));
  PrepareImageResourcesForSave();
  SaveToConfig(cfg);
  return true;
}

void LayoutManager::SyncToConfig() {
  if (batchDepth > 0) {
    pendingSync = true;
    return;
  }
  pendingSync = false;
  SaveToConfig(ConfigManager::Get());
}

std::string LayoutManager::MakeUniqueLayoutName(
    const std::string &baseName) const {
  const std::string safeBase = baseName.empty() ? "Layout" : baseName;
  std::string candidate = safeBase;
  int suffix = 2;

  const auto &items = layouts.Items();
  auto exists = [&items](const std::string &name) {
    return std::any_of(items.begin(), items.end(),
                       [&name](const LayoutDefinition &layout) {
                         return layout.name == name;
                       });
  };

  while (exists(candidate)) {
    candidate = safeBase + " (" + std::to_string(suffix) + ")";
    ++suffix;
  }
  return candidate;
}

} // namespace layouts
