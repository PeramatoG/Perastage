#include "mainwindow.h"

#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

namespace {

bool IsPaneShown(wxAuiManager *manager, const char *name) {
  if (!manager)
    return false;
  auto &pane = manager->GetPane(name);
  return pane.IsOk() && pane.IsShown();
}

bool IsChildOrSame(const wxWindow *root, const wxWindow *window) {
  if (!root || !window)
    return false;
  const wxWindow *current = window;
  while (current) {
    if (current == root)
      return true;
    current = current->GetParent();
  }
  return false;
}

} // namespace

void MainWindow::OnViewportTopView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Top);
}

void MainWindow::OnViewportFrontView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Front);
}

void MainWindow::OnViewportSideView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Side);
}

// Synchronizes Select/Measure toolbar toggle buttons with the active viewport tool mode.
void MainWindow::SyncViewportToolToggleState(bool measureEnabled) {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_SelectTool, !measureEnabled);
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_MeasureTool, measureEnabled);
  layoutViewsToolBar->Refresh();
}

// Switches the viewport interaction back to standard selection mode.
void MainWindow::OnViewportSelectTool(wxCommandEvent &WXUNUSED(event)) {
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(false);
  SyncViewportToolToggleState(false);
}

// Enables or disables the viewport measure tool and syncs toolbar toggle state.
void MainWindow::OnViewportMeasureTool(wxCommandEvent &WXUNUSED(event)) {
  const bool enableMeasure =
      !(viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled());
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(enableMeasure);
  SyncViewportToolToggleState(enableMeasure);
}


bool MainWindow::ApplyFitShortcut() {
  const wxWindow *focusedWindow = wxWindow::FindFocus();
  const bool focusIn2D = IsChildOrSame(viewport2DPanel, focusedWindow) ||
                         IsChildOrSame(viewport2DRenderPanel, focusedWindow);
  if (focusIn2D && viewport2DPanel)
    return viewport2DPanel->FitViewToScene();

  const bool focusIn3D = IsChildOrSame(viewportPanel, focusedWindow);
  if (focusIn3D && viewportPanel)
    return viewportPanel->FrameSceneToFit();

  return false;
}

void MainWindow::ApplyViewportShortcut(Viewer2DView view) {
  const bool is2DShown = IsPaneShown(auiManager, "2DViewport");
  const bool is3DShown = IsPaneShown(auiManager, "3DViewport");
  const wxWindow *focusedWindow = wxWindow::FindFocus();
  const bool focusIn2D = IsChildOrSame(viewport2DPanel, focusedWindow) ||
                         IsChildOrSame(viewport2DRenderPanel, focusedWindow);
  const bool use2DViewport =
      viewport2DPanel && is2DShown && (focusIn2D || !is3DShown);

  if (use2DViewport) {
    if (viewport2DRenderPanel)
      viewport2DRenderPanel->SetViewSelection(view);
    else if (viewport2DPanel) {
      viewport2DPanel->SetView(view);
      viewport2DPanel->UpdateScene(false);
    }
    return;
  }

  if (viewportPanel)
    viewportPanel->SetStandardView(view);
}
