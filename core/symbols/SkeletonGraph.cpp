#include "symbols/SkeletonGraph.h"

#include "symbols/PolylineSimplify.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace symbols {
namespace {
using Edge = std::pair<int, int>;

Edge MakeEdge(int a, int b) {
  return a < b ? Edge{a, b} : Edge{b, a};
}

float PolylineLength(const std::vector<Point2D> &points, bool closed) {
  if (points.size() < 2)
    return 0.0f;
  float len = 0.0f;
  for (size_t i = 1; i < points.size(); ++i)
    len += std::hypot(points[i].x - points[i - 1].x, points[i].y - points[i - 1].y);
  if (closed)
    len += std::hypot(points.front().x - points.back().x, points.front().y - points.back().y);
  return len;
}
} // namespace

std::vector<Polyline2D> SkeletonToPolylines(const BinaryMask &skeleton,
                                            int width,
                                            int height,
                                            float minLength,
                                            float simplifyTolerance) {
  auto inBounds = [&](int x, int y) {
    return x >= 0 && y >= 0 && x < width && y < height;
  };
  auto id = [&](int x, int y) { return y * width + x; };

  std::vector<int> nodes;
  nodes.reserve(skeleton.size());
  std::vector<int> degree(skeleton.size(), 0);

  static const int kNeigh[8][2] = {{1, 0},  {1, 1},  {0, 1},  {-1, 1},
                                   {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (!skeleton[id(x, y)])
        continue;
      const int current = id(x, y);
      nodes.push_back(current);
      int d = 0;
      for (const auto &n : kNeigh) {
        const int nx = x + n[0];
        const int ny = y + n[1];
        if (inBounds(nx, ny) && skeleton[id(nx, ny)])
          ++d;
      }
      degree[current] = d;
    }
  }

  std::set<Edge> usedEdges;
  std::vector<Polyline2D> lines;

  auto buildPath = [&](int start, int next) {
    std::vector<Point2D> points;
    int prev = start;
    int cur = next;
    points.push_back({static_cast<float>(start % width) + 0.5f,
                      static_cast<float>(start / width) + 0.5f});

    while (true) {
      points.push_back({static_cast<float>(cur % width) + 0.5f,
                        static_cast<float>(cur / width) + 0.5f});
      usedEdges.insert(MakeEdge(prev, cur));

      if (degree[cur] != 2)
        break;

      int nextNode = -1;
      const int x = cur % width;
      const int y = cur / width;
      for (const auto &n : kNeigh) {
        const int nx = x + n[0];
        const int ny = y + n[1];
        if (!inBounds(nx, ny) || !skeleton[id(nx, ny)])
          continue;
        const int nid = id(nx, ny);
        if (nid == prev)
          continue;
        if (usedEdges.count(MakeEdge(cur, nid)))
          continue;
        nextNode = nid;
        break;
      }
      if (nextNode < 0)
        break;
      prev = cur;
      cur = nextNode;
      if (cur == start)
        break;
    }

    Polyline2D line;
    line.closed = points.size() > 2 &&
                  std::abs(points.front().x - points.back().x) < 0.01f &&
                  std::abs(points.front().y - points.back().y) < 0.01f;
    line.points = SimplifyRdp(points, simplifyTolerance, line.closed);
    if (PolylineLength(line.points, line.closed) >= minLength)
      lines.push_back(std::move(line));
  };

  for (int node : nodes) {
    if (degree[node] != 1 && degree[node] < 3)
      continue;
    const int x = node % width;
    const int y = node / width;
    for (const auto &n : kNeigh) {
      const int nx = x + n[0];
      const int ny = y + n[1];
      if (!inBounds(nx, ny) || !skeleton[id(nx, ny)])
        continue;
      const int nid = id(nx, ny);
      if (usedEdges.count(MakeEdge(node, nid)))
        continue;
      buildPath(node, nid);
    }
  }

  for (int node : nodes) {
    const int x = node % width;
    const int y = node / width;
    for (const auto &n : kNeigh) {
      const int nx = x + n[0];
      const int ny = y + n[1];
      if (!inBounds(nx, ny) || !skeleton[id(nx, ny)])
        continue;
      const int nid = id(nx, ny);
      if (usedEdges.count(MakeEdge(node, nid)))
        continue;
      buildPath(node, nid);
    }
  }

  return lines;
}

} // namespace symbols
