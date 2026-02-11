#include "visibilitysystem.h"
#include "../viewer3dcontroller.h"

bool VisibilitySystem::EnsureBoundsComputed(
    const std::string &uuid, ViewerItemType type,
    const std::unordered_set<std::string> &hiddenLayers) {
  return m_controller.EnsureBoundsComputedImpl(uuid, type, hiddenLayers);
}

bool VisibilitySystem::TryBuildVisibleSet(
    const ViewerViewFrustumSnapshot &frustum,
    bool useFrustumCulling, float minPixels,
    const ViewerVisibleSet &layerVisibleCandidates,
    ViewerVisibleSet &out) const {
  return m_controller.TryBuildVisibleSetImpl(frustum, useFrustumCulling,
                                             minPixels, layerVisibleCandidates,
                                             out);
}

void VisibilitySystem::RebuildVisibleSetCache() {
  m_controller.RebuildVisibleSetCacheImpl();
}
