#pragma once

#include <array>
#include <optional>

namespace viewer3d {

// Projects a screen pointer onto a finite world-space line segment.
std::optional<std::array<float, 3>>
ProjectPointerOntoScreenLine(const std::array<float, 3> &worldStart,
                             const std::array<float, 3> &worldEnd,
                             const std::array<float, 2> &screenStart,
                             const std::array<float, 2> &screenEnd,
                             const std::array<float, 2> &screenPointer);

} // namespace viewer3d
