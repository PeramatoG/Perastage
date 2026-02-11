#pragma once

#include "../viewer3d_types.h"

#include <string>
#include <unordered_set>

class Viewer3DController;

class VisibilitySystem {
public:
  explicit VisibilitySystem(Viewer3DController &controller) : m_controller(controller) {}

  bool EnsureBoundsComputed(const std::string &uuid, ViewerItemType type,
                            const std::unordered_set<std::string> &hiddenLayers);
  bool TryBuildVisibleSet(const ViewerViewFrustumSnapshot &frustum,
                          bool useFrustumCulling, float minPixels,
                          const ViewerVisibleSet &layerVisibleCandidates,
                          ViewerVisibleSet &out) const;
  void RebuildVisibleSetCache();

private:
  Viewer3DController &m_controller;
};
