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

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace layouts {
// Projected scene metres: Top=(X,Y), Front=(X,Z), Side=(Y,Z).
// Stored center uses unprojected world axes. Side projects horizontal=-Y;
// Bottom (compatibility) projects horizontal=-X. Document points grow right/down.
enum class ViewportScaleMode { Free, Fixed };
struct PhysicalViewport {
  double denominator = 50.0;
  double centerX = 0.0;
  double centerY = 0.0;
};
struct ViewportFramePoints {
  double x = 0, y = 0, width = 0, height = 0;
};
struct PhysicalViewportMapping {
  double minX, minY, width, height, pointsPerMeter;
};
inline void ValidatePhysicalViewport(const PhysicalViewport &v) {
  if (!std::isfinite(v.denominator) || v.denominator < 0.01 || v.denominator > 1e7)
    throw std::invalid_argument("Drawing scale denominator must be between 0.01 and 10000000.");
  if (!std::isfinite(v.centerX) || !std::isfinite(v.centerY) ||
      std::abs(v.centerX) > 1e7 || std::abs(v.centerY) > 1e7)
    throw std::invalid_argument("View center must contain finite scene metres within +/-10000000.");
}
inline void ValidateViewportFrame(const ViewportFramePoints &f) {
  if (!std::isfinite(f.x) || !std::isfinite(f.y) ||
      !std::isfinite(f.width) || !std::isfinite(f.height) ||
      std::abs(f.x) > 200000 || std::abs(f.y) > 200000 ||
      f.width < 1.0 || f.height < 1.0 || f.width > 200000 || f.height > 200000)
    throw std::invalid_argument("Frame positions must be within +/-200000 pt and sizes between 1 and 200000 pt.");
}
inline double ViewportHorizontalSign(int orientation) { return orientation == 2 || orientation == 3 ? -1.0 : 1.0; }
inline PhysicalViewportMapping ResolvePhysicalViewport(
    const PhysicalViewport &v, double widthPt, double heightPt, int orientation = 0) {
  ValidatePhysicalViewport(v);
  ValidateViewportFrame({0, 0, widthPt, heightPt});
  const double ppm = 1000.0 / v.denominator * 72.0 / 25.4;
  const double w = widthPt / ppm, h = heightPt / ppm;
  return {ViewportHorizontalSign(orientation) * v.centerX - w / 2, v.centerY - h / 2, w, h, ppm};
}
// A physical declaration may only be exported at document size. Legacy fitting
// remains available for documents without fixed viewports.
inline void ValidatePhysicalOutput(bool hasFixed, double sx, double sy) {
  if (hasFixed && (!std::isfinite(sx) || !std::isfinite(sy) ||
      std::abs(sx - 1.0) > 1e-9 || std::abs(sy - 1.0) > 1e-9))
    throw std::invalid_argument("Fixed-scale viewports require 100% document-size PDF output. Select the Layout paper size.");
}
} // namespace layouts
