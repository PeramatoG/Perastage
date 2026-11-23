#pragma once

#include "../fixture_label_composition.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace viewer2d {

// The orthographic label plane is axis-aligned in framebuffer space. Bottom
// view reverses the horizontal anchor axis, but glyphs still face the reader.
struct FixtureLabelScreenProjection {
  double originX = 0, originY = 0, scaleX = 0, scaleY = 0;

  std::array<double, 2> Project(double x, double y) const {
    return {originX + scaleX * x, originY + scaleY * y};
  }

  bool IntersectsViewport(const LabelBounds &ink, double padding,
                          int width, int height) const {
    const auto a = Project(ink.minX, ink.minY);
    const auto b = Project(ink.maxX, ink.maxY);
    if (!std::isfinite(a[0]) || !std::isfinite(a[1]) ||
        !std::isfinite(b[0]) || !std::isfinite(b[1]))
      return true;
    return std::max(a[0], b[0]) + padding >= 0 &&
           std::min(a[0], b[0]) - padding <= width &&
           std::max(a[1], b[1]) + padding >= 0 &&
           std::min(a[1], b[1]) - padding <= height;
  }
};

} // namespace viewer2d
