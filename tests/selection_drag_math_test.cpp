#include "selection_drag_math.h"

#include <cassert>
#include <cmath>

// Verifies perspective plane depth and constrained-axis math use explicit data.
int main() {
  const std::array<double, 3> origin{0.0, 0.0, 0.0};
  const std::array<double, 3> previousRay{0.0, 0.0, 1.0};
  const std::array<double, 3> currentRay{0.2, 0.0, 1.0};
  const std::array<double, 3> normal{0.0, 0.0, 1.0};
  const auto previousRaw = viewer3d::IntersectRayWithPlane(
      origin, previousRay, {0.0, 0.0, 5.0}, normal);
  const auto currentRaw = viewer3d::IntersectRayWithPlane(
      origin, currentRay, {0.0, 0.0, 5.0}, normal);
  const auto currentDisplayed = viewer3d::IntersectRayWithPlane(
      origin, currentRay, {0.0, 0.0, 8.0}, normal);
  assert(previousRaw && currentRaw && currentDisplayed);
  assert(std::abs(((*currentRaw)[0] - (*previousRaw)[0]) - 1.0) < 1e-9);
  assert(std::abs((*currentDisplayed)[0] - 1.6) < 1e-9);
  assert(!viewer3d::IntersectRayWithPlane(origin, {1.0, 0.0, 0.0},
                                          {0.0, 0.0, 5.0}, normal));

  const std::array<viewer3d::ProjectedAxis, 3> axes{
      {{1.0, 0.0, 10.0, true}, {0.0, 1.0, 20.0, true}, {0.5, 0.5, 5.0, true}}};
  assert(viewer3d::SelectDragAxisFromMouseDelta(10, 1, axes) ==
         viewer3d::SelectionDragAxis::X);
  assert(std::abs(viewer3d::ComputeDragMetersOnAxis(
                      10, 0, viewer3d::SelectionDragAxis::X, axes) -
                  1.0) < 1e-9);
  assert(viewer3d::SelectDragAxisFromMouseDelta(14, 2, axes) ==
         viewer3d::SelectionDragAxis::X);
  assert(viewer3d::SelectDragAxisFromMouseDelta(6, 18, axes) ==
         viewer3d::SelectionDragAxis::Y);
  assert(std::abs(viewer3d::ComputeDragMetersOnAxis(
                      6, 18, viewer3d::SelectionDragAxis::Y, axes) -
                  0.9) < 1e-9);
  assert(std::abs(viewer3d::ComputeProjectedPixelsOnAxis(
                      6, 18, viewer3d::SelectionDragAxis::Y, axes) -
                  18.0) < 1e-9);
  const auto switchIntent = viewer3d::DetectAxisSwitchIntent(
      40, 100, viewer3d::SelectionDragAxis::Y, axes);
  assert(switchIntent.axis == viewer3d::SelectionDragAxis::X);
  assert(viewer3d::DetectAxisSwitchIntent(
             8, 100, viewer3d::SelectionDragAxis::Y, axes)
             .axis == viewer3d::SelectionDragAxis::None);

  const std::array<viewer3d::ProjectedAxis, 3> overlappingAxes{{
      {0.0, 1.0, 4.0, true},
      {1.0, 0.0, 12.0, true},
      {0.0, 2.0, 18.0, true},
  }};
  assert(viewer3d::SelectDragAxisFromMouseDelta(1, 30, overlappingAxes) ==
         viewer3d::SelectionDragAxis::Z);
  assert(viewer3d::SelectDragAxisFromMouseDelta(30, 1, overlappingAxes) ==
         viewer3d::SelectionDragAxis::Y);
  return 0;
}
