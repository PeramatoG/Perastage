#include "symbols/Symbol2DImageBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <set>
#include <limits>
#include <unordered_map>

namespace symbols {
namespace {

struct PixelMask {
  int width = 0;
  int height = 0;
  std::vector<unsigned char> value;

  bool InBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height;
  }

  unsigned char Get(int x, int y) const {
    return value[static_cast<size_t>(y) * static_cast<size_t>(width) +
                 static_cast<size_t>(x)];
  }

  unsigned char &Get(int x, int y) {
    return value[static_cast<size_t>(y) * static_cast<size_t>(width) +
                 static_cast<size_t>(x)];
  }
};

struct RgbColor {
  unsigned char r = 255;
  unsigned char g = 255;
  unsigned char b = 255;
};

struct GridPoint {
  int x = 0;
  int y = 0;

  bool operator==(const GridPoint &other) const {
    return x == other.x && y == other.y;
  }
};

struct GridPointHash {
  size_t operator()(const GridPoint &p) const {
    return (static_cast<size_t>(static_cast<uint32_t>(p.x)) << 32) ^
           static_cast<size_t>(static_cast<uint32_t>(p.y));
  }
};

float SignedArea(const Polyline2D &polyline) {
  if (polyline.size() < 3)
    return 0.0f;
  double area = 0.0;
  for (size_t i = 0; i < polyline.size(); ++i) {
    const Point2D &a = polyline[i];
    const Point2D &b = polyline[(i + 1) % polyline.size()];
    area += static_cast<double>(a.x) * static_cast<double>(b.y) -
            static_cast<double>(b.x) * static_cast<double>(a.y);
  }
  return static_cast<float>(0.5 * area);
}

bool PointInPolygon(const Point2D &point, const Polyline2D &polygon) {
  if (polygon.size() < 3)
    return false;
  bool inside = false;
  for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const Point2D &pi = polygon[i];
    const Point2D &pj = polygon[j];
    const bool intersect =
        ((pi.y > point.y) != (pj.y > point.y)) &&
        (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y + 1e-6f) + pi.x);
    if (intersect)
      inside = !inside;
  }
  return inside;
}

Point2D PolygonProbePoint(const Polyline2D &polygon) {
  Point2D probe{};
  if (polygon.empty())
    return probe;
  for (const auto &p : polygon) {
    probe.x += p.x;
    probe.y += p.y;
  }
  const float inv = 1.0f / static_cast<float>(polygon.size());
  probe.x *= inv;
  probe.y *= inv;
  return probe;
}

int PixelIndex(const PixelMask &mask, int x, int y) {
  return y * mask.width + x;
}

Point2D ToPoint(int x, int y, int imageHeight) {
  return Point2D{static_cast<float>(x), static_cast<float>(imageHeight - 1 - y)};
}

Point2D ToVertexPoint(int x, int y, int imageHeight) {
  return Point2D{static_cast<float>(x), static_cast<float>(imageHeight - y)};
}

void ExtendBounds(Aabb2D &bounds, const Point2D &p) {
  if (!bounds.valid) {
    bounds.min = p;
    bounds.max = p;
    bounds.valid = true;
    return;
  }
  bounds.min.x = std::min(bounds.min.x, p.x);
  bounds.min.y = std::min(bounds.min.y, p.y);
  bounds.max.x = std::max(bounds.max.x, p.x);
  bounds.max.y = std::max(bounds.max.y, p.y);
}

PixelMask BuildFillMask(const RenderedSymbolImage &render,
                        const ImageBuildParams &params) {
  PixelMask mask;
  mask.width = render.width;
  mask.height = render.height;
  mask.value.assign(static_cast<size_t>(render.width) * static_cast<size_t>(render.height),
                    0);

  const auto pixelRgb = [&](int x, int y) {
    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
                        static_cast<size_t>(x)) *
                       4;
    return RgbColor{render.rgba[idx + 0], render.rgba[idx + 1], render.rgba[idx + 2]};
  };

  const auto isNear = [&](const RgbColor &a, const RgbColor &b) {
    return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <=
               params.backgroundTolerance &&
           std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <=
               params.backgroundTolerance &&
           std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <=
               params.backgroundTolerance;
  };

  std::array<RgbColor, 4> corners = {
      pixelRgb(0, 0), pixelRgb(render.width - 1, 0),
      pixelRgb(0, render.height - 1), pixelRgb(render.width - 1, render.height - 1)};

  RgbColor background = corners[0];
  int bestCount = -1;
  for (const auto &candidate : corners) {
    int count = 0;
    for (const auto &sample : corners) {
      if (isNear(candidate, sample))
        ++count;
    }
    if (count > bestCount) {
      bestCount = count;
      background = candidate;
    }
  }

  const size_t pixelCount = static_cast<size_t>(render.width) *
                            static_cast<size_t>(render.height);
  for (size_t i = 0; i < pixelCount; ++i) {
    const unsigned char r = render.rgba[i * 4 + 0];
    const unsigned char g = render.rgba[i * 4 + 1];
    const unsigned char b = render.rgba[i * 4 + 2];
    const unsigned char a = render.rgba[i * 4 + 3];

    const bool visible = a > params.fillAlphaThreshold;
    const RgbColor current{r, g, b};
    const bool isBackgroundColor = isNear(current, background);
    if (visible && !isBackgroundColor)
      mask.value[i] = 1;
  }
  return mask;
}

PixelMask BuildLineMask(const RenderedSymbolImage &render,
                        const ImageBuildParams &params) {
  PixelMask mask;
  mask.width = render.width;
  mask.height = render.height;
  mask.value.assign(static_cast<size_t>(render.width) * static_cast<size_t>(render.height),
                    0);

  const size_t pixelCount = static_cast<size_t>(render.width) *
                            static_cast<size_t>(render.height);
  for (size_t i = 0; i < pixelCount; ++i) {
    const unsigned char r = render.rgba[i * 4 + 0];
    const unsigned char g = render.rgba[i * 4 + 1];
    const unsigned char b = render.rgba[i * 4 + 2];
    const unsigned char a = render.rgba[i * 4 + 3];
    const int luminance = static_cast<int>(0.2126f * static_cast<float>(r) +
                                           0.7152f * static_cast<float>(g) +
                                           0.0722f * static_cast<float>(b));
    if (a > params.lineAlphaThreshold && luminance < params.blackThreshold)
      mask.value[i] = 1;
  }
  return mask;
}

std::vector<PolygonWithHoles2D> ExtractFillPolygons(const PixelMask &fillMask) {
  const int w = fillMask.width;
  const int h = fillMask.height;
  std::vector<PolygonWithHoles2D> result;
  if (w <= 0 || h <= 0)
    return result;

  std::unordered_multimap<GridPoint, GridPoint, GridPointHash> edges;
  auto isFilledAt = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h)
      return false;
    return fillMask.Get(x, y) != 0;
  };
  auto addEdge = [&edges](GridPoint a, GridPoint b) { edges.emplace(a, b); };

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!fillMask.Get(x, y))
        continue;
      if (!isFilledAt(x, y - 1))
        addEdge(GridPoint{x, y}, GridPoint{x + 1, y});
      if (!isFilledAt(x + 1, y))
        addEdge(GridPoint{x + 1, y}, GridPoint{x + 1, y + 1});
      if (!isFilledAt(x, y + 1))
        addEdge(GridPoint{x + 1, y + 1}, GridPoint{x, y + 1});
      if (!isFilledAt(x - 1, y))
        addEdge(GridPoint{x, y + 1}, GridPoint{x, y});
    }
  }

  if (edges.empty())
    return result;

  std::vector<Polyline2D> loops;
  const size_t maxSteps = static_cast<size_t>(w) * static_cast<size_t>(h) * 8;
  while (!edges.empty()) {
    auto currentIt = edges.begin();
    GridPoint start = currentIt->first;
    GridPoint current = start;
    GridPoint next = currentIt->second;
    edges.erase(currentIt);

    Polyline2D loop;
    loop.push_back(ToVertexPoint(start.x, start.y, h));
    loop.push_back(ToVertexPoint(next.x, next.y, h));
    current = next;

    for (size_t step = 0; step < maxSteps; ++step) {
      auto range = edges.equal_range(current);
      if (range.first == range.second)
        break;

      auto take = range.first;
      GridPoint candidate = take->second;
      edges.erase(take);

      loop.push_back(ToVertexPoint(candidate.x, candidate.y, h));
      current = candidate;
      if (current == start)
        break;
    }

    if (loop.size() >= 2 && loop.front().x == loop.back().x &&
        loop.front().y == loop.back().y) {
      loop.pop_back();
    }
    if (loop.size() >= 3)
      loops.push_back(std::move(loop));
  }

  if (loops.empty())
    return result;

  struct LoopInfo {
    Polyline2D polygon;
    float areaAbs = 0.0f;
    int depth = 0;
    bool isOuter = true;
    int ownerOuter = -1;
  };
  std::vector<LoopInfo> infos;
  infos.reserve(loops.size());
  for (auto &loop : loops)
    infos.push_back(LoopInfo{std::move(loop), std::abs(SignedArea(loop))});

  for (size_t i = 0; i < infos.size(); ++i) {
    const Point2D probe = PolygonProbePoint(infos[i].polygon);
    int depth = 0;
    int owner = -1;
    float ownerArea = std::numeric_limits<float>::max();
    for (size_t j = 0; j < infos.size(); ++j) {
      if (i == j)
        continue;
      if (!PointInPolygon(probe, infos[j].polygon))
        continue;
      ++depth;
      if (infos[j].areaAbs < ownerArea) {
        ownerArea = infos[j].areaAbs;
        owner = static_cast<int>(j);
      }
    }
    infos[i].depth = depth;
    infos[i].isOuter = (depth % 2 == 0);
    infos[i].ownerOuter = owner;
  }

  bool anyOuter = false;
  for (const auto& info : infos) {
      if (info.isOuter) {
          anyOuter = true;
          break;
      }
  }

  // Fallback: if nesting classification produced no outer loops, treat the largest
  // loop as outer. This avoids losing fill when probe-point classification fails.
  if (!anyOuter && !infos.empty()) {
      size_t best = 0;
      for (size_t k = 1; k < infos.size(); ++k) {
          if (infos[k].areaAbs > infos[best].areaAbs)
              best = k;
      }
      infos[best].isOuter = true;
      infos[best].ownerOuter = -1;
      infos[best].depth = 0;
  }

  std::unordered_map<int, int> outerMap;
  for (size_t i = 0; i < infos.size(); ++i) {
    if (!infos[i].isOuter)
      continue;
    outerMap[static_cast<int>(i)] = static_cast<int>(result.size());
    result.push_back(PolygonWithHoles2D{infos[i].polygon, {}});
  }

  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].isOuter)
      continue;

    int owner = infos[i].ownerOuter;
    int safety = 0;
    while (owner >= 0 && !infos[static_cast<size_t>(owner)].isOuter) {
        const int next = infos[static_cast<size_t>(owner)].ownerOuter;

        // Guard against cyclic ownership chains caused by degenerate polygon nesting.
        if (next == owner || ++safety > static_cast<int>(infos.size())) {
            owner = -1;
            break;
        }

        owner = next;
    }
    auto ownerIt = outerMap.find(owner);
    if (ownerIt != outerMap.end())
      result[static_cast<size_t>(ownerIt->second)].holes.push_back(infos[i].polygon);
  }

  return result;
}

int CountNeighbors(const PixelMask &mask, int x, int y) {
  int count = 0;
  for (int oy = -1; oy <= 1; ++oy) {
    for (int ox = -1; ox <= 1; ++ox) {
      if (ox == 0 && oy == 0)
        continue;
      if (mask.InBounds(x + ox, y + oy) && mask.Get(x + ox, y + oy))
        ++count;
    }
  }
  return count;
}

int CountTransitions(const PixelMask &mask, int x, int y) {
  static constexpr std::array<std::pair<int, int>, 8> neighbors = {
      {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};
  int transitions = 0;
  for (size_t i = 0; i < neighbors.size(); ++i) {
    const auto [x0, y0] = neighbors[i];
    const auto [x1, y1] = neighbors[(i + 1) % neighbors.size()];
    const unsigned char a =
        mask.InBounds(x + x0, y + y0) ? mask.Get(x + x0, y + y0) : 0;
    const unsigned char b =
        mask.InBounds(x + x1, y + y1) ? mask.Get(x + x1, y + y1) : 0;
    if (a == 0 && b == 1)
      ++transitions;
  }
  return transitions;
}

void ThinZhangSuen(PixelMask &mask) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (int phase = 0; phase < 2; ++phase) {
      std::vector<std::pair<int, int>> toClear;
      for (int y = 1; y < mask.height - 1; ++y) {
        for (int x = 1; x < mask.width - 1; ++x) {
          if (!mask.Get(x, y))
            continue;
          const int n = CountNeighbors(mask, x, y);
          if (n < 2 || n > 6)
            continue;
          if (CountTransitions(mask, x, y) != 1)
            continue;

          const int p2 = mask.Get(x, y - 1);
          const int p4 = mask.Get(x + 1, y);
          const int p6 = mask.Get(x, y + 1);
          const int p8 = mask.Get(x - 1, y);

          bool shouldDelete = false;
          if (phase == 0)
            shouldDelete = (p2 * p4 * p6 == 0) && (p4 * p6 * p8 == 0);
          else
            shouldDelete = (p2 * p4 * p8 == 0) && (p2 * p6 * p8 == 0);

          if (shouldDelete)
            toClear.emplace_back(x, y);
        }
      }

      if (!toClear.empty()) {
        changed = true;
        for (const auto &[x, y] : toClear)
          mask.Get(x, y) = 0;
      }
    }
  }
}

std::vector<Polyline2D> ExtractPolylines(const PixelMask &skeleton,
                                         const ImageBuildParams &params) {
  const int w = skeleton.width;
  const int h = skeleton.height;
  std::vector<std::vector<int>> adjacency(static_cast<size_t>(w * h));
  std::vector<int> activeNodes;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!skeleton.Get(x, y))
        continue;
      const int id = PixelIndex(skeleton, x, y);
      activeNodes.push_back(id);
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          if (ox == 0 && oy == 0)
            continue;
          const int nx = x + ox;
          const int ny = y + oy;
          if (!skeleton.InBounds(nx, ny) || !skeleton.Get(nx, ny))
            continue;
          adjacency[static_cast<size_t>(id)].push_back(PixelIndex(skeleton, nx, ny));
        }
      }
    }
  }

  std::set<std::pair<int, int>> visitedEdges;
  auto markEdge = [&visitedEdges](int a, int b) {
    if (a > b)
      std::swap(a, b);
    visitedEdges.insert({a, b});
  };
  auto edgeVisited = [&visitedEdges](int a, int b) {
    if (a > b)
      std::swap(a, b);
    return visitedEdges.find({a, b}) != visitedEdges.end();
  };

  auto nodeToPoint = [w, h](int node) {
    const int x = node % w;
    const int y = node / w;
    return ToPoint(x, y, h);
  };

  auto walkFrom = [&](int start, int next) {
    Polyline2D polyline;
    polyline.push_back(nodeToPoint(start));

    int prev = start;
    int current = next;
    markEdge(prev, current);

    while (true) {
      polyline.push_back(nodeToPoint(current));
      const auto &neighbors = adjacency[static_cast<size_t>(current)];
      const int degree = static_cast<int>(neighbors.size());
      if (degree != 2)
        break;

      int candidate = -1;
      for (int n : neighbors) {
        if (n == prev)
          continue;
        candidate = n;
        break;
      }
      if (candidate < 0 || edgeVisited(current, candidate))
        break;
      prev = current;
      current = candidate;
      markEdge(prev, current);
    }

    return polyline;
  };

  std::vector<Polyline2D> polylines;

  for (int node : activeNodes) {
    const auto &neighbors = adjacency[static_cast<size_t>(node)];
    if (neighbors.size() != 1)
      continue;
    for (int next : neighbors) {
      if (edgeVisited(node, next))
        continue;
      Polyline2D polyline = walkFrom(node, next);
      if (static_cast<int>(polyline.size()) >= params.minStrokePixels)
        polylines.push_back(std::move(polyline));
    }
  }

  for (int node : activeNodes) {
    for (int next : adjacency[static_cast<size_t>(node)]) {
      if (edgeVisited(node, next))
        continue;
      Polyline2D polyline = walkFrom(node, next);
      if (static_cast<int>(polyline.size()) >= params.minStrokePixels)
        polylines.push_back(std::move(polyline));
    }
  }

  return polylines;
}

} // namespace

std::vector<Symbol2D>
Symbol2DImageBuilder::BuildFromRenderedImages(
    const std::vector<RenderedSymbolImage> &renders, const ImageBuildParams &params) {
  std::vector<Symbol2D> symbols;
  symbols.reserve(renders.size());

  for (const auto &render : renders) {
    const size_t expectedBytes = static_cast<size_t>(render.width) *
                                 static_cast<size_t>(render.height) * 4;
    if (render.width <= 0 || render.height <= 0 || render.rgba.size() < expectedBytes)
      continue;

    PixelMask fillMask = BuildFillMask(render, params);
    PixelMask lineMask = BuildLineMask(render, params);
    ThinZhangSuen(lineMask);

    Symbol2D symbol;
    symbol.view = render.view;
    symbol.strokeWidthPx = params.previewStrokeWidthPx;

    symbol.fill = ExtractFillPolygons(fillMask);

    symbol.strokes = ExtractPolylines(lineMask, params);

    for (const auto &polygon : symbol.fill)
      for (const auto &p : polygon.outer)
        ExtendBounds(symbol.bounds, p);
    for (const auto &line : symbol.strokes)
      for (const auto &p : line)
        ExtendBounds(symbol.bounds, p);

    symbols.push_back(std::move(symbol));
  }

  return symbols;
}

} // namespace symbols
