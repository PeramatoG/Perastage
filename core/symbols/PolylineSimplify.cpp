#include "symbols/PolylineSimplify.h"

#include <cmath>

namespace symbols {
namespace {
float DistToSegment(const Point2D &p, const Point2D &a, const Point2D &b) {
  const float vx = b.x - a.x;
  const float vy = b.y - a.y;
  const float wx = p.x - a.x;
  const float wy = p.y - a.y;
  const float c1 = vx * wx + vy * wy;
  if (c1 <= 0.0f)
    return std::hypot(p.x - a.x, p.y - a.y);
  const float c2 = vx * vx + vy * vy;
  if (c2 <= c1)
    return std::hypot(p.x - b.x, p.y - b.y);
  const float t = c1 / c2;
  const float px = a.x + t * vx;
  const float py = a.y + t * vy;
  return std::hypot(p.x - px, p.y - py);
}

void SimplifyRec(const std::vector<Point2D> &in, int a, int b, float tol,
                 std::vector<uint8_t> &keep) {
  float maxDist = 0.0f;
  int idx = -1;
  for (int i = a + 1; i < b; ++i) {
    const float d = DistToSegment(in[i], in[a], in[b]);
    if (d > maxDist) {
      maxDist = d;
      idx = i;
    }
  }
  if (idx >= 0 && maxDist > tol) {
    keep[idx] = 1;
    SimplifyRec(in, a, idx, tol, keep);
    SimplifyRec(in, idx, b, tol, keep);
  }
}
} // namespace

std::vector<Point2D> SimplifyRdp(const std::vector<Point2D> &points,
                                 float tolerance,
                                 bool closed) {
  if (points.size() <= 3)
    return points;

  std::vector<Point2D> input = points;
  if (closed && !(points.front().x == points.back().x && points.front().y == points.back().y))
    input.push_back(points.front());

  if (input.size() <= 3)
    return input;

  std::vector<uint8_t> keep(input.size(), 0);
  keep.front() = keep.back() = 1;
  SimplifyRec(input, 0, static_cast<int>(input.size() - 1), tolerance, keep);

  std::vector<Point2D> out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (keep[i])
      out.push_back(input[i]);
  }

  if (closed && !out.empty()) {
    if (!(out.front().x == out.back().x && out.front().y == out.back().y))
      out.push_back(out.front());
  }
  return out;
}

} // namespace symbols
