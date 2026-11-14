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
#include "viewer2d_coordinate_math.h"
namespace viewer2d {
struct LayoutProjectionBounds { double left, right, bottom, top; };
// Native raster previews and vector mappings resolve the same physical crop.
// Uniform letterboxing accounts only for rounding the target to whole pixels.
inline std::optional<LayoutProjectionBounds> ResolveLayoutProjectionBounds(
    const CoordinateTransform &camera,
    const std::optional<layouts::PhysicalViewportMapping> &physical) {
  if (!physical) {
    const auto legacy = ComputeOrthographicBounds(camera);
    if (!legacy) return std::nullopt;
    return LayoutProjectionBounds{legacy->left,legacy->right,legacy->bottom,legacy->top};
  }
  const auto &m=*physical;
  if (camera.framebufferWidth<=0 || camera.framebufferHeight<=0 ||
      !std::isfinite(m.minX) || !std::isfinite(m.minY) ||
      !std::isfinite(m.width) || !std::isfinite(m.height) || m.width<=0 || m.height<=0)
    return std::nullopt;
  const double ppm=std::min(camera.framebufferWidth/m.width,camera.framebufferHeight/m.height);
  const double halfW=camera.framebufferWidth/ppm/2, halfH=camera.framebufferHeight/ppm/2;
  const double cx=m.minX+m.width/2, cy=m.minY+m.height/2;
  return LayoutProjectionBounds{cx-halfW,cx+halfW,cy-halfH,cy+halfH};
}
}
