#pragma once
#include "DocumentComposition.h"
#include <algorithm>
#include <cmath>
namespace layouts::composition {
// Top-left anchored uniform fit. Inputs are output/document page-size ratios.
// Composition always precedes mapping; unlike papers leave space at right/bottom.
struct PageOutputMapping {
  double scale = 1;
  Point Map(Point p) const { return {p.x * scale, p.y * scale}; }
  Rect Map(Rect r) const { return {r.x*scale, r.y*scale, r.width*scale, r.height*scale}; }
};
inline PageOutputMapping UniformPageFit(double ratioX, double ratioY) {
  if (!std::isfinite(ratioX) || !std::isfinite(ratioY) || ratioX <= 0 || ratioY <= 0)
    return {0};
  return {std::min(ratioX,ratioY)};
}
}
