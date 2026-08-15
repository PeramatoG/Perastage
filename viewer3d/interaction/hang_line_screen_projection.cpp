#include "hang_line_screen_projection.h"

#include <algorithm>

namespace viewer3d {

// Projects a screen pointer onto a finite world-space line segment.
std::optional<std::array<float, 3>>
ProjectPointerOntoScreenLine(const std::array<float, 3> &worldStart,
                             const std::array<float, 3> &worldEnd,
                             const std::array<float, 2> &screenStart,
                             const std::array<float, 2> &screenEnd,
                             const std::array<float, 2> &screenPointer) {
  const float dx = screenEnd[0] - screenStart[0];
  const float dy = screenEnd[1] - screenStart[1];
  const float lengthSquared = dx * dx + dy * dy;
  if (lengthSquared <= 0.01f)
    return std::nullopt;
  const float fraction = std::clamp(((screenPointer[0] - screenStart[0]) * dx +
                                     (screenPointer[1] - screenStart[1]) * dy) /
                                        lengthSquared,
                                    0.0f, 1.0f);
  std::array<float, 3> projected{};
  for (int axis = 0; axis < 3; ++axis)
    projected[axis] =
        worldStart[axis] + (worldEnd[axis] - worldStart[axis]) * fraction;
  return projected;
}

} // namespace viewer3d
