#include "symbols/ContourTracer.h"

#include "symbols/PolylineSimplify.h"

#include <cmath>
#include <unordered_map>

namespace symbols {
namespace {
struct IntPoint {
  int x = 0;
  int y = 0;
  bool operator==(const IntPoint &o) const { return x == o.x && y == o.y; }
};

struct IntPointHash {
  size_t operator()(const IntPoint &p) const {
    return (static_cast<size_t>(p.x) << 32) ^ static_cast<size_t>(p.y);
  }
};

float SignedArea(const std::vector<Point2D> &poly) {
  float area = 0.0f;
  for (size_t i = 0; i < poly.size(); ++i) {
    const auto &a = poly[i];
    const auto &b = poly[(i + 1) % poly.size()];
    area += a.x * b.y - b.x * a.y;
  }
  return area * 0.5f;
}

bool PointInPolygon(const Point2D &p, const std::vector<Point2D> &poly) {
  bool inside = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    const auto &a = poly[i];
    const auto &b = poly[j];
    const bool intersect = ((a.y > p.y) != (b.y > p.y)) &&
                           (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) + 1e-6f) + a.x);
    if (intersect)
      inside = !inside;
  }
  return inside;
}
} // namespace

std::vector<PolygonWithHoles2D> TraceFillPolygons(const BinaryMask &mask,
                                                  int width,
                                                  int height,
                                                  float simplifyTolerance) {
  std::unordered_multimap<IntPoint, IntPoint, IntPointHash> edges;

  auto filled = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height)
      return false;
    return mask[y * width + x] != 0;
  };

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!filled(x, y))
        continue;
      if (!filled(x, y - 1))
        edges.emplace(IntPoint{x, y}, IntPoint{x + 1, y});
      if (!filled(x + 1, y))
        edges.emplace(IntPoint{x + 1, y}, IntPoint{x + 1, y + 1});
      if (!filled(x, y + 1))
        edges.emplace(IntPoint{x + 1, y + 1}, IntPoint{x, y + 1});
      if (!filled(x - 1, y))
        edges.emplace(IntPoint{x, y + 1}, IntPoint{x, y});
    }
  }

  std::vector<std::vector<Point2D>> loops;
  while (!edges.empty()) {
    auto it = edges.begin();
    IntPoint start = it->first;
    IntPoint current = it->second;
    edges.erase(it);

    std::vector<Point2D> loop;
    loop.push_back({static_cast<float>(start.x), static_cast<float>(start.y)});
    loop.push_back({static_cast<float>(current.x), static_cast<float>(current.y)});

    while (!(current == start)) {
      auto range = edges.equal_range(current);
      if (range.first == range.second)
        break;
      auto nextIt = range.first;
      IntPoint next = nextIt->second;
      edges.erase(nextIt);
      current = next;
      loop.push_back({static_cast<float>(current.x), static_cast<float>(current.y)});
      if (loop.size() > static_cast<size_t>(width * height * 2))
        break;
    }

    if (loop.size() >= 4) {
      auto simplified = SimplifyRdp(loop, simplifyTolerance, true);
      if (simplified.size() >= 4)
        loops.push_back(std::move(simplified));
    }
  }

  struct LoopData {
    std::vector<Point2D> points;
    bool outer = true;
  };
  std::vector<LoopData> loopData;
  loopData.reserve(loops.size());
  for (auto &loop : loops) {
    loopData.push_back({loop, SignedArea(loop) < 0.0f});
  }

  std::vector<PolygonWithHoles2D> polygons;
  for (auto &loop : loopData) {
    if (!loop.outer)
      continue;
    PolygonWithHoles2D poly;
    poly.outer = loop.points;
    polygons.push_back(std::move(poly));
  }

  for (auto &loop : loopData) {
    if (loop.outer)
      continue;
    if (loop.points.empty())
      continue;
    const Point2D probe = loop.points.front();
    for (auto &poly : polygons) {
      if (PointInPolygon(probe, poly.outer)) {
        poly.holes.push_back(loop.points);
        break;
      }
    }
  }

  return polygons;
}

} // namespace symbols
