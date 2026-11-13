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
#pragma once
#include "LayoutCollection.h"
#include "viewer2dpanel.h"
#include "viewer2dcommandrenderer.h"
#include <algorithm>
namespace viewer2d {
inline void AttachPhysicalMapping(const layouts::Layout2DViewDefinition &v,
                                  Viewer2DViewState &state) {
  if (v.scaleMode != layouts::ViewportScaleMode::Fixed) return;
  layouts::ValidateViewport(v);
  const auto f = layouts::ResolveViewportFrame(v);
  state.physicalMapping = layouts::ResolvePhysicalViewport(*v.physicalScale, f.width, f.height, v.camera.view);
}
// This camera is capture-only; authoritative scale stays in document points.
inline void PreparePhysicalCapture(const layouts::Layout2DViewDefinition &v,
    layouts::Layout2DViewCameraState &camera, int width, int height) {
  if (v.scaleMode != layouts::ViewportScaleMode::Fixed) return;
  layouts::ValidateViewport(v);
  const auto f = layouts::ResolveViewportFrame(v);
  const auto m = layouts::ResolvePhysicalViewport(*v.physicalScale, f.width, f.height, v.camera.view);
  camera.viewportWidth = width;
  camera.viewportHeight = height;
  camera.zoom = static_cast<float>(std::min(width / m.width, height / m.height) /
                                  kViewer2DPixelsPerMeter);
  camera.offsetPixelsX = static_cast<float>(-layouts::ViewportHorizontalSign(v.camera.view) * v.physicalScale->centerX * kViewer2DPixelsPerMeter);
  camera.offsetPixelsY = static_cast<float>(-v.physicalScale->centerY * kViewer2DPixelsPerMeter);
}
} // namespace viewer2d
