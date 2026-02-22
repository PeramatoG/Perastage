#include "symbols/SymbolGeometrySimplifier.h"

#include <cmath>
#include <cstddef>
#include <sstream>
#include <utility>

#include "geometry/RdpSimplifier.h"
#include "logger.h"

namespace symbols {
namespace {


bool NearEqual(const Point2D &a, const Point2D &b, float threshold = 1e-4f) {
  return std::fabs(a.x - b.x) <= threshold && std::fabs(a.y - b.y) <= threshold;
}

size_t CountPolylinePoints(const std::vector<Polyline2D> &polylines) {
  size_t total = 0;
  for (const auto &polyline : polylines)
    total += polyline.size();
  return total;
}

size_t CountPolygonPoints(const std::vector<PolygonWithHoles2D> &polygons) {
  size_t total = 0;
  for (const auto &polygon : polygons) {
    total += polygon.outer.size();
    for (const auto &hole : polygon.holes)
      total += hole.size();
  }
  return total;
}

void SimplifyPolygonRing(Polyline2D &ring, float epsilon) {
  const auto original = ring;
  Polyline2D simplified = geometry::SimplifyRdpPolygonClosed(ring, epsilon);

  const bool hasClosure =
      simplified.size() >= 2 && NearEqual(simplified.front(), simplified.back());
  const size_t uniqueVertices = hasClosure ? simplified.size() - 1 : simplified.size();
  if (uniqueVertices < 3) {
    ring = original;
    return;
  }

  ring = std::move(simplified);
}

} // namespace

void SimplifySymbolGeometry(Symbol2D &symbol, float epsilon) {
  const size_t beforePoints = CountPolylinePoints(symbol.strokes) +
                              CountPolygonPoints(symbol.fill);

  for (auto &polyline : symbol.strokes) {
    if (polyline.size() <= 2)
      continue;
    polyline = geometry::SimplifyRdpPolyline(polyline, epsilon);
  }

  for (auto &polygon : symbol.fill) {
    SimplifyPolygonRing(polygon.outer, epsilon);
    for (auto &hole : polygon.holes)
      SimplifyPolygonRing(hole, epsilon);
  }

  const size_t afterPoints = CountPolylinePoints(symbol.strokes) +
                             CountPolygonPoints(symbol.fill);

  if (beforePoints == 0)
    return;

  const double reduction =
      (1.0 - static_cast<double>(afterPoints) / static_cast<double>(beforePoints)) *
      100.0;

  std::ostringstream oss;
  oss << "Symbol RDP simplification view=" << static_cast<int>(symbol.view)
      << " points " << beforePoints << " -> " << afterPoints
      << " (" << reduction << "%)";
  Logger::Instance().Log(Logger::Level::Debug, oss.str());
}

} // namespace symbols
