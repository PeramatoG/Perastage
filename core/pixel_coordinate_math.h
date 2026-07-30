#pragma once

#include <array>
#include <optional>

namespace pixel_coordinates {

// Converts logical pixels to rounded physical framebuffer pixels.
std::optional<std::array<int, 2>>
LogicalToFramebuffer(const std::array<double, 2> &logical, double scale);

// Converts physical framebuffer pixels to rounded logical pixels.
std::optional<std::array<int, 2>>
FramebufferToLogical(const std::array<double, 2> &framebuffer, double scale);

} // namespace pixel_coordinates
