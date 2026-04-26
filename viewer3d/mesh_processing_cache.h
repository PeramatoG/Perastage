#pragma once

#include "mesh.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

struct MeshProcessingCacheContext {
  std::string gdtfPath;
  uint64_t gdtfContentHash = 0;
  std::filesystem::file_time_type gdtfTimestamp{};
};

// Loads an optimized mesh from disk cache when possible or processes and
// persists it when the cache is missing/stale.
bool LoadOrBuildOptimizedMesh(const std::string &modelPath,
                              const MeshProcessingCacheContext &context,
                              Mesh &mesh,
                              const std::function<bool(const std::string &, Mesh &)> &loader);

// Applies meshoptimizer simplification and post-processing stages.
void OptimizeMeshForGpu(Mesh &mesh);
