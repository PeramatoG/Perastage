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
#include "alignment_geometry.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace alignment {
Projection WorkPlane(Plane plane) {
  switch (plane) {
  case Plane::XY: return {0, 1, 1, 1};
  case Plane::XZ: return {0, 2, 1, 1};
  case Plane::YZ: return {1, 2, -1, 1};
  }
  throw std::invalid_argument("Unsupported alignment work plane.");
}
bool Valid(const Bounds &bounds) {
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) ||
        bounds.min[axis] > bounds.max[axis]) return false;
  return true;
}
Rectangle Project(const Bounds &bounds, Plane plane) {
  const auto p = WorkPlane(plane);
  const double h0 = bounds.min[p.horizontal] * p.horizontalSign;
  const double h1 = bounds.max[p.horizontal] * p.horizontalSign;
  const double v0 = bounds.min[p.vertical] * p.verticalSign;
  const double v1 = bounds.max[p.vertical] * p.verticalSign;
  return {std::min(h0,h1), std::min(v0,v1), std::max(h0,h1), std::max(v0,v1)};
}
namespace {
bool Horizontal(Operation operation) {
  return operation == Operation::Left || operation == Operation::Right ||
         operation == Operation::CenterHorizontal;
}
double Anchor(const Rectangle &r, Operation operation) {
  switch (operation) {
  case Operation::Top: return r.top;
  case Operation::Bottom: return r.bottom;
  case Operation::Left: return r.left;
  case Operation::Right: return r.right;
  case Operation::CenterHorizontal: return r.left/2 + r.right/2;
  case Operation::CenterVertical: return r.bottom/2 + r.top/2;
  }
  throw std::invalid_argument("Unsupported alignment operation.");
}
}
std::array<double, 3> WorldDelta(double offset, Plane plane, Operation operation) {
  const auto p = WorkPlane(plane);
  std::array<double, 3> delta{};
  delta[Horizontal(operation) ? p.horizontal : p.vertical] =
      offset * (Horizontal(operation) ? p.horizontalSign : p.verticalSign);
  return delta;
}
Plan Calculate(const std::vector<Rectangle> &rectangles, Reference reference,
               Operation operation) {
  Plan plan;
  if (rectangles.size() < 2) {
    plan.diagnostic = "Select at least two distinct effective transform targets.";
    return plan;
  }
  Rectangle extents = rectangles.front();
  for (const auto &r : rectangles) {
    if (!std::isfinite(r.left) || !std::isfinite(r.right) ||
        !std::isfinite(r.bottom) || !std::isfinite(r.top) ||
        r.left > r.right || r.bottom > r.top) {
      plan.diagnostic = "Alignment requires finite, ordered geometric extents.";
      return plan;
    }
    extents.left = std::min(extents.left, r.left);
    extents.right = std::max(extents.right, r.right);
    extents.bottom = std::min(extents.bottom, r.bottom);
    extents.top = std::max(extents.top, r.top);
  }
  if (reference != Reference::Extents && reference != Reference::First &&
      reference != Reference::Last) {
    plan.diagnostic = "Unsupported alignment reference.";
    return plan;
  }
  const auto &r = reference == Reference::First ? rectangles.front() :
                  reference == Reference::Last ? rectangles.back() : extents;
  const double anchor = Anchor(r, operation);
  for (const auto &rectangle : rectangles) {
    const double offset = anchor - Anchor(rectangle, operation);
    if (!std::isfinite(offset)) {
      plan.diagnostic = "Alignment translation exceeds finite precision.";
      plan.offsets.clear();
      return plan;
    }
    plan.offsets.push_back(offset);
    plan.changed |= std::abs(offset) > 0.0001;
  }
  plan.valid = true;
  return plan;
}
} // namespace alignment
