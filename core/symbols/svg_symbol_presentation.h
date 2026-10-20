#pragma once
#include "PerastageSvgSymbol.h"
#include <algorithm>
#include <array>

namespace symbols {
// Reuses the symbol builder's centre/top-hang convention, including hole geometry.
inline std::array<double, 2> SvgPresentationAnchor(const PerastageSvgSymbolData &svg) {
  double minX = 0, minY = 0, maxX = 0, maxY = 0;
  bool first = true;
  const auto include = [&](const auto &points) {
    for (const auto &p : points) {
      const double x = p.x + svg.offsetXmm, y = p.y + svg.offsetYmm;
      if (first) {
        minX = maxX = x;
        minY = maxY = y;
        first = false;
      } else {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  };
  for (const auto &fill : svg.fills) {
    include(fill.points);
    for (const auto &hole : fill.holes) include(hole);
  }
  for (const auto &stroke : svg.strokes) include(stroke.points);
  const bool hang = svg.viewKind == SymbolViewKind::Front || svg.viewKind == SymbolViewKind::Left;
  return {(minX + maxX) / 2, hang ? maxY : (minY + maxY) / 2};
}
// Every target paints all fills before contour ink.
template <class Fill, class Stroke>
void VisitSvgPresentation(const PerastageSvgSymbolData &svg, Fill fill, Stroke stroke) {
  for (const auto &polygon : svg.fills) fill(polygon);
  for (const auto &line : svg.strokes) stroke(line);
}
} // namespace symbols
