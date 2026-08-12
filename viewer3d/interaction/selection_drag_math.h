#pragma once

#include <array>
#include <optional>

namespace viewer3d {

enum class SelectionDragAxis { None, X, Y, Z };

struct ProjectedAxis {
  double screenDx = 0.0;
  double screenDy = 0.0;
  double pixelsPerMeter = 0.0;
  bool valid = false;
};

SelectionDragAxis
SelectDragAxisFromMouseDelta(int mouseDx, int mouseDy,
                             const std::array<ProjectedAxis, 3> &axes,
                             int activationThresholdPx = 3);

struct AxisSwitchIntent {
  SelectionDragAxis axis = SelectionDragAxis::None;
};

AxisSwitchIntent DetectAxisSwitchIntent(
    int mouseDx, int mouseDy, SelectionDragAxis activeAxis,
    const std::array<ProjectedAxis, 3> &axes, int switchThresholdPx = 12);

double ComputeDragMetersOnAxis(int mouseDx, int mouseDy, SelectionDragAxis axis,
                               const std::array<ProjectedAxis, 3> &axes);

double ComputeProjectedPixelsOnAxis(
    int mouseDx, int mouseDy, SelectionDragAxis axis,
    const std::array<ProjectedAxis, 3> &axes);

// Intersects a world-space ray with a plane for deterministic drag projection.
std::optional<std::array<double, 3>>
IntersectRayWithPlane(const std::array<double, 3> &rayOrigin,
                      const std::array<double, 3> &rayDirection,
                      const std::array<double, 3> &planePoint,
                      const std::array<double, 3> &planeNormal);

} // namespace viewer3d
