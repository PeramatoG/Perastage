#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>

class ConfigManager;

// Returns the current hidden-layer set from configuration cache state.
std::unordered_set<std::string> ControllerSnapshotHiddenLayers(const ConfigManager &cfg);

// Returns whether the provided layer remains visible against the cached hidden set.
bool ControllerIsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                         const std::string &layer);

// Returns a normalized model-cache key that is stable across equivalent paths.
std::string ControllerNormalizeModelKey(const std::string &path);

// Resets the visible-set layer-candidate scene marker to force cache rebuilds.
void ControllerInvalidateVisibleSetLayerCandidateCacheOwnership(size_t &sceneVersionMarker);
