#include "selection_drag_math.h"

#include <cmath>

namespace viewer3d {
namespace {

double Dot(int mouseDx, int mouseDy, const ProjectedAxis &axis) {
  return static_cast<double>(mouseDx) * axis.screenDx +
         static_cast<double>(mouseDy) * axis.screenDy;
}

double ScreenLengthSquared(const ProjectedAxis &axis) {
  return axis.screenDx * axis.screenDx + axis.screenDy * axis.screenDy;
}

size_t AxisToIndex(SelectionDragAxis axis) {
  switch (axis) {
  case SelectionDragAxis::X:
    return 0;
  case SelectionDragAxis::Y:
    return 1;
  case SelectionDragAxis::Z:
    return 2;
  case SelectionDragAxis::None:
  default:
    return 0;
  }
}

SelectionDragAxis IndexToAxis(size_t index) {
  if (index == 0)
    return SelectionDragAxis::X;
  if (index == 1)
    return SelectionDragAxis::Y;
  if (index == 2)
    return SelectionDragAxis::Z;
  return SelectionDragAxis::None;
}

} // namespace

SelectionDragAxis
SelectDragAxisFromMouseDelta(int mouseDx, int mouseDy,
                             const std::array<ProjectedAxis, 3> &axes,
                             int activationThresholdPx) {
  if (std::abs(mouseDx) < activationThresholdPx &&
      std::abs(mouseDy) < activationThresholdPx) {
    return SelectionDragAxis::None;
  }

  double bestScore = 0.0;
  SelectionDragAxis bestAxis = SelectionDragAxis::None;
  for (size_t axisIndex = 0; axisIndex < axes.size(); ++axisIndex) {
    const ProjectedAxis &axis = axes[axisIndex];
    if (!axis.valid || axis.pixelsPerMeter <= 0.0)
      continue;
    const double lenSq = ScreenLengthSquared(axis);
    if (lenSq <= 1e-8)
      continue;
    const double projected =
        std::abs(Dot(mouseDx, mouseDy, axis)) / std::sqrt(lenSq);
    if (projected > bestScore) {
      bestScore = projected;
      bestAxis = IndexToAxis(axisIndex);
    }
  }
  return bestAxis;
}

double ComputeDragMetersOnAxis(int mouseDx, int mouseDy, SelectionDragAxis axis,
                               const std::array<ProjectedAxis, 3> &axes) {
  if (axis == SelectionDragAxis::None)
    return 0.0;
  const ProjectedAxis &projectedAxis = axes[AxisToIndex(axis)];
  if (!projectedAxis.valid || projectedAxis.pixelsPerMeter <= 0.0)
    return 0.0;

  const double lenSq = ScreenLengthSquared(projectedAxis);
  if (lenSq <= 1e-8)
    return 0.0;
  const double projectedPixels =
      Dot(mouseDx, mouseDy, projectedAxis) / std::sqrt(lenSq);
  return projectedPixels / projectedAxis.pixelsPerMeter;
}

// Intersects a world-space ray with a plane for deterministic drag projection.
std::optional<std::array<double, 3>>
IntersectRayWithPlane(const std::array<double, 3> &rayOrigin,
                      const std::array<double, 3> &rayDirection,
                      const std::array<double, 3> &planePoint,
                      const std::array<double, 3> &planeNormal) {
  const double denominator = rayDirection[0] * planeNormal[0] +
                             rayDirection[1] * planeNormal[1] +
                             rayDirection[2] * planeNormal[2];
  if (!std::isfinite(denominator) || std::abs(denominator) <= 1e-12)
    return std::nullopt;
  const double t = ((planePoint[0] - rayOrigin[0]) * planeNormal[0] +
                    (planePoint[1] - rayOrigin[1]) * planeNormal[1] +
                    (planePoint[2] - rayOrigin[2]) * planeNormal[2]) /
                   denominator;
  std::array<double, 3> result{rayOrigin[0] + rayDirection[0] * t,
                               rayOrigin[1] + rayDirection[1] * t,
                               rayOrigin[2] + rayDirection[2] * t};
  if (!std::isfinite(result[0]) || !std::isfinite(result[1]) ||
      !std::isfinite(result[2]))
    return std::nullopt;
  return result;
}

} // namespace viewer3d
