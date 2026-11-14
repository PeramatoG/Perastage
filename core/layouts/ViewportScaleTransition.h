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
#include <limits>

namespace layouts {
// Keep the free zoom and capture dimensions while carrying the user's latest
// world center across the scale-mode boundary. Legacy offsets remain floats.
inline Layout2DViewCameraState RecenterViewportCamera(
    Layout2DViewCameraState camera, const PhysicalViewport &center,
    int orientation, double pixelsPerMeter) {
  ValidatePhysicalViewport(center);
  if (orientation<0 || orientation>3 || !std::isfinite(pixelsPerMeter) || pixelsPerMeter<=0)
    throw std::invalid_argument("Camera orientation and pixel conversion must be valid.");
  const double x=-ViewportHorizontalSign(orientation)*center.centerX*pixelsPerMeter;
  const double y=-center.centerY*pixelsPerMeter;
  if (!std::isfinite(x) || !std::isfinite(y) ||
      std::abs(x)>std::numeric_limits<float>::max() || std::abs(y)>std::numeric_limits<float>::max())
    throw std::invalid_argument("View center cannot be represented by the captured camera.");
  camera.offsetPixelsX=static_cast<float>(x);
  camera.offsetPixelsY=static_cast<float>(y);
  camera.view=orientation;
  return camera;
}
}
