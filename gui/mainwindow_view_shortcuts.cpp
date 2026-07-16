#include "mainwindow.h"

#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"
#include "selection_movement_settings.h"
#include "guiconfigservices.h"
#include "magnet_snap.h"
#include "transform_space.h"
#include "../viewport_interaction_scope.h"

namespace {

// Returns whether the named AUI pane is currently visible.
bool IsPaneShown(wxAuiManager *manager, const char *name) {
  if (!manager)
    return false;
  auto &pane = manager->GetPane(name);
  return pane.IsOk() && pane.IsShown();
}

// Returns whether a window is the same as or contained by another window.
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

// Applies the top view shortcut to the active viewport.
void MainWindow::OnViewportTopView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Top);
}

// Applies the front view shortcut to the active viewport.
void MainWindow::OnViewportFrontView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Front);
}

// Applies the side view shortcut to the active viewport.
void MainWindow::OnViewportSideView(wxCommandEvent &WXUNUSED(event)) {
  ApplyViewportShortcut(Viewer2DView::Side);
}

// Synchronizes Select/Measure toolbar toggle buttons with the active viewport tool mode.
void MainWindow::SyncViewportToolToggleState(bool measureEnabled, Viewer2DMeasureMode mode) {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_SelectTool, !measureEnabled);
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_MeasureTool,
      measureEnabled && mode == Viewer2DMeasureMode::CenterToCenter);
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_GapMeasureTool,
      measureEnabled && mode == Viewer2DMeasureMode::EdgeToEdge);
  ApplyViewportMovementToolState();
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

// Synchronizes the local axes toolbar toggle with user preferences.
void MainWindow::SyncLocalAxesToolToggleState() {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_LocalAxes,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kLocalTransformSpaceConfigKey) ==
          "1");
}


// Synchronizes the cross-table viewport-actions toolbar toggle with project settings.
void MainWindow::SyncCrossTableActionsToolToggleState() {
  if (!layoutViewsToolBar)
    return;
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_CrossTableActions,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          viewport_interaction_scope::kCrossTableActionsConfigKey) == "1");
}

// Applies the persisted movement tool state to the toolbar and active viewports.
void MainWindow::ApplyViewportMovementToolState() {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool axisConstraintEnabled =
      preferences.GetValue(
          selection_movement_settings::kAxisConstrainedMovementConfigKey) !=
      "0";
  const bool leftDragMoveEnabled =
      preferences.GetValue(
          selection_movement_settings::kLeftDragSelectionMovementConfigKey) ==
      "1";
  const bool magnetEnabled =
      preferences.GetValue(magnet_snap::kMagnetEnabledConfigKey) == "1";
  const bool localAxesEnabled =
      preferences.GetValue(
          selection_movement_settings::kLocalTransformSpaceConfigKey) == "1";

  if (viewport2DPanel) {
    viewport2DPanel->SetAxisConstrainedMovementEnabled(axisConstraintEnabled);
    viewport2DPanel->SetLeftDragSelectionMovementEnabled(leftDragMoveEnabled);
    viewport2DPanel->SetMagnetEnabled(magnetEnabled, false);
    viewport2DPanel->SetTransformSpace(localAxesEnabled
                                           ? transform_space::TransformSpace::Local
                                           : transform_space::TransformSpace::World);
  }
  if (viewportPanel) {
    viewportPanel->SetAxisConstrainedMovementEnabled(axisConstraintEnabled);
    viewportPanel->SetLeftDragSelectionMovementEnabled(leftDragMoveEnabled);
    viewportPanel->SetMagnetEnabled(magnetEnabled, false);
    viewportPanel->SetTransformSpace(localAxesEnabled
                                         ? transform_space::TransformSpace::Local
                                         : transform_space::TransformSpace::World);
  }
  SyncAxisConstraintToolToggleState();
  SyncLeftDragMoveToolToggleState();
  SyncLocalAxesToolToggleState();
  SyncCrossTableActionsToolToggleState();
  if (layoutViewsToolBar) {
    layoutViewsToolBar->ToggleTool(ID_View_Viewport_Magnet, magnetEnabled);
    layoutViewsToolBar->Refresh();
  }
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
      ((viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled() &&
        viewport2DPanel->GetMeasureToolMode() == Viewer2DMeasureMode::CenterToCenter) ||
       (viewportPanel && viewportPanel->IsMeasureToolEnabled() &&
        viewportPanel->GetMeasureToolMode() == Viewer2DMeasureMode::CenterToCenter));
  const bool enableMeasure = !measureEnabled;
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(enableMeasure, Viewer2DMeasureMode::CenterToCenter);
  if (viewportPanel)
    viewportPanel->SetMeasureToolEnabled(enableMeasure, Viewer2DMeasureMode::CenterToCenter);
  SyncViewportToolToggleState(enableMeasure, Viewer2DMeasureMode::CenterToCenter);
}


// Enables or disables the viewport gap measure tool and syncs toolbar toggle state.
void MainWindow::OnViewportGapMeasureTool(wxCommandEvent &WXUNUSED(event)) {
  const bool measureEnabled =
      ((viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled() &&
        viewport2DPanel->GetMeasureToolMode() == Viewer2DMeasureMode::EdgeToEdge) ||
       (viewportPanel && viewportPanel->IsMeasureToolEnabled() &&
        viewportPanel->GetMeasureToolMode() == Viewer2DMeasureMode::EdgeToEdge));
  const bool enableMeasure = !measureEnabled;
  if (viewport2DPanel)
    viewport2DPanel->SetMeasureToolEnabled(enableMeasure, Viewer2DMeasureMode::EdgeToEdge);
  if (viewportPanel)
    viewportPanel->SetMeasureToolEnabled(enableMeasure, Viewer2DMeasureMode::EdgeToEdge);
  SyncViewportToolToggleState(enableMeasure, Viewer2DMeasureMode::EdgeToEdge);
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
  preferences.SaveUserConfig();
  ApplyViewportMovementToolState();
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
  preferences.SaveUserConfig();
  ApplyViewportMovementToolState();
}

// Toggles user-level local axes for viewport transforms.
void MainWindow::OnViewportLocalAxes(wxCommandEvent &WXUNUSED(event)) {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool enabled =
      preferences.GetValue(
          selection_movement_settings::kLocalTransformSpaceConfigKey) == "1";
  preferences.SetValue(selection_movement_settings::kLocalTransformSpaceConfigKey,
                       enabled ? "0" : "1");
  preferences.SaveUserConfig();
  ApplyViewportMovementToolState();
}

// Fits the viewport that currently owns keyboard focus.
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

// Applies a standard view shortcut to the active or only visible viewport.
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
  preferences.SetValue(magnet_snap::kMagnetEnabledConfigKey,
                       enabled ? "1" : "0");
  preferences.SaveUserConfig();
  ApplyViewportMovementToolState();
}

// Toggles viewport actions across all table types and persists the preference.
void MainWindow::OnViewportCrossTableActions(wxCommandEvent &WXUNUSED(event)) {
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  const bool enabled =
      preferences.GetValue(
          viewport_interaction_scope::kCrossTableActionsConfigKey) == "1";
  preferences.SetValue(
      viewport_interaction_scope::kCrossTableActionsConfigKey,
      enabled ? "0" : "1");
  preferences.SaveUserConfig();
  SyncCrossTableActionsToolToggleState();
  if (viewport2DPanel)
    viewport2DPanel->Refresh();
  if (viewportPanel)
    viewportPanel->Refresh();
}
