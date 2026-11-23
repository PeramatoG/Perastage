#include "viewer2dpanel.h"

#include "performance_metrics.h"

// Interactive plans share the controller's bounded loading contract. Captures
// drain resources synchronously to preserve complete exported symbols/plans.
void Viewer2DPanel::SynchronizeRenderResources(bool capture) {
  if (capture) {
    m_controller.UpdateResourcesIfDirty();
    return;
  }
  if (!IsEnabled()) return;
  m_controller.UpdateFrameStateLightweight();
  if (m_controller.ConsumeResourceSyncPending()) {
    performance::Scope metric(performance::Work::View2DResourceSync,
                              performance::viewer2DFrameActive);
    m_controller.UpdateResourcesIfDirty(true);
  }
  if (m_controller.IsResourceSyncPending()) RequestRepaint();
}
