#include "layer_service.h"

#include "configmanager.h"
#include "utf8_utils.h"
#include "uuidutils.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace layerdomain {
namespace {

// Reports a layer-domain action in a concise structured form.
void LogLayerAction(const char *action, const LayerResult &result) {
  std::cerr << "layer action=" << action << " status=" << static_cast<int>(result.status)
            << " uuid=" << result.layerUuid << " name="
            << EscapeTextForDiagnostics(result.layerName) << " message="
            << result.message << '\n';
}

// Returns true when the color is a canonical #RRGGBB value.
bool IsCanonicalColor(const std::string &color) {
  if (color.empty())
    return true;
  if (color.size() != 7 || color[0] != '#')
    return false;
  return std::all_of(color.begin() + 1, color.end(), [](unsigned char c) {
    return std::isxdigit(c) != 0 && !std::islower(c);
  });
}

// Trims ASCII and common Unicode whitespace without changing interior text.
std::string TrimLayerName(const std::string &name) {
  auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  size_t first = 0;
  size_t last = name.size();
  while (first < last && isSpace(static_cast<unsigned char>(name[first])))
    ++first;
  while (last > first && isSpace(static_cast<unsigned char>(name[last - 1])))
    --last;
  return name.substr(first, last - first);
}

// Validates and normalizes a layer name for editable scene storage.
LayerResult NormalizeLayerName(const std::string &name, std::string &out) {
  if (!IsValidUtf8(name))
    return {LayerStatus::InvalidUtf8, "Layer name is not valid UTF-8"};
  out = TrimLayerName(name);
  if (out.empty())
    return {LayerStatus::ValidationFailure, "Layer name is empty"};
  if (out == DEFAULT_LAYER_NAME)
    return {LayerStatus::ValidationFailure, "Default layer is reserved"};
  if (out.size() > kMaxLayerNameBytes)
    return {LayerStatus::ValidationFailure, "Layer name is too long"};
  for (unsigned char c : out) {
    if (c < 0x20 || c == 0x7F)
      return {LayerStatus::ValidationFailure,
              "Layer name contains a control character"};
  }
  return {};
}

// Returns the layer name for an existing UUID.
std::optional<std::string> FindLayerName(const MvrScene &scene,
                                         const std::string &uuid) {
  const auto it = scene.layers.find(uuid);
  if (it == scene.layers.end())
    return std::nullopt;
  return it->second.name;
}

// Generates a canonical UUID that does not collide with existing layers.
std::optional<std::string> GenerateLayerUuid(const MvrScene &scene) {
  for (int i = 0; i < 32; ++i) {
    const std::string uuid = CanonicalizeUuid(GenerateUuid());
    if (!uuid.empty() && scene.layers.find(uuid) == scene.layers.end())
      return uuid;
  }
  return std::nullopt;
}

// Renames all object layer references that belong to one unique layer name.
void RenameObjectLayers(MvrScene &scene, const std::string &oldName,
                        const std::string &newName) {
  auto rename = [&](auto &container) {
    for (auto &[uuid, object] : container) {
      (void)uuid;
      if (object.layer == oldName)
        object.layer = newName;
    }
  };
  rename(scene.fixtures);
  rename(scene.trusses);
  rename(scene.supports);
  rename(scene.sceneObjects);
  rename(scene.groupObjects);
}

// Deletes all objects assigned to the given layer name and cleans selections.
void DeleteLayerContents(ConfigManager &config, const std::string &name) {
  MvrScene &scene = config.GetScene();
  auto eraseLayer = [&](auto &container) {
    for (auto it = container.begin(); it != container.end();) {
      if (it->second.layer == name)
        it = container.erase(it);
      else
        ++it;
    }
  };
  eraseLayer(scene.fixtures);
  eraseLayer(scene.trusses);
  eraseLayer(scene.supports);
  eraseLayer(scene.sceneObjects);
  eraseLayer(scene.groupObjects);

  auto keepFixtures = config.GetSelectedFixtures();
  keepFixtures.erase(std::remove_if(keepFixtures.begin(), keepFixtures.end(),
                                    [&](const std::string &uuid) {
                                      return scene.fixtures.count(uuid) == 0;
                                    }),
                     keepFixtures.end());
  config.SetSelectedFixtures(keepFixtures);
  auto keepTrusses = config.GetSelectedTrusses();
  keepTrusses.erase(std::remove_if(keepTrusses.begin(), keepTrusses.end(),
                                   [&](const std::string &uuid) {
                                     return scene.trusses.count(uuid) == 0;
                                   }),
                    keepTrusses.end());
  config.SetSelectedTrusses(keepTrusses);
  auto keepSupports = config.GetSelectedSupports();
  keepSupports.erase(std::remove_if(keepSupports.begin(), keepSupports.end(),
                                    [&](const std::string &uuid) {
                                      return scene.supports.count(uuid) == 0;
                                    }),
                     keepSupports.end());
  config.SetSelectedSupports(keepSupports);
  auto keepObjects = config.GetSelectedSceneObjects();
  keepObjects.erase(std::remove_if(keepObjects.begin(), keepObjects.end(),
                                   [&](const std::string &uuid) {
                                     return scene.sceneObjects.count(uuid) == 0;
                                   }),
                    keepObjects.end());
  config.SetSelectedSceneObjects(keepObjects);
}

} // namespace

// Returns every canonical layer, including names inferred from all object types.
std::vector<LayerEntry> EnumerateLayers(const MvrScene &scene) {
  std::map<std::string, LayerEntry> byName;
  byName[DEFAULT_LAYER_NAME] = {"layer_default", DEFAULT_LAYER_NAME, {}};
  for (const auto &[uuid, layer] : scene.layers)
    byName[layer.name] = {uuid, layer.name, layer.color};
  auto collect = [&](const std::string &name) {
    if (!name.empty() && byName.find(name) == byName.end())
      byName[name] = {{}, name, {}};
  };
  for (const auto &[uuid, object] : scene.fixtures) { (void)uuid; collect(object.layer); }
  for (const auto &[uuid, object] : scene.trusses) { (void)uuid; collect(object.layer); }
  for (const auto &[uuid, object] : scene.supports) { (void)uuid; collect(object.layer); }
  for (const auto &[uuid, object] : scene.sceneObjects) { (void)uuid; collect(object.layer); }
  for (const auto &[uuid, object] : scene.groupObjects) { (void)uuid; collect(object.layer); }
  std::vector<LayerEntry> out;
  for (const auto &[name, entry] : byName)
    out.push_back(entry);
  return out;
}

// Creates a validated layer through the undoable domain pipeline.
LayerResult CreateLayer(ConfigManager &config, const std::string &name) {
  std::string normalized;
  LayerResult result = NormalizeLayerName(name, normalized);
  if (result.status != LayerStatus::Success) { LogLayerAction("create", result); return result; }
  MvrScene &scene = config.GetScene();
  for (const auto &[uuid, layer] : scene.layers) {
    (void)uuid;
    if (layer.name == normalized)
      return {LayerStatus::DuplicateName, "Layer name already exists"};
  }
  const auto uuid = GenerateLayerUuid(scene);
  if (!uuid)
    return {LayerStatus::InvalidUuid, "Could not generate a unique layer UUID"};
  config.PushUndoState("add layer");
  scene.layers[*uuid] = Layer{*uuid, normalized, {}, {}};
  config.SetCurrentLayer(normalized);
  result = {LayerStatus::Success, {}, *uuid, normalized, {}};
  result.changes.layerStructureChanged = true;
  result.changes.currentLayerChanged = true;
  LogLayerAction("create", result);
  return result;
}

// Renames one existing layer by UUID and updates object references atomically.
LayerResult RenameLayer(ConfigManager &config, const std::string &uuid,
                        const std::string &newName) {
  MvrScene &scene = config.GetScene();
  const auto oldName = FindLayerName(scene, uuid);
  if (!oldName)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (*oldName == DEFAULT_LAYER_NAME)
    return {LayerStatus::ValidationFailure, "Default layer cannot be renamed", uuid};
  std::string normalized;
  LayerResult result = NormalizeLayerName(newName, normalized);
  if (result.status != LayerStatus::Success)
    return result;
  if (normalized == *oldName)
    return {LayerStatus::NoChange, {}, uuid, normalized};
  for (const auto &[otherUuid, layer] : scene.layers) {
    if (otherUuid != uuid && layer.name == normalized)
      return {LayerStatus::DuplicateName, "Layer name already exists", uuid,
              normalized};
  }
  const MvrScene before = scene;
  config.PushUndoState("rename layer");
  scene.layers[uuid].name = normalized;
  RenameObjectLayers(scene, *oldName, normalized);
  auto hidden = config.GetHiddenLayers();
  if (hidden.erase(*oldName))
    hidden.insert(normalized);
  config.SetHiddenLayers(hidden);
  if (config.GetCurrentLayer() == *oldName)
    config.SetCurrentLayer(normalized);
  result = ValidateSceneLayers(scene);
  if (result.status != LayerStatus::Success) {
    scene = before;
    return result;
  }
  result = {LayerStatus::Success, {}, uuid, normalized, {}};
  result.changes.layerStructureChanged = true;
  result.changes.sceneContentChanged = true;
  LogLayerAction("rename", result);
  return result;
}

// Deletes one existing layer by UUID together with its assigned objects.
LayerResult DeleteLayer(ConfigManager &config, const std::string &uuid) {
  MvrScene &scene = config.GetScene();
  const auto name = FindLayerName(scene, uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (*name == DEFAULT_LAYER_NAME)
    return {LayerStatus::ValidationFailure, "Default layer cannot be deleted", uuid};
  config.PushUndoState("delete layer");
  DeleteLayerContents(config, *name);
  scene.layers.erase(uuid);
  auto hidden = config.GetHiddenLayers();
  hidden.erase(*name);
  config.SetHiddenLayers(hidden);
  if (config.GetCurrentLayer() == *name)
    config.SetCurrentLayer(DEFAULT_LAYER_NAME);
  LayerResult result{LayerStatus::Success, {}, uuid, *name, {}};
  result.changes.layerStructureChanged = true;
  result.changes.sceneContentChanged = true;
  result.changes.selectionChanged = true;
  LogLayerAction("delete", result);
  return result;
}

// Changes an existing layer color by UUID without implicit layer creation.
LayerResult SetLayerColor(ConfigManager &config, const std::string &uuid,
                          const std::string &color) {
  if (!IsCanonicalColor(color))
    return {LayerStatus::InvalidColor, "Layer color must be #RRGGBB", uuid};
  MvrScene &scene = config.GetScene();
  const auto it = scene.layers.find(uuid);
  if (it == scene.layers.end())
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (it->second.color == color)
    return {LayerStatus::NoChange, {}, uuid, it->second.name};
  config.PushUndoState("change layer color");
  it->second.color = color;
  LayerResult result{LayerStatus::Success, {}, uuid, it->second.name, {}};
  result.changes.layerAppearanceChanged = true;
  LogLayerAction("color", result);
  return result;
}

// Changes layer visibility by UUID using the current name-backed compatibility state.
LayerResult SetLayerVisibility(ConfigManager &config, const std::string &uuid,
                               bool visible) {
  const auto name = FindLayerName(config.GetScene(), uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  auto hidden = config.GetHiddenLayers();
  const bool wasVisible = hidden.count(*name) == 0;
  if (wasVisible == visible)
    return {LayerStatus::NoChange, {}, uuid, *name};
  config.PushUndoState("change layer visibility");
  if (visible)
    hidden.erase(*name);
  else
    hidden.insert(*name);
  config.SetHiddenLayers(hidden);
  LayerResult result{LayerStatus::Success, {}, uuid, *name, {}};
  result.changes.layerVisibilityChanged = true;
  return result;
}

// Sets the current layer by UUID using centralized name compatibility mapping.
LayerResult SetCurrentLayer(ConfigManager &config, const std::string &uuid) {
  const auto name = FindLayerName(config.GetScene(), uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (config.GetCurrentLayer() == *name)
    return {LayerStatus::NoChange, {}, uuid, *name};
  config.SetCurrentLayer(*name);
  LayerResult result{LayerStatus::Success, {}, uuid, *name, {}};
  result.changes.currentLayerChanged = true;
  return result;
}

// Validates layer UUIDs, names, colors, and duplicate canonical names.
LayerResult ValidateSceneLayers(const MvrScene &scene) {
  std::set<std::string> names;
  std::set<std::string> uuids;
  for (const auto &[uuid, layer] : scene.layers) {
    if (CanonicalizeUuid(uuid).empty() && uuid != "layer_default")
      return {LayerStatus::InvalidUuid, "Layer UUID is malformed", uuid,
              layer.name};
    if (!uuids.insert(uuid).second)
      return {LayerStatus::InvalidUuid, "Layer UUID is duplicated", uuid,
              layer.name};
    if (!IsValidUtf8(layer.name))
      return {LayerStatus::InvalidUtf8, "Layer name is not valid UTF-8", uuid};
    if (!IsCanonicalColor(layer.color))
      return {LayerStatus::InvalidColor, "Layer color is invalid", uuid,
              layer.name};
    if (!names.insert(TrimLayerName(layer.name)).second)
      return {LayerStatus::DuplicateName, "Layer name is duplicated", uuid,
              layer.name};
  }
  return {};
}

// Repairs known legacy Windows-1252 layer-name corruption without merging layers.
LayerResult ReconcileLegacyLayers(MvrScene &scene) {
  bool repaired = false;
  std::set<std::string> names;
  for (auto &[uuid, layer] : scene.layers) {
    if (!IsValidUtf8(layer.name)) {
      const auto repairedName = RepairWindows1252AsUtf8(layer.name);
      if (!repairedName)
        return {LayerStatus::InvalidUtf8, "Layer name cannot be repaired", uuid};
      layer.name = *repairedName;
      repaired = true;
    }
    std::string unique = layer.name;
    for (int suffix = 2; names.count(unique) > 0; ++suffix)
      unique = layer.name + " (Recovered " + std::to_string(suffix) + ")";
    if (unique != layer.name) {
      RenameObjectLayers(scene, layer.name, unique);
      layer.name = unique;
      repaired = true;
    }
    names.insert(layer.name);
  }
  return {repaired ? LayerStatus::Success : LayerStatus::NoChange,
          repaired ? "Legacy layer data repaired" : "No legacy repairs needed"};
}

// Converts a layer status to a user-facing English diagnostic.
std::string StatusMessage(LayerStatus status) {
  switch (status) {
  case LayerStatus::Success: return "Layer operation succeeded.";
  case LayerStatus::NoChange: return "Layer operation made no changes.";
  case LayerStatus::ValidationFailure: return "Layer data is not valid.";
  case LayerStatus::NotFound: return "Layer was not found.";
  case LayerStatus::DuplicateName: return "Layer name already exists.";
  case LayerStatus::InvalidUtf8: return "Layer text is not valid UTF-8.";
  case LayerStatus::InvalidUuid: return "Layer UUID is not valid.";
  case LayerStatus::InvalidColor: return "Layer color is not valid.";
  }
  return "Layer operation failed.";
}

} // namespace layerdomain
