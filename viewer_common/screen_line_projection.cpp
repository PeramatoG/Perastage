#include "screen_line_projection.h"

#include <algorithm>
#include <cmath>

namespace viewer_common {

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
  const float fraction =
      std::clamp((pointerCoordinate - startCoordinate) / dominantLength, 0.0f,
                 1.0f);
  std::array<float, 3> projected{};
  for (int axis = 0; axis < 3; ++axis)
    projected[axis] =
        worldStart[axis] + (worldEnd[axis] - worldStart[axis]) * fraction;
  return projected;
}

} // namespace viewer_common
