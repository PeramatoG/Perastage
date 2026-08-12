#pragma once

#include <string>
#include <vector>

#include "gdtf_geometry_types.h"
#include "mesh.h"
#include "mesh_geometry_conventions.h"

namespace viewer3d::resources {

struct MeshProcessingOptions {
  bool enableMeshOptimization = true;
  bool enableDiskCache = true;
  bool applyThreeDsObjectTransforms = viewer3d::kApplyThreeDsObjectTransforms;
};

void OptimizeMeshForRuntime(Mesh &mesh);
void OptimizeGdtfObjectsForRuntime(std::vector<GdtfObject> &objects);

bool TryLoadMeshCache(const std::string &sourcePath, Mesh &outMesh);
bool TrySaveMeshCache(const std::string &sourcePath, const Mesh &mesh);

bool TryLoadGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      std::vector<GdtfObject> &outObjects);
bool TrySaveGdtfCache(const std::string &gdtfPath,
                      const std::string &modeName,
                      const std::vector<GdtfObject> &objects);

} // namespace viewer3d::resources
