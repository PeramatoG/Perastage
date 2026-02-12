#pragma once

#include "resource_sync_system.h"
#include "scenedatamanager.h"
#include "viewer3d_types.h"
#include <array>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class IVisibilityContext {
public:
  using ItemType = Viewer3DItemType;
  using VisibleSet = Viewer3DVisibleSet;
  using ViewFrustumSnapshot = Viewer3DViewFrustumSnapshot;
  using BoundingBox = Viewer3DBoundingBox;

  virtual ~IVisibilityContext() = default;

  virtual ResourceSyncState &GetResourceSyncState() = 0;
  virtual std::unordered_map<std::string, BoundingBox> &GetModelBounds() = 0;
  virtual std::unordered_map<std::string, BoundingBox> &GetFixtureBounds() = 0;
  virtual std::unordered_map<std::string, BoundingBox> &GetTrussBounds() = 0;
  virtual std::unordered_map<std::string, BoundingBox> &GetObjectBounds() = 0;

  virtual size_t GetSceneVersion() const = 0;
  virtual const std::vector<const std::pair<const std::string, Fixture> *> &
  GetSortedFixtures() const = 0;
  virtual const std::vector<const std::pair<const std::string, Truss> *> &
  GetSortedTrusses() const = 0;
  virtual const std::vector<const std::pair<const std::string, SceneObject> *> &
  GetSortedObjects() const = 0;
  virtual std::mutex &GetSortedListsMutex() const = 0;

  virtual VisibleSet &GetCachedVisibleSet() const = 0;
  virtual VisibleSet &GetCachedLayerVisibleCandidates() const = 0;
  virtual size_t &GetLayerVisibleCandidatesSceneVersion() const = 0;
  virtual std::unordered_set<std::string> &
  GetLayerVisibleCandidatesHiddenLayers() const = 0;
  virtual size_t &GetLayerVisibleCandidatesRevision() const = 0;
  virtual size_t &GetVisibleSetLayerCandidatesRevision() const = 0;
  virtual bool &GetVisibleSetFrustumCulling() const = 0;
  virtual float &GetVisibleSetMinPixels() const = 0;
  virtual std::array<int, 4> &GetVisibleSetViewport() const = 0;
  virtual std::array<double, 16> &GetVisibleSetModel() const = 0;
  virtual std::array<double, 16> &GetVisibleSetProjection() const = 0;
};
