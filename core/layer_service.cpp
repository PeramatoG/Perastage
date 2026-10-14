#include "layer_identity.h"
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
#include <vector>
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

// Returns the presentation label for an existing UUID.
std::optional<std::string> FindLayerLabel(const MvrScene &scene,
                                         const std::string &uuid) {
  const auto it = scene.layers.find(uuid);
  if (it == scene.layers.end())
    return std::nullopt;
  return PresentationName(scene, uuid);
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

// Deletes all objects assigned to the given layer UUID and cleans selections.
void DeleteLayerContents(ConfigManager &config, const std::string &layerUuid) {
  MvrScene &scene = config.GetScene();
  auto eraseLayer = [&](auto &container) {
    for (auto it = container.begin(); it != container.end();) {
      if (it->second.layerUuid == layerUuid)
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
  MvrScene snapshot = scene;
  MigrateLayerMembership(snapshot);
  EnsureDefaultLayer(snapshot);
  std::vector<LayerEntry> out;
  for (const auto &[uuid, layer] : snapshot.layers)
    out.push_back({uuid, PresentationName(snapshot, uuid), layer.color});
  std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    return a.name < b.name;
  });
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
  config.SetCurrentLayer(*uuid);
  result = {LayerStatus::Success, {}, *uuid, normalized, {}};
  result.changes.layerStructureChanged = true;
  result.changes.currentLayerChanged = true;
  LogLayerAction("create", result);
  return result;
}

// Renames presentation metadata; persistent object ownership never changes.
LayerResult RenameLayer(ConfigManager &config, const std::string &uuid,
                        const std::string &newName) {
  MvrScene &scene = config.GetScene();
  const auto oldName = scene.layers.contains(uuid)
      ? std::optional<std::string>(scene.layers.at(uuid).name) : std::nullopt;
  if (!oldName)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (uuid == DefaultLayerUuid())
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
  MvrScene candidate = scene;
  MigrateLayerMembership(candidate);
  candidate.layers[uuid].name = normalized;
  RefreshLayerLabels(candidate);
  result = ValidateSceneLayers(candidate);
  if (result.status != LayerStatus::Success)
    return result;
  config.PushUndoState("rename layer");
  scene = std::move(candidate);
  result = {LayerStatus::Success, {}, uuid, normalized, {}};
  result.changes.layerStructureChanged = true;
  result.changes.sceneContentChanged = true;
  LogLayerAction("rename", result);
  return result;
}

// Deletes one existing layer by UUID together with its assigned objects.
LayerResult DeleteLayer(ConfigManager &config, const std::string &uuid) {
  MvrScene &scene = config.GetScene();
  const auto name = FindLayerLabel(scene, uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (uuid == DefaultLayerUuid())
    return {LayerStatus::ValidationFailure, "Default layer cannot be deleted", uuid};
  config.PushUndoState("delete layer");
  MigrateLayerMembership(scene);
  DeleteLayerContents(config, uuid);
  scene.layers.erase(uuid);
  RefreshLayerLabels(scene);
  auto hidden = config.GetHiddenLayers();
  hidden.erase(uuid);
  config.SetHiddenLayers(hidden);
  if (config.GetCurrentLayer() == uuid)
    config.SetCurrentLayer(DefaultLayerUuid());
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

// Changes layer visibility by UUID in the UUID-backed workspace state.
LayerResult SetLayerVisibility(ConfigManager &config, const std::string &uuid,
                               bool visible) {
  const auto name = FindLayerLabel(config.GetScene(), uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  auto hidden = config.GetHiddenLayers();
  const bool wasVisible = hidden.count(uuid) == 0;
  if (wasVisible == visible)
    return {LayerStatus::NoChange, {}, uuid, *name};
  config.PushUndoState("change layer visibility");
  if (visible)
    hidden.erase(uuid);
  else
    hidden.insert(uuid);
  config.SetHiddenLayers(hidden);
  LayerResult result{LayerStatus::Success, {}, uuid, *name, {}};
  result.changes.layerVisibilityChanged = true;
  return result;
}

// Sets the current layer by UUID.
LayerResult SetCurrentLayer(ConfigManager &config, const std::string &uuid) {
  const auto name = FindLayerLabel(config.GetScene(), uuid);
  if (!name)
    return {LayerStatus::NotFound, "Layer UUID was not found", uuid};
  if (config.GetCurrentLayer() == uuid)
    return {LayerStatus::NoChange, {}, uuid, *name};
  config.SetCurrentLayer(uuid);
  LayerResult result{LayerStatus::Success, {}, uuid, *name, {}};
  result.changes.currentLayerChanged = true;
  return result;
}

// Repairs known legacy Windows-1252 layer-name corruption without merging valid layers.
LayerResult ReconcileLegacyLayers(MvrScene &scene) {
  struct RepairCandidate {
    std::string uuid;
    std::string originalName;
    std::string repairedName;
  };

  std::set<std::string> reservedNames;
  std::vector<RepairCandidate> repairs;
  for (const auto &[uuid, layer] : scene.layers) {
    if (IsValidUtf8(layer.name)) {
      reservedNames.insert(TrimLayerName(layer.name));
      continue;
    }
    const auto repairedName = RepairWindows1252AsUtf8(layer.name);
    if (!repairedName)
      return {LayerStatus::InvalidUtf8, "Layer name cannot be repaired", uuid};
    repairs.push_back({uuid, layer.name, TrimLayerName(*repairedName)});
  }

  if (repairs.empty())
    return {LayerStatus::NoChange, "No legacy repairs needed"};

  MigrateLayerMembership(scene);
  bool repaired = false;
  int recoveredSuffix = 2;
  for (const auto &repair : repairs) {
    auto layerIt = scene.layers.find(repair.uuid);
    if (layerIt == scene.layers.end())
      continue;

    std::string finalName = repair.repairedName;
    if (finalName.empty())
      return {LayerStatus::ValidationFailure, "Layer name is empty", repair.uuid};
    if (reservedNames.count(finalName) > 0) {
      do {
        finalName = repair.repairedName + " (Recovered " +
                    std::to_string(recoveredSuffix++) + ")";
      } while (reservedNames.count(finalName) > 0);
    }


    layerIt->second.name = finalName;
    reservedNames.insert(finalName);
    repaired = true;
  }

  RefreshLayerLabels(scene);
  const auto validation = ValidateSceneLayers(scene);
  if (validation.status != LayerStatus::Success)
    return validation;
  return {repaired ? LayerStatus::Success : LayerStatus::NoChange,
          repaired ? "Legacy layer data repaired" : "No legacy repairs needed"};
}

} // namespace layerdomain
