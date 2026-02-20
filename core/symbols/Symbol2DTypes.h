#pragma once

#include <array>
#include <string>
#include <vector>

namespace symbols {

enum class SymbolView { Front, Top, Bottom, Left };

struct Point2D {
  float x = 0.0f;
  float y = 0.0f;
};

struct Polyline2D {
  std::vector<Point2D> points;
  bool closed = false;
};

struct PolygonWithHoles2D {
  std::vector<Point2D> outer;
  std::vector<std::vector<Point2D>> holes;
};

struct Aabb2D {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;

  bool IsValid() const { return maxX >= minX && maxY >= minY; }
  float Width() const { return maxX - minX; }
  float Height() const { return maxY - minY; }
};

struct Symbol2D {
  SymbolView view = SymbolView::Front;
  std::vector<PolygonWithHoles2D> fill;
  std::vector<Polyline2D> strokes;
  Aabb2D bounds;
  float stroke_width_px = 2.0f;
};

using SymbolCollection = std::vector<Symbol2D>;

const char *ToString(SymbolView view);
std::array<SymbolView, 4> AllSymbolViews();

} // namespace symbols
