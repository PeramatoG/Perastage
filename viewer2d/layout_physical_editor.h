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
#include "PhysicalViewport.h"
namespace viewer2d {
struct PhysicalEditorGeometry {
  double left, bottom, width, height, zoom;
};
// Fit the red frame to the available display. Camera and border use these same
// dimensions, without independently rounding either axis to integer pixels.
inline PhysicalEditorGeometry ResolvePhysicalEditorGeometry(
    const layouts::PhysicalViewport &physical,
    const layouts::ViewportFramePoints &frame, int width, int height,
    double pixelsPerMeter) {
  if (width <= 0 || height <= 0 || !std::isfinite(pixelsPerMeter) || pixelsPerMeter <= 0)
    throw std::invalid_argument("Editor dimensions and pixel conversion must be positive.");
  const auto world = layouts::ResolvePhysicalViewport(physical, frame.width, frame.height);
  const double padding = std::min(width, height) * 0.1;
  const double paperZoom = std::min((width-2*padding)/frame.width,
                                   (height-2*padding)/frame.height);
  const double w=frame.width*paperZoom, h=frame.height*paperZoom;
  return {(width-w)/2, (height-h)/2, w, h, w/world.width/pixelsPerMeter};
}
// Anchor deltas preserve the original double center when no pan occurred.
inline layouts::PhysicalViewport ResolvePhysicalEditorPan(
    layouts::PhysicalViewport physical, double dx, double dy,
    int orientation, double pixelsPerMeter) {
  physical.centerX -= layouts::ViewportHorizontalSign(orientation)*dx/pixelsPerMeter;
  physical.centerY -= dy/pixelsPerMeter;
  return physical;
}
}
