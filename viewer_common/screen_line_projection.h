#pragma once

#include <array>
#include <functional>
#include <optional>

namespace viewer_common {

// Projects a screen pointer onto a finite world-space line segment.
std::optional<std::array<float, 3>>
ProjectPointerOntoScreenLine(const std::array<float, 3> &worldStart,
                             const std::array<float, 3> &worldEnd,
                             const std::array<float, 2> &screenStart,
                             const std::array<float, 2> &screenEnd,
                             const std::array<float, 2> &screenPointer);

using WorldToScreenProjection =
    std::function<std::optional<std::array<float, 2>>(
        const std::array<float, 3> &)>;

// Projects a pointer onto a perspective-projected world-space line segment.
std::optional<std::array<float, 3>> ProjectPointerOntoPerspectiveLine(
    const std::array<float, 3> &worldStart,
    const std::array<float, 3> &worldEnd,
    const std::array<float, 2> &screenPointer,
    const WorldToScreenProjection &projectWorldToScreen);

} // namespace viewer_common
