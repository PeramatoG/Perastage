#pragma once
#include "DocumentComposition.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
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
    throw std::invalid_argument("page scale ratios must be finite and greater than zero");
  return {std::min(ratioX,ratioY)};
}
inline PageOutputMapping PageFitForSizes(double docW, double docH, double outW, double outH) {
  const auto valid = [](double v) { return std::isfinite(v) && v > 0; };
  if (!valid(docW) || !valid(docH))
    throw std::invalid_argument("document page dimensions must be finite and greater than zero");
  if (!valid(outW) || !valid(outH))
    throw std::invalid_argument("output paper dimensions must be finite and greater than zero");
  return UniformPageFit(outW/docW, outH/docH);
}
}
