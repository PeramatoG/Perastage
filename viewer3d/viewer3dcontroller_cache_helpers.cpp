#include "viewer3dcontroller_cache_helpers.h"

#include <algorithm>
#include <filesystem>

#include "configmanager.h"
#include "projectutils.h"

namespace fs = std::filesystem;

// Captures and returns hidden layers from the current configuration state.
std::unordered_set<std::string> ControllerSnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
}

// Returns true when a layer is not currently marked as hidden in the cache set.
bool ControllerIsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                         const std::string &layer) {
  if (layer.empty())
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

// Converts separators so paths use the preferred separator on the current platform.
static std::string NormalizePath(const std::string &path) {
  std::string normalizedPath = path;
  const char separator = static_cast<char>(fs::path::preferred_separator);
  std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', separator);
  return normalizedPath;
}

// Returns a normalized, lexically-stable model key for cache lookup consistency.
std::string ControllerNormalizeModelKey(const std::string &path) {
  if (path.empty())
    return {};
  fs::path normalizedPath(path);
  normalizedPath = normalizedPath.lexically_normal();
  return NormalizePath(normalizedPath.string());
}

// Invalidates the layer-candidate ownership marker used by visible-set caching.
void ControllerInvalidateVisibleSetLayerCandidateCacheOwnership(
    size_t &sceneVersionMarker) {
  sceneVersionMarker = static_cast<size_t>(-1);
}
