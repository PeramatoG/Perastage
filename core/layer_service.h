#pragma once

#include "configservices.h"
#include "layer_validation.h"

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
LayerResult ReconcileLegacyLayers(MvrScene &scene);

} // namespace layerdomain
