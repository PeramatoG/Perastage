#pragma once

#include "ivisibilitycontext.h"

class VisibilitySystem {
public:
  explicit VisibilitySystem(IVisibilityContext &controller)
      : m_controller(controller) {}

  bool EnsureBoundsComputed(const std::string &uuid, IVisibilityContext::ItemType type,
                            const std::unordered_set<std::string> &hiddenLayers);
  bool TryBuildVisibleSet(const IVisibilityContext::ViewFrustumSnapshot &frustum,
                          bool useFrustumCulling, float minPixels,
                          const IVisibilityContext::VisibleSet &layerVisibleCandidates,
                          IVisibilityContext::VisibleSet &out) const;
  bool TryBuildLayerVisibleCandidates(
      const std::unordered_set<std::string> &hiddenLayers,
      IVisibilityContext::VisibleSet &out) const;
  const IVisibilityContext::VisibleSet &
  GetVisibleSet(const IVisibilityContext::ViewFrustumSnapshot &frustum,
                const std::unordered_set<std::string> &hiddenLayers,
                bool useFrustumCulling, float minPixels) const;
  void RebuildVisibleSetCache();

private:
  IVisibilityContext &m_controller;
};
