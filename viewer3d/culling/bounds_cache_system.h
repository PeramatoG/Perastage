#pragma once

#include "resource_sync_system.h"
#include "viewer3d_types.h"
#include "scenedatamanager.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class BoundsCacheSystem {
public:
  struct Context {
    ResourceSyncState &resourceSyncState;
    std::unordered_map<std::string, Viewer3DBoundingBox> &modelBounds;
    std::unordered_map<std::string, Viewer3DBoundingBox> &fixtureBounds;
    std::unordered_map<std::string, Viewer3DBoundingBox> &trussBounds;
    std::unordered_map<std::string, Viewer3DBoundingBox> &objectBounds;
    std::unordered_set<std::string> &boundsHiddenLayers;
    size_t sceneVersion;
    size_t &cachedVersion;
    bool &sceneChangedDirty;
    bool &assetsChangedDirty;
    bool &visibilityChangedDirty;
    std::mutex &sortedListsMutex;
    bool &sortedListsDirty;
  };

  static void RebuildIfDirty(
      Context &context,
      const std::unordered_set<std::string> &hiddenLayers,
      const std::unordered_map<std::string, Truss> &trusses,
      const std::unordered_map<std::string, SceneObject> &objects,
      const std::unordered_map<std::string, Fixture> &fixtures);
};
