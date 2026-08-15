#include "screen_line_projection.h"

#include <algorithm>
#include <cmath>

namespace viewer_common {
namespace {

// Interpolates one point along a finite world-space segment.
std::array<float, 3> Interpolate(const std::array<float, 3> &start,
                                 const std::array<float, 3> &end,
                                 float fraction) {
  std::array<float, 3> point{};
  for (int axis = 0; axis < 3; ++axis)
    point[axis] = start[axis] + (end[axis] - start[axis]) * fraction;
  return point;
}

} // namespace

// Projects a screen pointer onto a finite world-space line segment.
std::optional<std::array<float, 3>>
ProjectPointerOntoScreenLine(const std::array<float, 3> &worldStart,
                             const std::array<float, 3> &worldEnd,
                             const std::array<float, 2> &screenStart,
                             const std::array<float, 2> &screenEnd,
                             const std::array<float, 2> &screenPointer) {
  const float dx = screenEnd[0] - screenStart[0];
  const float dy = screenEnd[1] - screenStart[1];
  const bool horizontalDominant = std::fabs(dx) >= std::fabs(dy);
  const float dominantLength = horizontalDominant ? dx : dy;
  if (std::fabs(dominantLength) <= 0.1f)
    return std::nullopt;
  const float pointerCoordinate =
      horizontalDominant ? screenPointer[0] : screenPointer[1];
  const float startCoordinate =
      horizontalDominant ? screenStart[0] : screenStart[1];
  const float fraction = std::clamp(
      (pointerCoordinate - startCoordinate) / dominantLength, 0.0f, 1.0f);
  std::array<float, 3> projected{};
  for (int axis = 0; axis < 3; ++axis)
    projected[axis] =
        worldStart[axis] + (worldEnd[axis] - worldStart[axis]) * fraction;
  return projected;
}

// Projects a pointer onto a perspective-projected world-space line segment.
std::optional<std::array<float, 3>> ProjectPointerOntoPerspectiveLine(
    const std::array<float, 3> &worldStart,
    const std::array<float, 3> &worldEnd,
    const std::array<float, 2> &screenPointer,
    const WorldToScreenProjection &projectWorldToScreen) {
  const auto screenStart = projectWorldToScreen(worldStart);
  const auto screenEnd = projectWorldToScreen(worldEnd);
  if (!screenStart || !screenEnd)
    return std::nullopt;
  const float dx = (*screenEnd)[0] - (*screenStart)[0];
  const float dy = (*screenEnd)[1] - (*screenStart)[1];
  const int coordinate = std::fabs(dx) >= std::fabs(dy) ? 0 : 1;
  const float startCoordinate = (*screenStart)[coordinate];
  const float endCoordinate = (*screenEnd)[coordinate];
  if (std::fabs(endCoordinate - startCoordinate) <= 0.1f)
    return std::nullopt;
  const float target = std::clamp(screenPointer[coordinate],
                                  std::min(startCoordinate, endCoordinate),
                                  std::max(startCoordinate, endCoordinate));
  float low = 0.0f;
  float high = 1.0f;
  const bool increasing = endCoordinate > startCoordinate;
  for (int iteration = 0; iteration < 24; ++iteration) {
    const float middle = (low + high) * 0.5f;
    const auto projected =
        projectWorldToScreen(Interpolate(worldStart, worldEnd, middle));
    if (!projected)
      return std::nullopt;
    if (((*projected)[coordinate] < target) == increasing)
      low = middle;
    else
      high = middle;
  }
  return Interpolate(worldStart, worldEnd, (low + high) * 0.5f);
}

} // namespace viewer_common
