#pragma once

#include "mesh.h"
#include "gdtfloader.h"
#include "models/fixture.h"
#include "models/sceneobject.h"
#include "models/truss.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ResourceSyncState {
  struct PathResolutionEntry {
    std::string resolvedPath;
    bool attempted = false;
  };

  std::unordered_map<std::string, Mesh> loadedMeshes;
  std::unordered_map<std::string, std::vector<GdtfObject>> loadedGdtf;
  std::unordered_map<std::string, std::string> failedGdtfReasons;
  std::unordered_map<std::string, size_t> reportedGdtfFailureCounts;
  std::unordered_map<std::string, std::string> reportedGdtfFailureReasons;
  std::unordered_map<std::string, PathResolutionEntry> resolvedGdtfSpecs;
  std::unordered_map<std::string, PathResolutionEntry> resolvedModelRefs;
  std::string lastSceneBasePath;
  size_t lastSceneSignature = 0;
  bool hasSceneSignature = false;
};

struct ResourceSyncCallbacks {
  std::function<void(Mesh &)> setupMeshBuffers;
  std::function<void(Mesh &)> releaseMeshBuffers;
  std::function<void(const std::string &)> appendConsoleMessage;
};

struct ResourceSyncResult {
  bool sceneChanged = false;
  bool assetsChanged = false;
  size_t sceneSignature = 0;
  bool hasSceneSignature = false;
};

class ResourceSyncSystem {
public:
  static ResourceSyncResult
  Sync(const std::string &basePath,
       const std::vector<const std::pair<const std::string, Truss> *> &visibleTrusses,
       const std::vector<const std::pair<const std::string, SceneObject> *> &visibleObjects,
       const std::vector<const std::pair<const std::string, Fixture> *> &visibleFixtures,
       ResourceSyncState &state, const ResourceSyncCallbacks &callbacks);
};
