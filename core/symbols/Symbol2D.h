#pragma once

#include "Symbol2DTypes.h"

namespace symbols {

struct Symbol2D {
  SymbolView view = SymbolView::Top;
  std::vector<PolygonWithHoles2D> fill;
  std::vector<Polyline2D> strokes;
  Aabb2D bounds{};
  float strokeWidthPx = 2.0f;
};

} // namespace symbols
