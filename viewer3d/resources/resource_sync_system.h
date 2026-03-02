#pragma once

#include "mesh.h"
#include "gdtfloader.h"
#include "fixture.h"
#include "sceneobject.h"
#include "truss.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct FixtureSceneNode {
  std::string stableName;
  int parentIndex = -1;
  Matrix localTransform = Matrix{};
  Matrix worldTransform = Matrix{};
  bool isAxis = false;
  bool isEmitter = false;
};

struct FixtureAnchorRegistryEntry {
  std::string fixtureRootName;
  std::vector<FixtureSceneNode> axisNodes;
  std::vector<FixtureSceneNode> emitterNodes;
};

struct ResourceSyncState {
  struct PathResolutionEntry {
    std::string resolvedPath;
    bool attempted = false;
  };

  std::unordered_map<std::string, Mesh> loadedMeshes;
  std::unordered_map<std::string, std::vector<GdtfObject>> loadedGdtf;
  std::unordered_map<std::string, GdtfGeometryTree> loadedGdtfGeometryTrees;
  std::unordered_map<std::string, std::string> failedGdtfReasons;
  std::unordered_map<std::string, size_t> failedGdtfAttemptCounts;
  std::unordered_map<std::string, size_t> reportedGdtfFailureCounts;
  std::unordered_map<std::string, std::string> reportedGdtfFailureReasons;
  std::unordered_map<std::string, PathResolutionEntry> resolvedGdtfSpecs;
  std::unordered_map<std::string, PathResolutionEntry> resolvedModelRefs;
  std::unordered_map<std::string, std::vector<FixtureSceneNode>> fixtureNodeRegistry;
  std::unordered_map<std::string, FixtureAnchorRegistryEntry> fixtureAnchorRegistry;
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
