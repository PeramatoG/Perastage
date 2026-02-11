#pragma once

#include "../viewer3dcontroller.h"

class VisibilitySystem {
public:
  explicit VisibilitySystem(Viewer3DController &controller) : m_controller(controller) {}

  bool EnsureBoundsComputed(const std::string &uuid, Viewer3DController::ItemType type,
                            const std::unordered_set<std::string> &hiddenLayers);
  bool TryBuildVisibleSet(const Viewer3DController::ViewFrustumSnapshot &frustum,
                          bool useFrustumCulling, float minPixels,
                          const Viewer3DController::VisibleSet &layerVisibleCandidates,
                          Viewer3DController::VisibleSet &out) const;
  void RebuildVisibleSetCache();

private:
  Viewer3DController &m_controller;
};
