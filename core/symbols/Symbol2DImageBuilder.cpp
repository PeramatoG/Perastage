#include "symbols/Symbol2DImageBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iterator>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <limits>
#include <unordered_map>

#include "geometry/RdpSimplifier.h"

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

  bool operator<(const GridPoint &other) const {
    return x < other.x || (x == other.x && y < other.y);
  }
};

struct DirectedGridEdge {
  GridPoint from;
  GridPoint to;
  bool consumed = false;
};

// Returns a stable direction index in clockwise image-grid order.
int GridDirection(const GridPoint &from, const GridPoint &to) {
  if (to.x > from.x)
    return 0;
  if (to.y > from.y)
    return 1;
  if (to.x < from.x)
    return 2;
  return 3;
}

// Ranks continuations while keeping the filled region on the traversal's right.
int ContinuationRank(const DirectedGridEdge &incoming,
                     const DirectedGridEdge &outgoing) {
  const int turn = (GridDirection(outgoing.from, outgoing.to) -
                    GridDirection(incoming.from, incoming.to) + 4) % 4;
  static constexpr std::array<int, 4> rank = {1, 0, 3, 2};
  return rank[static_cast<size_t>(turn)];
}

// Compares implicit closed rings by their full canonical point sequence.
bool RingLess(const Polyline2D &a, const Polyline2D &b) {
  return std::lexicographical_compare(
      a.begin(), a.end(), b.begin(), b.end(),
      [](const Point2D &left, const Point2D &right) {
        return left.x < right.x || (left.x == right.x && left.y < right.y);
      });
}

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

Point2D FindInteriorProbePoint(const Polyline2D &polygon) {
  if (polygon.empty())
    return {};

  float minX = polygon.front().x;
  float minY = polygon.front().y;
  float maxX = polygon.front().x;
  float maxY = polygon.front().y;
  for (const auto &p : polygon) {
    minX = std::min(minX, p.x);
    minY = std::min(minY, p.y);
    maxX = std::max(maxX, p.x);
    maxY = std::max(maxY, p.y);
  }

  const int x0 = static_cast<int>(std::floor(minX));
  const int x1 = static_cast<int>(std::ceil(maxX));
  const int y0 = static_cast<int>(std::floor(minY));
  const int y1 = static_cast<int>(std::ceil(maxY));

  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const Point2D candidate{static_cast<float>(x) + 0.5f,
                              static_cast<float>(y) + 0.5f};
      if (PointInPolygon(candidate, polygon))
        return candidate;
    }
  }

  return PolygonProbePoint(polygon);
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

void CloseMaskGaps(PixelMask &mask, int maxGapPixels) {
  if (maxGapPixels <= 0 || mask.width <= 0 || mask.height <= 0)
    return;

  const auto closeDirectionalRuns = [&](int dx, int dy) {
    std::vector<std::pair<int, int>> toFill;
    for (int y = 0; y < mask.height; ++y) {
      for (int x = 0; x < mask.width; ++x) {
        const int px = x - dx;
        const int py = y - dy;
        if (mask.InBounds(px, py))
          continue;

        int cx = x;
        int cy = y;
        int distanceFromFilled = -1;
        std::vector<std::pair<int, int>> gapCells;
        while (mask.InBounds(cx, cy)) {
          if (mask.Get(cx, cy)) {
            if (distanceFromFilled >= 0 && distanceFromFilled <= maxGapPixels)
              toFill.insert(toFill.end(), gapCells.begin(), gapCells.end());
            distanceFromFilled = 0;
            gapCells.clear();
          } else if (distanceFromFilled >= 0) {
            ++distanceFromFilled;
            if (distanceFromFilled <= maxGapPixels)
              gapCells.emplace_back(cx, cy);
            else
              gapCells.clear();
          }
          cx += dx;
          cy += dy;
        }
      }
    }

    for (const auto &[x, y] : toFill)
      mask.Get(x, y) = 1;
  };

  closeDirectionalRuns(1, 0);
  closeDirectionalRuns(0, 1);
  closeDirectionalRuns(1, 1);
  closeDirectionalRuns(1, -1);
}

Polyline2D BuildClosedStrokeFromRing(const Polyline2D &ring) {
  if (ring.size() < 3)
    return {};
  Polyline2D closed = ring;
  closed.push_back(ring.front());
  return closed;
}

void AddFillBoundaryStrokeFallback(Symbol2D &symbol) {
  const auto squaredDistance = [](const Point2D &a, const Point2D &b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
  };

  constexpr float kRingCoverageTolerancePx = 1.5f;
  constexpr float kBoundaryOverlapThreshold = 0.85f;

  std::vector<Polyline2D> boundaryStrokes;
  for (const auto &polygon : symbol.fill) {
    Polyline2D outerStroke = BuildClosedStrokeFromRing(polygon.outer);
    if (!outerStroke.empty())
      boundaryStrokes.push_back(std::move(outerStroke));
    for (const auto &hole : polygon.holes) {
      Polyline2D holeStroke = BuildClosedStrokeFromRing(hole);
      if (!holeStroke.empty())
        boundaryStrokes.push_back(std::move(holeStroke));
    }
  }

  if (boundaryStrokes.empty())
    return;

  const auto strokeBoundaryOverlap = [&](const Polyline2D &stroke) {
    if (stroke.empty())
      return 0.0f;
    int overlappingPoints = 0;
    const float toleranceSq = kRingCoverageTolerancePx * kRingCoverageTolerancePx;
    for (const auto &strokePoint : stroke) {
      bool overlaps = false;
      for (const auto &boundary : boundaryStrokes) {
        for (const auto &boundaryPoint : boundary) {
          if (squaredDistance(strokePoint, boundaryPoint) <= toleranceSq) {
            overlaps = true;
            break;
          }
        }
        if (overlaps)
          break;
      }
      if (overlaps)
        ++overlappingPoints;
    }
    return static_cast<float>(overlappingPoints) /
           static_cast<float>(stroke.size());
  };

  std::vector<Polyline2D> filteredStrokes;
  filteredStrokes.reserve(symbol.strokes.size() + boundaryStrokes.size());
  for (auto &stroke : symbol.strokes) {
    const float overlap = strokeBoundaryOverlap(stroke);
    if (overlap >= kBoundaryOverlapThreshold)
      continue;
    filteredStrokes.push_back(std::move(stroke));
  }

  filteredStrokes.insert(filteredStrokes.end(),
                         std::make_move_iterator(boundaryStrokes.begin()),
                         std::make_move_iterator(boundaryStrokes.end()));
  symbol.strokes = std::move(filteredStrokes);
}

std::vector<PolygonWithHoles2D> ExtractFillPolygons(const PixelMask &fillMask) {
  const int w = fillMask.width;
  const int h = fillMask.height;
  std::vector<PolygonWithHoles2D> result;
  if (w <= 0 || h <= 0)
    return result;

  std::vector<DirectedGridEdge> edges;
  auto isFilledAt = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h)
      return false;
    return fillMask.Get(x, y) != 0;
  };
  auto addEdge = [&edges](GridPoint a, GridPoint b) {
    edges.push_back({a, b, false});
  };

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

  std::sort(edges.begin(), edges.end(), [](const auto &left, const auto &right) {
    if (left.from < right.from)
      return true;
    if (right.from < left.from)
      return false;
    return left.to < right.to;
  });
  std::map<GridPoint, std::vector<size_t>> outgoingEdges;
  for (size_t index = 0; index < edges.size(); ++index)
    outgoingEdges[edges[index].from].push_back(index);

  std::vector<Polyline2D> loops;
  const size_t maxSteps = edges.size();
  for (size_t startEdge = 0; startEdge < edges.size(); ++startEdge) {
    if (edges[startEdge].consumed)
      continue;
    const GridPoint start = edges[startEdge].from;
    GridPoint current = start;
    GridPoint next = edges[startEdge].to;
    edges[startEdge].consumed = true;
    size_t incomingIndex = startEdge;

    Polyline2D loop;
    loop.push_back(ToVertexPoint(start.x, start.y, h));
    loop.push_back(ToVertexPoint(next.x, next.y, h));
    current = next;

    for (size_t step = 0; step < maxSteps; ++step) {
      if (current == start)
        break;
      const auto outgoing = outgoingEdges.find(current);
      if (outgoing == outgoingEdges.end())
        break;

      std::optional<size_t> selected;
      for (size_t candidateIndex : outgoing->second) {
        if (edges[candidateIndex].consumed)
          continue;
        if (!selected ||
            ContinuationRank(edges[incomingIndex], edges[candidateIndex]) <
                ContinuationRank(edges[incomingIndex], edges[*selected]) ||
            (ContinuationRank(edges[incomingIndex], edges[candidateIndex]) ==
                 ContinuationRank(edges[incomingIndex], edges[*selected]) &&
             edges[candidateIndex].to < edges[*selected].to)) {
          selected = candidateIndex;
        }
      }
      if (!selected)
        break;
      edges[*selected].consumed = true;
      const GridPoint candidate = edges[*selected].to;

      loop.push_back(ToVertexPoint(candidate.x, candidate.y, h));
      current = candidate;
      incomingIndex = *selected;
    }

    if (loop.size() >= 2 && loop.front().x == loop.back().x &&
        loop.front().y == loop.back().y) {
      loop.pop_back();
    }
    if (current == start && loop.size() >= 3)
      loops.push_back(geometry::CanonicalizePolygonRing(loop));
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
  for (auto &loop : loops) {
    const float areaAbs = std::abs(SignedArea(loop));
    infos.push_back(LoopInfo{std::move(loop), areaAbs});
  }

  for (size_t i = 0; i < infos.size(); ++i) {
    const Point2D probe = FindInteriorProbePoint(infos[i].polygon);
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

  std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
    const float leftArea = std::abs(SignedArea(left.outer));
    const float rightArea = std::abs(SignedArea(right.outer));
    if (leftArea != rightArea)
      return leftArea > rightArea;
    return RingLess(left.outer, right.outer);
  });
  for (auto &polygon : result)
    std::sort(polygon.holes.begin(), polygon.holes.end(), RingLess);

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

// Converts one rendered RGBA image into one 2D symbol without shared mutable state.
std::optional<Symbol2D> Symbol2DImageBuilder::BuildFromRenderedImage(
    const RenderedSymbolImage &render, const ImageBuildParams &params) {
  const size_t expectedBytes = static_cast<size_t>(render.width) *
                               static_cast<size_t>(render.height) * 4;
  if (render.width <= 0 || render.height <= 0 || render.rgba.size() < expectedBytes)
    return std::nullopt;

  PixelMask fillMask = BuildFillMask(render, params);
  PixelMask lineMask = BuildLineMask(render, params);
  CloseMaskGaps(fillMask, params.fillGapClosurePixels);
  CloseMaskGaps(lineMask, params.lineGapClosurePixels);
  ThinZhangSuen(lineMask);

  Symbol2D symbol;
  symbol.view = render.view;
  symbol.strokeWidthPx = params.previewStrokeWidthPx;

  symbol.fill = ExtractFillPolygons(fillMask);
  symbol.strokes = ExtractPolylines(lineMask, params);
  AddFillBoundaryStrokeFallback(symbol);

  for (const auto &polygon : symbol.fill)
    for (const auto &p : polygon.outer)
      ExtendBounds(symbol.bounds, p);
  for (const auto &line : symbol.strokes)
    for (const auto &p : line)
      ExtendBounds(symbol.bounds, p);

  return symbol;
}

// Converts rendered images sequentially for deterministic tests and fallback.
std::vector<Symbol2D> Symbol2DImageBuilder::BuildFromRenderedImagesSequential(
    const std::vector<RenderedSymbolImage> &renders, const ImageBuildParams &params) {
  std::vector<Symbol2D> symbols;
  symbols.reserve(renders.size());
  for (const auto &render : renders) {
    auto symbol = BuildFromRenderedImage(render, params);
    if (symbol)
      symbols.push_back(std::move(*symbol));
  }
  return symbols;
}

// Converts rendered images in bounded parallel tasks while preserving input order.
std::vector<Symbol2D> Symbol2DImageBuilder::BuildFromRenderedImages(
    const std::vector<RenderedSymbolImage> &renders, const ImageBuildParams &params) {
  const unsigned int hardware = std::thread::hardware_concurrency();
  if (renders.size() < 2 || hardware < 2)
    return BuildFromRenderedImagesSequential(renders, params);

  const size_t workerCount =
      std::min(renders.size(), static_cast<size_t>(std::max(2u, hardware)));
  std::vector<std::optional<Symbol2D>> ordered(renders.size());
  std::vector<std::future<void>> futures;
  futures.reserve(workerCount);

  for (size_t worker = 0; worker < workerCount; ++worker) {
    futures.push_back(std::async(std::launch::async, [&, worker]() {
      for (size_t index = worker; index < renders.size(); index += workerCount)
        ordered[index] = BuildFromRenderedImage(renders[index], params);
    }));
  }

  for (auto &future : futures)
    future.get();

  std::vector<Symbol2D> symbols;
  symbols.reserve(renders.size());
  for (auto &symbol : ordered) {
    if (symbol)
      symbols.push_back(std::move(*symbol));
  }
  return symbols;
}

} // namespace symbols
