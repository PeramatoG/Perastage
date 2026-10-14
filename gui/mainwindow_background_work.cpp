#include "mainwindow.h"
#include "viewer3dpanel.h"
#include "viewer2dpanel.h"

// Automatic captures must not replace scene state while the viewport is still
// filling in, navigating, importing, or waiting for close/save confirmation.
bool MainWindow::ShouldDeferAutomaticSceneWork() const {
  if (viewportInteractionLockDepth > 0 || !IsEnabled() || mvrImportPipelineActive)
    return true;
  if (viewportPanel && viewportPanel->IsShownOnScreen() &&
      (viewportPanel->IsCameraMoving() || viewportPanel->HasPendingScenePreparation()))
    return true;
  return viewport2DPanel && viewport2DPanel->IsShownOnScreen() &&
         viewport2DPanel->HasPendingResourceLoading();
}
