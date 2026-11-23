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
