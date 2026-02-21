#pragma once

#include <vector>

namespace symbols {

enum class SymbolView { Front, Top, Bottom, Left };

struct Point2D {
  float x = 0.0f;
  float y = 0.0f;
};

using Polyline2D = std::vector<Point2D>;

struct PolygonWithHoles2D {
  Polyline2D outer;
  std::vector<Polyline2D> holes;
};

struct Aabb2D {
  Point2D min{};
  Point2D max{};
  bool valid = false;
};

} // namespace symbols
