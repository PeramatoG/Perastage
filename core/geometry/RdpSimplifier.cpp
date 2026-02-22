#include "geometry/RdpSimplifier.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stack>

namespace geometry {
namespace {

constexpr float kDedupThreshold = 1e-4f;

bool NearEqual(const symbols::Point2D &a, const symbols::Point2D &b,
               float threshold = kDedupThreshold) {
  return std::fabs(a.x - b.x) <= threshold && std::fabs(a.y - b.y) <= threshold;
}

std::vector<symbols::Point2D>
RemoveConsecutiveDuplicates(const std::vector<symbols::Point2D> &pts) {
  std::vector<symbols::Point2D> cleaned;
  cleaned.reserve(pts.size());
  for (const auto &point : pts) {
    if (cleaned.empty() || !NearEqual(cleaned.back(), point))
      cleaned.push_back(point);
  }
  return cleaned;
}

float DistanceSquaredToSegment(const symbols::Point2D &point,
                               const symbols::Point2D &segmentStart,
                               const symbols::Point2D &segmentEnd) {
  const float dx = segmentEnd.x - segmentStart.x;
  const float dy = segmentEnd.y - segmentStart.y;
  const float segLenSq = dx * dx + dy * dy;
  if (segLenSq <= std::numeric_limits<float>::epsilon()) {
    const float px = point.x - segmentStart.x;
    const float py = point.y - segmentStart.y;
    return px * px + py * py;
  }

  const float t = ((point.x - segmentStart.x) * dx +
                   (point.y - segmentStart.y) * dy) /
                  segLenSq;
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  const float projX = segmentStart.x + clamped * dx;
  const float projY = segmentStart.y + clamped * dy;
  const float diffX = point.x - projX;
  const float diffY = point.y - projY;
  return diffX * diffX + diffY * diffY;
}

std::vector<symbols::Point2D>
SimplifyOpenPolyline(const std::vector<symbols::Point2D> &pts, float epsilon) {
  if (pts.size() <= 2)
    return pts;

  const float epsilonSq = epsilon * epsilon;
  std::vector<unsigned char> keep(pts.size(), 0);
  keep.front() = 1;
  keep.back() = 1;

  std::stack<std::pair<size_t, size_t>> ranges;
  ranges.push({0, pts.size() - 1});

  while (!ranges.empty()) {
    const auto [start, end] = ranges.top();
    ranges.pop();
    if (end <= start + 1)
      continue;

    float maxDistanceSq = -1.0f;
    size_t splitIndex = start;
    for (size_t i = start + 1; i < end; ++i) {
      const float distanceSq = DistanceSquaredToSegment(pts[i], pts[start], pts[end]);
      if (distanceSq > maxDistanceSq) {
        maxDistanceSq = distanceSq;
        splitIndex = i;
      }
    }

    if (maxDistanceSq > epsilonSq) {
      keep[splitIndex] = 1;
      ranges.push({start, splitIndex});
      ranges.push({splitIndex, end});
    }
  }

  std::vector<symbols::Point2D> simplified;
  simplified.reserve(pts.size());
  for (size_t i = 0; i < pts.size(); ++i) {
    if (keep[i])
      simplified.push_back(pts[i]);
  }

  if (simplified.size() < 2)
    return pts;
  return simplified;
}

size_t CountUniqueVertices(const std::vector<symbols::Point2D> &pts) {
  std::vector<symbols::Point2D> uniques;
  uniques.reserve(pts.size());
  for (const auto &point : pts) {
    bool exists = false;
    for (const auto &u : uniques) {
      if (NearEqual(u, point)) {
        exists = true;
        break;
      }
    }
    if (!exists)
      uniques.push_back(point);
  }
  return uniques.size();
}

} // namespace

std::vector<symbols::Point2D>
SimplifyRdpPolyline(const std::vector<symbols::Point2D> &pts, float epsilon) {
  if (pts.size() <= 2)
    return pts;

  const auto cleaned = RemoveConsecutiveDuplicates(pts);
  if (cleaned.size() <= 2)
    return cleaned;

  return SimplifyOpenPolyline(cleaned, epsilon);
}

std::vector<symbols::Point2D>
SimplifyRdpPolygonClosed(const std::vector<symbols::Point2D> &pts, float epsilon) {
  if (pts.size() < 3)
    return pts;

  std::vector<symbols::Point2D> ring = RemoveConsecutiveDuplicates(pts);
  if (ring.size() < 3)
    return pts;

  const bool inputExplicitlyClosed = NearEqual(ring.front(), ring.back());
  if (inputExplicitlyClosed)
    ring.pop_back();

  if (ring.size() < 3)
    return pts;

  std::vector<symbols::Point2D> duplicated = ring;
  duplicated.push_back(ring.front());
  const auto simplifiedOpen = SimplifyOpenPolyline(duplicated, epsilon);
  if (simplifiedOpen.size() < 2)
    return pts;

  std::vector<symbols::Point2D> simplified = simplifiedOpen;
  if (NearEqual(simplified.front(), simplified.back()))
    simplified.pop_back();

  if (CountUniqueVertices(simplified) < 3)
    return pts;

  if (inputExplicitlyClosed)
    simplified.push_back(simplified.front());
  return simplified;
}

} // namespace geometry
