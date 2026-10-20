#include "mainwindow.h"
#include "viewer2dpanel.h"
#include "layoutviewerpanel.h"

// Label publication preserves camera, selection and symbol/scene resources.
void MainWindow::RefreshAfterFixtureLabelUpdate() {
  auto *activePanel = Viewer2DPanel::Instance();
  if (!activePanel)
    activePanel = viewport2DPanel;
  if (activePanel) {
    activePanel->UpdateScene(false);
    activePanel->Refresh(false);
    activePanel->Update();
  }
  if (layoutViewerPanel)
    layoutViewerPanel->RefreshAfterFixtureLabelUpdate();
}
