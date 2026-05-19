#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>

class ConfigManager;

// Returns the current hidden-layer set from configuration cache state.
std::unordered_set<std::string> SnapshotHiddenLayers(const ConfigManager &cfg);

// Returns whether the provided layer remains visible against the cached hidden set.
bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                         const std::string &layer);

// Returns a normalized model-cache key that is stable across equivalent paths.
std::string NormalizeModelKey(const std::string &path);

// Resets the visible-set layer-candidate scene marker to force cache rebuilds.
void InvalidateVisibleSetLayerCandidateCacheOwnership(size_t &sceneVersionMarker);
