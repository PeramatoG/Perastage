#pragma once
#include <array>
#include <cmath>

namespace typography {
enum class TextInkPass { Both, Outline, Fill };
inline constexpr float kFixtureLabelOutlineReferenceMetres = 0.004f;

// Legacy halos extend one framebuffer pixel, independent of scene zoom.
// NanoVG uses framebuffer units in the viewer, logical units in the preview.
inline double ScreenOutlineStrokeWidth(double radiusMetres, double devicePixelRatio = 1) {
  if (!std::isfinite(radiusMetres) || radiusMetres <= 0 ||
      !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0) return 0;
  return 2 * (radiusMetres / kFixtureLabelOutlineReferenceMetres) / devicePixelRatio;
}

// Legacy physical export offsets; never apply screen compensation to these.
inline std::array<std::array<double, 2>, 8> OutlineOffsets(double radius) {
  return {{{-radius,0}, {radius,0}, {0,-radius}, {0,radius},
           {-radius,-radius}, {radius,-radius}, {-radius,radius}, {radius,radius}}};
}
} // namespace typography
