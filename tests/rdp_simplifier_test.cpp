#include "geometry/RdpSimplifier.h"

#include <cmath>
#include <iostream>

namespace {

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

symbols::Point2D Pt(float x, float y) { return symbols::Point2D{x, y}; }

} // namespace

int main() {
  {
    std::vector<symbols::Point2D> polyline;
    for (int i = 0; i <= 20; ++i)
      polyline.push_back(Pt(static_cast<float>(i), 0.05f * std::sin(i * 0.2f)));

    auto simplified = geometry::SimplifyRdpPolyline(polyline, 1.0f);
    if (simplified.size() != 2) {
      std::cerr << "Polyline simplification did not collapse to endpoints\n";
      return 1;
    }
    if (!Near(simplified.front().x, 0.0f) || !Near(simplified.back().x, 20.0f)) {
      std::cerr << "Polyline simplification changed endpoints\n";
      return 1;
    }
  }

  {
    std::vector<symbols::Point2D> rectangle = {
        Pt(0, 0), Pt(5, 0), Pt(10, 0), Pt(10, 4), Pt(10, 8), Pt(5, 8),
        Pt(0, 8), Pt(0, 4), Pt(0, 0),
    };

    auto simplified = geometry::SimplifyRdpPolygonClosed(rectangle, 1.0f);
    if (simplified.size() != 5) {
      std::cerr << "Rectangle simplification should keep 4 corners and closure\n";
      return 1;
    }
    if (!Near(simplified.front().x, simplified.back().x) ||
        !Near(simplified.front().y, simplified.back().y)) {
      std::cerr << "Rectangle output is not closed\n";
      return 1;
    }
  }

  {
    std::vector<symbols::Point2D> tinyTriangle = {
        Pt(0.0f, 0.0f), Pt(0.1f, 0.0f), Pt(0.0f, 0.1f), Pt(0.0f, 0.0f)};

    auto simplified = geometry::SimplifyRdpPolygonClosed(tinyTriangle, 1.0f);
    if (simplified.size() != tinyTriangle.size()) {
      std::cerr << "Tiny polygon should keep original geometry\n";
      return 1;
    }
  }

  return 0;
}
