#include "pixel_coordinate_math.h"

#include <cmath>
#include <limits>

namespace pixel_coordinates {
namespace {

// Rounds a finite scaled coordinate when it is representable as an integer.
std::optional<int> RoundCoordinate(double value) {
  if (!std::isfinite(value) ||
      value < static_cast<double>(std::numeric_limits<int>::min()) ||
      value > static_cast<double>(std::numeric_limits<int>::max()))
    return std::nullopt;
  return static_cast<int>(std::lround(value));
}

// Converts two coordinates using a validated content scale.
std::optional<std::array<int, 2>> Convert(const std::array<double, 2> &point,
                                          double scale) {
  if (!std::isfinite(scale) || scale <= 0.0)
    return std::nullopt;
  const auto x = RoundCoordinate(point[0] * scale);
  const auto y = RoundCoordinate(point[1] * scale);
  if (!x || !y)
    return std::nullopt;
  return std::array<int, 2>{*x, *y};
}

} // namespace

// Converts logical pixels to rounded physical framebuffer pixels.
std::optional<std::array<int, 2>>
LogicalToFramebuffer(const std::array<double, 2> &logical, double scale) {
  return Convert(logical, scale);
}

// Converts physical framebuffer pixels to rounded logical pixels.
std::optional<std::array<int, 2>>
FramebufferToLogical(const std::array<double, 2> &framebuffer, double scale) {
  if (!std::isfinite(scale) || scale <= 0.0)
    return std::nullopt;
  return Convert(framebuffer, 1.0 / scale);
}

} // namespace pixel_coordinates
