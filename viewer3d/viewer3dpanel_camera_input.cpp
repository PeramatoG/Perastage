#include "viewer3dpanel.h"

#include "../gui/mainwindow/ids/tools_ids.h"
#include "editable_focus_utils.h"
#include "mainwindow.h"
#include "navigation_diagnostics.h"

#include <chrono>

namespace {
constexpr int kZoomInteractionTimeoutMs = 260;
} // namespace

// Applies an already-resolved camera drag intent to panel state.
void Viewer3DPanel::ApplyCameraIntent(
    const viewer3d::interaction::CameraDragIntent &intent,
    const wxPoint &mousePos) {
  m_navigationSession.MarkMoved();
  m_runtimeState.BeginInteraction(std::chrono::steady_clock::now());
  if (intent.action == viewer3d::interaction::CameraDragIntent::Action::Orbit)
    m_camera.Orbit(intent.horizontal, intent.vertical);
  else
    m_camera.Pan(intent.horizontal, intent.vertical);
  m_lastMousePos = mousePos;
  m_continuousPlacementSession.InvalidateView();
}

// Handles mouse wheel (zoom)
void Viewer3DPanel::OnMouseWheel(wxMouseEvent &event) {
  m_hasLastMousePos = true;
  viewer3d::diagnostics::Logf("Mouse wheel start rotation=%d delta=%d",
                              event.GetWheelRotation(), event.GetWheelDelta());
  SetFocus();

  // wxWidgets may report multiple wheel detents in a single event.
  // Use the ratio of the rotation to the wheel delta to scale zoom
  // steps accordingly so that large scrolls result in proportionally
  // larger zoom changes.
  const auto steps = viewer3d::interaction::ResolveWheelZoomSteps(
      event.GetWheelRotation(), event.GetWheelDelta());
  if (!steps) {
    viewer3d::diagnostics::Log(
        "Mouse wheel ignored because zoom steps are invalid or zero.");
    return;
  }
  m_controller.SetInteracting(true);
  m_runtimeState.BeginInteraction(std::chrono::steady_clock::now());

  m_camera.Zoom(*steps);
  m_continuousPlacementSession.InvalidateView();
  if (IsContinuousPlacementActive())
    AlignContinuousElementToPointer(event.GetPosition());
  ArmZoomInteractionTimeout();

  Refresh();
}

// Handles keyboard shortcuts for viewport tools and camera actions.
void Viewer3DPanel::OnKeyDown(wxKeyEvent &event) {
  viewer3d::diagnostics::Logf("Key interaction start key=%d shift=%d alt=%d",
                              event.GetKeyCode(), event.ShiftDown() ? 1 : 0,
                              event.AltDown() ? 1 : 0);
  if (!m_mouseInside && !HasFocus()) {
    event.Skip();
    return;
  }
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  std::optional<viewer3d::interaction::CameraDirection> direction;
  switch (event.GetKeyCode()) {
  case WXK_ESCAPE:
    if (m_linePointSelectionSession.IsActive()) {
      CancelLinePointSelection();
      return;
    }
    if (IsContinuousPlacementActive()) {
      CancelContinuousPlacement();
      return;
    }
    if (IsMeasureToolEnabled()) {
      SetMeasureToolEnabled(false);
      return;
    }
    event.Skip();
    return;
  case 'M':
  case 'm':
    SetMeasureToolEnabled(!IsMeasureToolEnabled());
    return;
  case WXK_LEFT:
    direction = viewer3d::interaction::CameraDirection::Left;
    break;
  case WXK_RIGHT:
    direction = viewer3d::interaction::CameraDirection::Right;
    break;
  case WXK_UP:
    direction = viewer3d::interaction::CameraDirection::Up;
    break;
  case WXK_DOWN:
    direction = viewer3d::interaction::CameraDirection::Down;
    break;
  case WXK_DELETE:
  case WXK_NUMPAD_DELETE: {
    if (MainWindow::Instance()) {
      wxCommandEvent deleteEvent(wxEVT_MENU, ID_Edit_Delete);
      MainWindow::Instance()->GetEventHandler()->ProcessEvent(deleteEvent);
      return;
    }
    event.Skip();
    return;
  }
  default:
    event.Skip();
    return;
  }

  const auto cameraIntent = viewer3d::interaction::ResolveKeyboardCameraInput(
      {*direction, event.ShiftDown(), event.AltDown()});
  const bool zoomTriggered =
      cameraIntent.action ==
      viewer3d::interaction::KeyboardCameraIntent::Action::Zoom;
  if (cameraIntent.action ==
      viewer3d::interaction::KeyboardCameraIntent::Action::Orbit)
    m_camera.Orbit(cameraIntent.horizontal, cameraIntent.vertical);
  else if (cameraIntent.action ==
           viewer3d::interaction::KeyboardCameraIntent::Action::Pan)
    m_camera.Pan(cameraIntent.horizontal, cameraIntent.vertical);
  else
    m_camera.Zoom(cameraIntent.horizontal);

  m_controller.SetInteracting(true);
  m_runtimeState.BeginInteraction(std::chrono::steady_clock::now());
  if (zoomTriggered)
    ArmZoomInteractionTimeout();
  m_continuousPlacementSession.InvalidateView();
  if (IsContinuousPlacementActive() && m_hasLastMousePos)
    AlignContinuousElementToPointer(m_lastMousePos);

  viewer3d::diagnostics::Log("Key interaction end.");
  Refresh();
}

// Arms the timeout that ends wheel-driven zoom interaction.
void Viewer3DPanel::ArmZoomInteractionTimeout() {
  m_zoomInteractionTimer.StartOnce(kZoomInteractionTimeoutMs);
}

// Ends wheel-driven zoom interaction after input has gone idle.
void Viewer3DPanel::OnZoomInteractionTimeout(wxTimerEvent &event) {
  (void)event;

  if (m_navigationSession.IsActive() || m_rectSelecting)
    return;

  m_runtimeState.EndInteraction();
  m_controller.SetInteracting(false);
  m_controller.SetCameraMoving(false);
  m_controller.MarkResourceSyncPending();
  m_runtimeState.MarkPointerMoved();
  Refresh();
}
