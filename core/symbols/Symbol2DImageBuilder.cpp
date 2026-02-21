#include "symbols/Symbol2DImageBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <queue>
#include <set>
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

int PixelIndex(const PixelMask &mask, int x, int y) {
  return y * mask.width + x;
}

Point2D ToPoint(int x, int y, int imageHeight) {
  return Point2D{static_cast<float>(x), static_cast<float>(imageHeight - 1 - y)};
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

  const size_t pixelCount = static_cast<size_t>(render.width) *
                            static_cast<size_t>(render.height);
  for (size_t i = 0; i < pixelCount; ++i) {
    const unsigned char r = render.rgba[i * 4 + 0];
    const unsigned char g = render.rgba[i * 4 + 1];
    const unsigned char b = render.rgba[i * 4 + 2];
    const unsigned char a = render.rgba[i * 4 + 3];

    const bool visible = a > params.fillAlphaThreshold;
    const bool nonWhite = !(r > params.whiteThreshold && g > params.whiteThreshold &&
                            b > params.whiteThreshold);
    if (visible && nonWhite)
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
    if (a > params.fillAlphaThreshold && luminance < params.blackThreshold)
      mask.value[i] = 1;
  }
  return mask;
}

Polyline2D TraceOuterPolygon(const PixelMask &fillMask) {
  int minX = fillMask.width;
  int minY = fillMask.height;
  int maxX = -1;
  int maxY = -1;

  for (int y = 0; y < fillMask.height; ++y) {
    for (int x = 0; x < fillMask.width; ++x) {
      if (!fillMask.Get(x, y))
        continue;
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }

  Polyline2D outer;
  if (maxX < minX || maxY < minY)
    return outer;

  outer.push_back(ToPoint(minX, minY, fillMask.height));
  outer.push_back(ToPoint(maxX + 1, minY, fillMask.height));
  outer.push_back(ToPoint(maxX + 1, maxY + 1, fillMask.height));
  outer.push_back(ToPoint(minX, maxY + 1, fillMask.height));
  return outer;
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

    Polyline2D fillPolygon = TraceOuterPolygon(fillMask);
    if (fillPolygon.size() >= 3)
      symbol.fill.push_back(PolygonWithHoles2D{fillPolygon, {}});

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
