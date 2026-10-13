#include "viewer2dpanel.h"

// Interactive plans share the controller's bounded loading contract. Captures
// drain resources synchronously to preserve complete exported symbols/plans.
void Viewer2DPanel::SynchronizeRenderResources(bool capture) {
  if (capture) {
    m_controller.UpdateResourcesIfDirty();
    return;
  }
  m_controller.UpdateFrameStateLightweight();
  if (m_controller.ConsumeResourceSyncPending())
    m_controller.UpdateResourcesIfDirty(true);
  if (m_controller.IsResourceSyncPending()) RequestRepaint();
}
