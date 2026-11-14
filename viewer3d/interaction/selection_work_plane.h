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
#include "alignment_geometry.h"
#include "viewer3d_types.h"
#include <cmath>

namespace viewer3d {
inline alignment::Plane WorkPlaneForView(Viewer2DView view) {
  switch (view) {
  case Viewer2DView::Front: return alignment::Plane::XZ;
  case Viewer2DView::Side: return alignment::Plane::YZ;
  default: return alignment::Plane::XY;
  }
}
inline int WorkPlaneNormalAxis(alignment::Plane plane) {
  const auto axes = alignment::WorkPlane(plane);
  return 3 - axes.horizontal - axes.vertical;
}
inline std::array<double,3> WorkPlaneNormal(alignment::Plane plane) {
  std::array<double,3> normal{};
  normal[WorkPlaneNormalAxis(plane)] = 1;
  return normal;
}
// Project local/world drag axes onto the explicit world plane before picking.
inline std::array<float,3> ProjectDragAxisToPlane(std::array<float,3> axis, alignment::Plane plane) {
  axis[WorkPlaneNormalAxis(plane)] = 0;
  const double length = std::hypot(axis[0],axis[1],axis[2]);
  if (length < 1e-8 || !std::isfinite(length)) return {};
  for (auto &component : axis) component = static_cast<float>(component/length);
  return axis;
}
} // namespace viewer3d
