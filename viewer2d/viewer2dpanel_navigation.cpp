#include "viewer2dpanel.h"

#include "configmanager.h"
#include "pixel_coordinate_math.h"
#include "performance_metrics.h"
#include "interaction/viewer2d_navigation_policy.h"

void Viewer2DPanel::LoadViewFromConfig() {
  m_navigationPersistencePending = false;
  ConfigManager &cfg = ConfigManager::Get();
  m_offsetX = cfg.GetFloat("view2d_offset_x");
  m_offsetY = cfg.GetFloat("view2d_offset_y");
  m_zoom = cfg.GetFloat("view2d_zoom");
  m_renderMode = static_cast<Viewer2DRenderMode>(
      static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_view =
      static_cast<Viewer2DView>(static_cast<int>(cfg.GetFloat("view2d_view")));
  m_runtimeState.MarkViewMotion();
  m_fastHoverHasPos = false;
}

void Viewer2DPanel::SaveViewToConfig() const {
  m_navigationPersistencePending = false;
  if (!m_persistViewState)
    return;
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("view2d_offset_x", m_offsetX);
  cfg.SetFloat("view2d_offset_y", m_offsetY);
  cfg.SetFloat("view2d_zoom", m_zoom);
  cfg.SetFloat("view2d_render_mode", static_cast<float>(m_renderMode));
  cfg.SetFloat("view2d_view", static_cast<float>(m_view));
}

void Viewer2DPanel::MarkNavigationChanged() {
  m_navigationPersistencePending = true;
  if (m_persistViewState)
    m_performance.NavigationChanged();
  m_placementViewRevision.Invalidate();
  m_runtimeState.MarkViewMotion();
  m_fastHoverHasPos = false;
  MarkInteractionActivity();
}

void Viewer2DPanel::PersistPendingNavigation() const {
  if (m_navigationPersistencePending)
    SaveViewToConfig();
}

void Viewer2DPanel::PanViewTo(const wxPoint &position) {
  const auto framebufferDelta = pixel_coordinates::IncrementalFramebufferDelta(
      {static_cast<double>(position.x), static_cast<double>(position.y)},
      {static_cast<double>(m_lastMousePos.x), static_cast<double>(m_lastMousePos.y)},
      GetContentScaleFactor());
  const wxPoint delta = framebufferDelta
      ? wxPoint((*framebufferDelta)[0], (*framebufferDelta)[1])
      : position - m_lastMousePos;
  m_lastMousePos = position;
  if (delta.x == 0 && delta.y == 0)
    return;
  const auto navigation =
      viewer2d::interaction::Viewer2DNavigationPolicy::ApplyDragPan(
          {m_offsetX, m_offsetY, m_zoom}, delta.x, delta.y);
  m_offsetX = navigation.view.offsetX;
  m_offsetY = navigation.view.offsetY;
  m_interaction.MarkNavigationMoved({position.x, position.y});
  MarkNavigationChanged();
  RequestRepaint();
}

void Viewer2DPanel::OnInteractionPauseTimer(wxTimerEvent &WXUNUSED(event)) {
  if (m_interaction.mode != DragMode::View)
    PersistPendingNavigation();
  if (!ShouldPauseHeavyTasks())
    RequestRepaint();
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent &event) {
  if (IsLayoutPhysicalZoomLocked())
    return;

  performance::Scope metric(performance::Work::View2DNavigation,
                            m_persistViewState && performance::Viewer2DEnabled());
  m_hasLastMousePos = true;

  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  const auto navigation =
      viewer2d::interaction::Viewer2DNavigationPolicy::ApplyWheelZoom(
          {m_offsetX, m_offsetY, m_zoom}, steps);
  if (navigation.view.zoom == m_zoom)
    return;
  m_zoom = navigation.view.zoom;
  MarkNavigationChanged();
  if (m_interaction.mode != DragMode::View) {
    m_lastMousePos = event.GetPosition();
    if (m_placementSession.IsActive())
      AlignContinuousElementToPointer(m_lastMousePos);
  }
  RequestRepaint();
}

// Applies keyboard pan and zoom navigation to the 2D viewport.
bool Viewer2DPanel::TryHandleViewportNavigationKey(int keyCode, bool altDown) {
  using NavigationKey = viewer2d::interaction::NavigationKey;
  NavigationKey key = NavigationKey::Unsupported;
  switch (keyCode) {
  case WXK_LEFT:
    key = NavigationKey::Left;
    break;
  case WXK_RIGHT:
    key = NavigationKey::Right;
    break;
  case WXK_UP:
    key = NavigationKey::Up;
    break;
  case WXK_DOWN:
    key = NavigationKey::Down;
    break;
  default:
    break;
  }

  if (key == NavigationKey::Unsupported)
    return false;
  const bool zoomLocked = IsLayoutPhysicalZoomLocked();
  if (zoomLocked && altDown)
    return true;

  performance::Scope metric(performance::Work::View2DNavigation,
                            m_persistViewState && performance::Viewer2DEnabled());
  const auto navigation =
      viewer2d::interaction::Viewer2DNavigationPolicy::ApplyKeyboard(
          {m_offsetX, m_offsetY, m_zoom}, key, altDown);
  if (!navigation.handled)
    return false;
  if (m_offsetX == navigation.view.offsetX &&
      m_offsetY == navigation.view.offsetY &&
      (zoomLocked || m_zoom == navigation.view.zoom))
    return true;
  m_offsetX = navigation.view.offsetX;
  m_offsetY = navigation.view.offsetY;
  if (!zoomLocked)
    m_zoom = navigation.view.zoom;
  MarkNavigationChanged();
  if (m_placementSession.IsActive() && m_hasLastMousePos &&
      m_interaction.mode != DragMode::View)
    AlignContinuousElementToPointer(m_lastMousePos);
  RequestRepaint();
  return true;
}
