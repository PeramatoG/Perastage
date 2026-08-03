#include "geometry/RdpSimplifier.h"

#include <cmath>
#include <iostream>

namespace {

// Compares scalar coordinates with the geometry tolerance.
bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// Constructs a concise test point.
symbols::Point2D Pt(float x, float y) { return symbols::Point2D{x, y}; }

// Compares two point sequences exactly for deterministic geometry output.
bool EqualPoints(const std::vector<symbols::Point2D> &left,
                 const std::vector<symbols::Point2D> &right) {
  if (left.size() != right.size())
    return false;
  for (size_t index = 0; index < left.size(); ++index) {
    if (left[index].x != right[index].x || left[index].y != right[index].y)
      return false;
  }
  return true;
}

} // namespace

// Verifies open and closed RDP geometry contracts.
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
    const std::vector<symbols::Point2D> asymmetric = {
        Pt(0, 0), Pt(4, 0), Pt(8, 0), Pt(9, 1), Pt(9, 6),
        Pt(5, 6), Pt(5, 5), Pt(0, 6), Pt(0, 0)};
    const auto expected =
        geometry::SimplifyRdpPolygonClosed(asymmetric, 1.0f);
    const size_t uniqueCount = asymmetric.size() - 1;
    for (size_t rotation = 0; rotation < uniqueCount; ++rotation) {
      std::vector<symbols::Point2D> rotated;
      for (size_t offset = 0; offset < uniqueCount; ++offset)
        rotated.push_back(asymmetric[(rotation + offset) % uniqueCount]);
      rotated.push_back(rotated.front());
      const auto actual = geometry::SimplifyRdpPolygonClosed(rotated, 1.0f);
      if (!EqualPoints(expected, actual)) {
        std::cerr << "Closed RDP changed with cyclic input rotation " << rotation
                  << '\n';
        return 1;
      }
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
