#pragma once

#include "configservices.h"

#include <cstddef>
#include <string>
#include <vector>

class ConfigManager;

namespace layerdomain {

constexpr size_t kMaxLayerNameBytes = 256;

struct LayerEntry {
  std::string uuid;
  std::string name;
  std::string color;
};

enum class LayerStatus {
  Success,
  NoChange,
  ValidationFailure,
  NotFound,
  DuplicateName,
  InvalidUtf8,
  InvalidUuid,
  InvalidColor
};

struct LayerChangeSet {
  bool layerStructureChanged = false;
  bool layerAppearanceChanged = false;
  bool layerVisibilityChanged = false;
  bool currentLayerChanged = false;
  bool sceneContentChanged = false;
  bool selectionChanged = false;
};

struct LayerResult {
  LayerStatus status = LayerStatus::Success;
  std::string message;
  std::string layerUuid;
  std::string layerName;
  LayerChangeSet changes;
};

std::vector<LayerEntry> EnumerateLayers(const MvrScene &scene);
LayerResult CreateLayer(ConfigManager &config, const std::string &name);
LayerResult RenameLayer(ConfigManager &config, const std::string &uuid,
                        const std::string &newName);
LayerResult DeleteLayer(ConfigManager &config, const std::string &uuid);
LayerResult SetLayerColor(ConfigManager &config, const std::string &uuid,
                          const std::string &color);
LayerResult SetLayerVisibility(ConfigManager &config, const std::string &uuid,
                               bool visible);
LayerResult SetCurrentLayer(ConfigManager &config, const std::string &uuid);
LayerResult ValidateSceneLayers(const MvrScene &scene);
LayerResult ReconcileLegacyLayers(MvrScene &scene);
std::string StatusMessage(LayerStatus status);

} // namespace layerdomain
