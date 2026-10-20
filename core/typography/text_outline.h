#pragma once
#include <array>

namespace typography {
enum class TextInkPass { Both, Outline, Fill };
// Radius uses the caller's units; screen and PDF apply only their unit mapping.
inline std::array<std::array<double, 2>, 8> OutlineOffsets(double radius) {
  return {{{-radius,0}, {radius,0}, {0,-radius}, {0,radius},
           {-radius,-radius}, {radius,-radius}, {-radius,radius}, {radius,radius}}};
}
} // namespace typography
