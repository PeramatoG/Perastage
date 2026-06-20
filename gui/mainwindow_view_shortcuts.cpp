#include "mainwindow.h"

#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"
#include "selection_movement_settings.h"
#include "guiconfigservices.h"
#include "magnet_snap.h"

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
  SyncAxisConstraintToolToggleState();
  SyncLeftDragMoveToolToggleState();
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_Magnet,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          magnet_snap::kMagnetEnabledConfigKey) == "1");
  layoutViewsToolBar->Refresh();
}

// Synchronizes the axis-constrained movement toolbar toggle with project settings.
void MainWindow::SyncAxisConstraintToolToggleState() {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_AxisConstraint,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kAxisConstrainedMovementConfigKey) !=
          "0");
}

// Synchronizes the left-click selection dragging toolbar toggle with project settings.
void MainWindow::SyncLeftDragMoveToolToggleState() {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_LeftDragMove,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kLeftDragSelectionMovementConfigKey) ==
          "1");
}

// Switches the viewport interaction back to standard selection mode.
void MainWindow::OnViewportSelectTool(wxCommandEvent &WXUNUSED(event)) {
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(false);
  if (viewportPanel)
    viewportPanel->SetMeasureToolEnabled(false);
  SyncViewportToolToggleState(false);
}

// Enables or disables the viewport measure tool and syncs toolbar toggle state.
void MainWindow::OnViewportMeasureTool(wxCommandEvent &WXUNUSED(event)) {
  const bool measureEnabled =
      (viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled()) ||
      (viewportPanel && viewportPanel->IsMeasureToolEnabled());
  const bool enableMeasure = !measureEnabled;
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(enableMeasure);
  if (viewportPanel)
    viewportPanel->SetMeasureToolEnabled(enableMeasure);
  SyncViewportToolToggleState(enableMeasure);
}

// Toggles project-level axis-constrained selection movement.
void MainWindow::OnViewportAxisConstraint(wxCommandEvent &WXUNUSED(event)) {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool axisConstraintEnabled =
      preferences.GetValue(
          selection_movement_settings::kAxisConstrainedMovementConfigKey) !=
      "0";
  preferences.SetValue(
      selection_movement_settings::kAxisConstrainedMovementConfigKey,
      axisConstraintEnabled ? "0" : "1");
  SyncAxisConstraintToolToggleState();
}

// Toggles project-level left-click selection dragging.
void MainWindow::OnViewportLeftDragMove(wxCommandEvent &WXUNUSED(event)) {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool enabled =
      preferences.GetValue(
          selection_movement_settings::kLeftDragSelectionMovementConfigKey) ==
      "1";
  preferences.SetValue(
      selection_movement_settings::kLeftDragSelectionMovementConfigKey,
      enabled ? "0" : "1");
  SyncLeftDragMoveToolToggleState();
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

// Toggles Magnet snapping for 2D selection dragging and persists the preference.
void MainWindow::OnViewportMagnet(wxCommandEvent &WXUNUSED(event)) {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool enabled =
      preferences.GetValue(magnet_snap::kMagnetEnabledConfigKey) != "1";
  if (viewport2DPanel)
    viewport2DPanel->SetMagnetEnabled(enabled);
  if (viewportPanel)
    viewportPanel->SetMagnetEnabled(enabled);
  SyncViewportToolToggleState(
      (viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled()) ||
      (viewportPanel && viewportPanel->IsMeasureToolEnabled()));
}
