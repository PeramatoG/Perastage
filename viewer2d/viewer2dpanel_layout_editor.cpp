/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "viewer2dpanel.h"
#include "viewer2dcommandrenderer.h"
#include "interaction/viewer2d_navigation_policy.h"
#include "ui_render_size.h"

void Viewer2DPanel::SetLayoutPhysicalViewport(
    const layouts::PhysicalViewport &physical, const layouts::ViewportFramePoints &frame) {
  layouts::ValidatePhysicalViewport(physical);
  layouts::ValidateViewportFrame(frame);
  m_offsetX = static_cast<float>(-layouts::ViewportHorizontalSign(static_cast<int>(m_view))*physical.centerX*viewer2d::kViewer2DPixelsPerMeter);
  m_offsetY = static_cast<float>(-physical.centerY*viewer2d::kViewer2DPixelsPerMeter);
  m_physicalEditor = PhysicalEditorState{physical, frame, m_offsetX, m_offsetY, m_view};
  m_layoutEditAspect = static_cast<float>(frame.width/frame.height);
  m_layoutEditScale = 1;
  const auto size = ResolveRenderSize(this);
  UpdateLayoutPhysicalCamera(size.width, size.height);
  InvalidatePickCache();
  RequestRepaint();
}
void Viewer2DPanel::ClearLayoutPhysicalViewport() { m_physicalEditor.reset(); }
std::optional<layouts::PhysicalViewport> Viewer2DPanel::GetLayoutPhysicalViewport() const {
  if (!m_physicalEditor) return std::nullopt;
  const auto &s = *m_physicalEditor;
  return viewer2d::ResolvePhysicalEditorPan(s.physical,
      m_offsetX-s.referenceX, m_offsetY-s.referenceY,
      static_cast<int>(s.orientation), viewer2d::kViewer2DPixelsPerMeter);
}
void Viewer2DPanel::UpdateLayoutPhysicalCamera(int width, int height) {
  if (!m_physicalEditor || width <= 0 || height <= 0) return;
  if (m_physicalEditor->orientation != m_view) {
    const auto physical = *GetLayoutPhysicalViewport();
    const auto frame = m_physicalEditor->frame;
    SetLayoutPhysicalViewport(physical, frame);
  }
  const auto &s = *m_physicalEditor;
  m_zoom = static_cast<float>(viewer2d::ResolvePhysicalEditorGeometry(
      s.physical, s.frame, width, height, viewer2d::kViewer2DPixelsPerMeter).zoom);
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent &event) {
  if (IsLayoutPhysicalZoomLocked()) return;
  m_hasLastMousePos = true;
  MarkInteractionActivity();

  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  const auto navigation =
      viewer2d::interaction::Viewer2DNavigationPolicy::ApplyWheelZoom(
          {m_offsetX, m_offsetY, m_zoom}, steps);
  m_zoom = navigation.view.zoom;
  m_runtimeState.MarkViewMotion();
  m_placementViewRevision.Invalidate();
  if (m_placementSession.IsActive())
    AlignContinuousElementToPointer(event.GetPosition());
  if (m_persistViewState)
    SaveViewToConfig();
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

  if (IsLayoutPhysicalZoomLocked() && altDown && key != NavigationKey::Unsupported) return true;
  const auto navigation =
      viewer2d::interaction::Viewer2DNavigationPolicy::ApplyKeyboard(
          {m_offsetX, m_offsetY, m_zoom}, key, altDown);
  if (!navigation.handled)
    return false;
  m_offsetX = navigation.view.offsetX;
  m_offsetY = navigation.view.offsetY;
  if (!IsLayoutPhysicalZoomLocked()) m_zoom = navigation.view.zoom;
  m_placementViewRevision.Invalidate();
  if (m_placementSession.IsActive() && m_hasLastMousePos)
    AlignContinuousElementToPointer(m_lastMousePos);
  InvalidatePickCache();
  if (m_persistViewState)
    SaveViewToConfig();
  RequestRepaint();
  return true;
}
