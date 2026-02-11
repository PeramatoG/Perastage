#include "visibilitysystem.h"

bool VisibilitySystem::EnsureBoundsComputed(
    const std::string &uuid, Viewer3DController::ItemType type,
    const std::unordered_set<std::string> &hiddenLayers) {
  return m_controller.EnsureBoundsComputedImpl(uuid, type, hiddenLayers);
}

bool VisibilitySystem::TryBuildVisibleSet(
    const Viewer3DController::ViewFrustumSnapshot &frustum,
    bool useFrustumCulling, float minPixels,
    const Viewer3DController::VisibleSet &layerVisibleCandidates,
    Viewer3DController::VisibleSet &out) const {
  return m_controller.TryBuildVisibleSetImpl(frustum, useFrustumCulling,
                                             minPixels, layerVisibleCandidates,
                                             out);
}

void VisibilitySystem::RebuildVisibleSetCache() {
  m_controller.RebuildVisibleSetCacheImpl();
}
