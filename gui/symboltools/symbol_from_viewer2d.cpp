#include "symboltools/symbol_from_viewer2d.h"

#include "canvas2d.h"
#include "configmanager.h"
#include "legendsymbolcapture.h"
#include "symbolcache.h"
#include "symbols/ContourTracer.h"
#include "symbols/MaskUtils.h"
#include "symbols/PolylineSimplify.h"
#include "symbols/SkeletonGraph.h"
#include "symbols/Skeletonize.h"
#include "viewer2dpanel.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <type_traits>

namespace symboltools {
namespace {

// Keep this moderate to avoid UI stalls when skeletonizing on the main thread.
constexpr int kRenderResolution = 384;
constexpr float kPadding = 24.0f;
constexpr int kMaxProcessingResolution = 384;

struct RasterTransform {
  float scale = 1.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

struct Bounds {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  bool valid = false;
};

struct CanvasState {
  float scale = 1.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

symbols::Point2D ApplyCanvasState(const CanvasState &state, float x, float y) {
  return {x * state.scale + state.offsetX, y * state.scale + state.offsetY};
}

SymbolViewKind ToViewKind(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return SymbolViewKind::Front;
  case symbols::SymbolView::Top:
    return SymbolViewKind::Top;
  case symbols::SymbolView::Bottom:
    return SymbolViewKind::Bottom;
  case symbols::SymbolView::Left:
    return SymbolViewKind::Left;
  }
  return SymbolViewKind::Top;
}

void ExpandBounds(Bounds &b, float x, float y) {
  if (!b.valid) {
    b.minX = b.maxX = x;
    b.minY = b.maxY = y;
    b.valid = true;
    return;
  }
  b.minX = std::min(b.minX, x);
  b.minY = std::min(b.minY, y);
  b.maxX = std::max(b.maxX, x);
  b.maxY = std::max(b.maxY, y);
}

Bounds ComputeBounds(const CommandBuffer &buffer) {
  Bounds b;
  CanvasState current;
  std::vector<CanvasState> stack;
  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      const auto p0 = ApplyCanvasState(current, line->x0, line->y0);
      const auto p1 = ApplyCanvasState(current, line->x1, line->y1);
      ExpandBounds(b, p0.x, p0.y);
      ExpandBounds(b, p1.x, p1.y);
    } else if (const auto *poly = std::get_if<PolylineCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        const auto p = ApplyCanvasState(current, poly->points[i], poly->points[i + 1]);
        ExpandBounds(b, p.x, p.y);
      }
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        const auto p = ApplyCanvasState(current, poly->points[i], poly->points[i + 1]);
        ExpandBounds(b, p.x, p.y);
      }
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      const auto p0 = ApplyCanvasState(current, rect->x, rect->y);
      const auto p1 = ApplyCanvasState(current, rect->x + rect->w, rect->y + rect->h);
      ExpandBounds(b, p0.x, p0.y);
      ExpandBounds(b, p1.x, p1.y);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      const float radius = circle->radius * current.scale;
      const auto c = ApplyCanvasState(current, circle->cx, circle->cy);
      ExpandBounds(b, c.x - radius, c.y - radius);
      ExpandBounds(b, c.x + radius, c.y + radius);
    } else if (std::get_if<SaveCommand>(&cmd)) {
      stack.push_back(current);
    } else if (std::get_if<RestoreCommand>(&cmd)) {
      if (!stack.empty()) {
        current = stack.back();
        stack.pop_back();
      }
    } else if (const auto *tf = std::get_if<TransformCommand>(&cmd)) {
      current.scale = tf->transform.scale;
      current.offsetX = tf->transform.offsetX;
      current.offsetY = tf->transform.offsetY;
    }
  }
  return b;
}

RasterTransform BuildTransform(const Bounds &b, int width, int height) {
  RasterTransform t;
  const float spanX = std::max(1.0f, b.maxX - b.minX);
  const float spanY = std::max(1.0f, b.maxY - b.minY);
  const float sx = (static_cast<float>(width) - 2.0f * kPadding) / spanX;
  const float sy = (static_cast<float>(height) - 2.0f * kPadding) / spanY;
  t.scale = std::max(0.001f, std::min(sx, sy));

  const float drawW = spanX * t.scale;
  const float drawH = spanY * t.scale;
  t.offsetX = (static_cast<float>(width) - drawW) * 0.5f - b.minX * t.scale;
  t.offsetY = (static_cast<float>(height) - drawH) * 0.5f - b.minY * t.scale;
  return t;
}

symbols::Point2D ToImage(const RasterTransform &t, float x, float y,
                         int height) {
  return {x * t.scale + t.offsetX,
          static_cast<float>(height) - (y * t.scale + t.offsetY)};
}

void SetPixel(symbols::ImageRGBA &img, int x, int y, uint8_t r, uint8_t g,
              uint8_t b, uint8_t a) {
  if (x < 0 || y < 0 || x >= img.width || y >= img.height)
    return;
  auto *px = img.Pixel(x, y);
  px[0] = r;
  px[1] = g;
  px[2] = b;
  px[3] = a;
}

void DrawLine(symbols::ImageRGBA &img, symbols::Point2D a, symbols::Point2D b,
              int thickness, uint8_t r, uint8_t g, uint8_t bl, uint8_t alpha) {
  int x0 = static_cast<int>(std::lround(a.x));
  int y0 = static_cast<int>(std::lround(a.y));
  int x1 = static_cast<int>(std::lround(b.x));
  int y1 = static_cast<int>(std::lround(b.y));

  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    for (int oy = -thickness; oy <= thickness; ++oy)
      for (int ox = -thickness; ox <= thickness; ++ox)
        SetPixel(img, x0 + ox, y0 + oy, r, g, bl, alpha);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void FillPolygon(symbols::ImageRGBA &img, const std::vector<symbols::Point2D> &pts,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
  if (pts.size() < 3)
    return;

  float minY = pts.front().y;
  float maxY = pts.front().y;
  for (const auto &p : pts) {
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }

  for (int y = static_cast<int>(std::floor(minY)); y <= static_cast<int>(std::ceil(maxY)); ++y) {
    std::vector<float> nodes;
    for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
      const auto &a = pts[i];
      const auto &bpt = pts[j];
      if (((a.y < y && bpt.y >= y) || (bpt.y < y && a.y >= y)) &&
          std::abs(bpt.y - a.y) > 1e-6f) {
        nodes.push_back(a.x + (y - a.y) / (bpt.y - a.y) * (bpt.x - a.x));
      }
    }
    std::sort(nodes.begin(), nodes.end());
    for (size_t i = 0; i + 1 < nodes.size(); i += 2) {
      int xStart = static_cast<int>(std::floor(nodes[i]));
      int xEnd = static_cast<int>(std::ceil(nodes[i + 1]));
      for (int x = xStart; x <= xEnd; ++x)
        SetPixel(img, x, y, r, g, b, alpha);
    }
  }
}

std::vector<symbols::Point2D> BuildCircle(const RasterTransform &t, int h,
                                          float cx, float cy, float radius) {
  constexpr int kSegments = 48;
  constexpr float kTwoPi = 6.28318530717958647692f;
  std::vector<symbols::Point2D> pts;
  pts.reserve(kSegments);
  for (int i = 0; i < kSegments; ++i) {
    const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
    pts.push_back(ToImage(t, cx + std::cos(a) * radius, cy + std::sin(a) * radius, h));
  }
  return pts;
}

void RasterizeSymbol(const SymbolDefinition &def, symbols::ImageRGBA &shapeImg,
                     symbols::ImageRGBA &lineImg) {
  shapeImg.width = kRenderResolution;
  shapeImg.height = kRenderResolution;
  shapeImg.pixels.assign(static_cast<size_t>(kRenderResolution * kRenderResolution * 4), 0);
  lineImg = shapeImg;

  Bounds b = ComputeBounds(def.localCommands);
  if (!b.valid)
    return;
  const auto tf = BuildTransform(b, shapeImg.width, shapeImg.height);
  auto renderPass = [&](symbols::ImageRGBA &img, bool drawStrokes,
                        bool drawFills, uint8_t fillGray) {
    CanvasState current;
    std::vector<CanvasState> stack;

    auto hasStroke = [&](size_t index) {
      if (index >= def.localCommands.metadata.size())
        return true;
      return def.localCommands.metadata[index].hasStroke;
    };
    auto hasFill = [&](size_t index) {
      if (index >= def.localCommands.metadata.size())
        return true;
      return def.localCommands.metadata[index].hasFill;
    };

    auto drawCommand = [&](const CanvasCommand &cmd, bool commandDrawStrokes,
                           bool commandDrawFills) {
      if (const auto *line = std::get_if<LineCommand>(&cmd)) {
        const auto p0 = ApplyCanvasState(current, line->x0, line->y0);
        const auto p1 = ApplyCanvasState(current, line->x1, line->y1);
        const auto a = ToImage(tf, p0.x, p0.y, img.height);
        const auto c = ToImage(tf, p1.x, p1.y, img.height);
        const int thick = std::clamp(
            static_cast<int>(std::ceil(line->stroke.width * current.scale * tf.scale * 0.25f)),
            1, 2);
        if (commandDrawStrokes)
          DrawLine(img, a, c, thick, 0, 0, 0, 255);
      } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
        if (!commandDrawStrokes || polyline->points.size() < 4)
          return;
        const int thick = std::clamp(
            static_cast<int>(std::ceil(polyline->stroke.width * current.scale * tf.scale * 0.25f)),
            1, 2);
        for (size_t i = 0; i + 3 < polyline->points.size(); i += 2) {
          const auto p0 =
              ApplyCanvasState(current, polyline->points[i], polyline->points[i + 1]);
          const auto p1 =
              ApplyCanvasState(current, polyline->points[i + 2], polyline->points[i + 3]);
          const auto a = ToImage(tf, p0.x, p0.y, img.height);
          const auto c = ToImage(tf, p1.x, p1.y, img.height);
          DrawLine(img, a, c, thick, 0, 0, 0, 255);
        }
      } else if (const auto *polygon = std::get_if<PolygonCommand>(&cmd)) {
        std::vector<symbols::Point2D> pts;
        pts.reserve(polygon->points.size() / 2);
        for (size_t i = 0; i + 1 < polygon->points.size(); i += 2) {
          const auto p =
              ApplyCanvasState(current, polygon->points[i], polygon->points[i + 1]);
          pts.push_back(ToImage(tf, p.x, p.y, img.height));
        }
        if (commandDrawFills && polygon->hasFill)
          FillPolygon(img, pts, fillGray, fillGray, fillGray, 255);
        if (commandDrawStrokes) {
          const int thick = std::clamp(
              static_cast<int>(std::ceil(polygon->stroke.width * current.scale * tf.scale * 0.25f)),
              1, 2);
          for (size_t i = 0; i < pts.size(); ++i)
            DrawLine(img, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
        }
      } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
        const auto p0 = ApplyCanvasState(current, rect->x, rect->y);
        const auto p1 = ApplyCanvasState(current, rect->x + rect->w, rect->y);
        const auto p2 = ApplyCanvasState(current, rect->x + rect->w, rect->y + rect->h);
        const auto p3 = ApplyCanvasState(current, rect->x, rect->y + rect->h);
        std::vector<symbols::Point2D> pts = {
            ToImage(tf, p0.x, p0.y, img.height), ToImage(tf, p1.x, p1.y, img.height),
            ToImage(tf, p2.x, p2.y, img.height), ToImage(tf, p3.x, p3.y, img.height)};
        if (commandDrawFills && rect->hasFill)
          FillPolygon(img, pts, fillGray, fillGray, fillGray, 255);
        if (commandDrawStrokes) {
          const int thick = std::clamp(
              static_cast<int>(std::ceil(rect->stroke.width * current.scale * tf.scale * 0.25f)),
              1, 2);
          for (size_t i = 0; i < pts.size(); ++i)
            DrawLine(img, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
        }
      } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
        const auto center = ApplyCanvasState(current, circle->cx, circle->cy);
        const float radius = circle->radius * current.scale;
        auto pts = BuildCircle(tf, img.height, center.x, center.y, radius);
        if (commandDrawFills && circle->hasFill)
          FillPolygon(img, pts, fillGray, fillGray, fillGray, 255);
        if (commandDrawStrokes) {
          const int thick = std::clamp(
              static_cast<int>(std::ceil(circle->stroke.width * current.scale * tf.scale * 0.25f)),
              1, 2);
          for (size_t i = 0; i < pts.size(); ++i)
            DrawLine(img, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
        }
      } else if (std::get_if<SaveCommand>(&cmd)) {
        stack.push_back(current);
      } else if (std::get_if<RestoreCommand>(&cmd)) {
        if (!stack.empty()) {
          current = stack.back();
          stack.pop_back();
        }
      } else if (const auto *transform = std::get_if<TransformCommand>(&cmd)) {
        current.scale = transform->transform.scale;
        current.offsetX = transform->transform.offsetX;
        current.offsetY = transform->transform.offsetY;
      }
    };

    std::vector<size_t> groupedIndices;
    std::string currentSource;
    auto flushGroup = [&]() {
      for (const auto index : groupedIndices) {
        const bool commandDrawStrokes = drawStrokes && hasStroke(index);
        const bool commandDrawFills = drawFills && hasFill(index);
        drawCommand(def.localCommands.commands[index], commandDrawStrokes,
                    commandDrawFills);
      }
      groupedIndices.clear();
    };

    for (size_t i = 0; i < def.localCommands.commands.size(); ++i) {
      const auto &cmd = def.localCommands.commands[i];
      const bool isBarrier = std::visit(
          [](auto &&c) {
            using T = std::decay_t<decltype(c)>;
            return std::is_same_v<T, SaveCommand> || std::is_same_v<T, RestoreCommand> ||
                   std::is_same_v<T, TransformCommand> ||
                   std::is_same_v<T, BeginSymbolCommand> ||
                   std::is_same_v<T, EndSymbolCommand> ||
                   std::is_same_v<T, PlaceSymbolCommand> ||
                   std::is_same_v<T, SymbolInstanceCommand> ||
                   std::is_same_v<T, TextCommand>;
          },
          cmd);

      if (isBarrier) {
        flushGroup();
        drawCommand(cmd, false, false);
        continue;
      }

      if (groupedIndices.empty() && i < def.localCommands.sources.size())
        currentSource = def.localCommands.sources[i];
      if (i < def.localCommands.sources.size() &&
          def.localCommands.sources[i] != currentSource) {
        flushGroup();
        currentSource = def.localCommands.sources[i];
      }
      groupedIndices.push_back(i);
    }

    flushGroup();
  };

  renderPass(shapeImg, true, true, 0);
  renderPass(lineImg, true, true, 255);
}

symbols::ImageRGBA ResizeNearest(const symbols::ImageRGBA &src, int targetW,
                                int targetH) {
  if (src.width <= 0 || src.height <= 0 || src.pixels.empty() || targetW <= 0 ||
      targetH <= 0) {
    return {};
  }
  if (src.width == targetW && src.height == targetH)
    return src;

  symbols::ImageRGBA dst;
  dst.width = targetW;
  dst.height = targetH;
  dst.pixels.assign(static_cast<size_t>(targetW * targetH * 4), 0);

  for (int y = 0; y < targetH; ++y) {
    const int srcY = std::clamp((y * src.height) / targetH, 0, src.height - 1);
    for (int x = 0; x < targetW; ++x) {
      const int srcX = std::clamp((x * src.width) / targetW, 0, src.width - 1);
      const size_t srcIdx = static_cast<size_t>((srcY * src.width + srcX) * 4);
      const size_t dstIdx = static_cast<size_t>((y * targetW + x) * 4);
      dst.pixels[dstIdx] = src.pixels[srcIdx];
      dst.pixels[dstIdx + 1] = src.pixels[srcIdx + 1];
      dst.pixels[dstIdx + 2] = src.pixels[srcIdx + 2];
      dst.pixels[dstIdx + 3] = src.pixels[srcIdx + 3];
    }
  }

  return dst;
}

symbols::ImageRGBA BuildProcessingImage(const symbols::ImageRGBA &src) {
  if (src.width <= kMaxProcessingResolution &&
      src.height <= kMaxProcessingResolution) {
    return src;
  }

  const float scale = std::min(
      static_cast<float>(kMaxProcessingResolution) / static_cast<float>(src.width),
      static_cast<float>(kMaxProcessingResolution) / static_cast<float>(src.height));
  const int targetW = std::max(1, static_cast<int>(std::lround(src.width * scale)));
  const int targetH = std::max(1, static_cast<int>(std::lround(src.height * scale)));
  return ResizeNearest(src, targetW, targetH);
}

symbols::Aabb2D ComputeBoundsFromGeometry(const symbols::Symbol2D &symbol) {
  symbols::Aabb2D bounds{};
  bool hasBounds = false;
  auto addPoint = [&](const symbols::Point2D &p) {
    if (!hasBounds) {
      bounds.minX = bounds.maxX = p.x;
      bounds.minY = bounds.maxY = p.y;
      hasBounds = true;
      return;
    }
    bounds.minX = std::min(bounds.minX, p.x);
    bounds.minY = std::min(bounds.minY, p.y);
    bounds.maxX = std::max(bounds.maxX, p.x);
    bounds.maxY = std::max(bounds.maxY, p.y);
  };

  for (const auto &poly : symbol.fill) {
    for (const auto &p : poly.outer)
      addPoint(p);
    for (const auto &hole : poly.holes)
      for (const auto &p : hole)
        addPoint(p);
  }
  for (const auto &stroke : symbol.strokes)
    for (const auto &p : stroke.points)
      addPoint(p);

  if (!hasBounds)
    return symbols::Aabb2D{};
  return bounds;
}

} // namespace

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      ConfigManager &configManager,
                                      const std::string &fixtureUuid,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines,
                                      std::vector<SymbolReferenceViews> &outReferences) {
  (void)fixtureUuid;
  outSymbols.clear();
  outLogLines.clear();
  outReferences.clear();

  const auto previousMode = panel.GetRenderMode();
  panel.SetRenderMode(Viewer2DRenderMode::White);

  const std::vector<Viewer2DView> captureViews = {
      Viewer2DView::Top, Viewer2DView::Front, Viewer2DView::Bottom,
      Viewer2DView::Side};
  const auto snapshot =
      CaptureSymbolSnapshotForViews(&panel, configManager, captureViews);

  panel.SetRenderMode(previousMode);

  if (!snapshot) {
    outLogLines.push_back("No symbol snapshot available from Viewer2D pipeline.");
    return false;
  }

  bool anyFound = false;
  for (const auto view : symbols::AllSymbolViews()) {
    const auto expectedKind = ToViewKind(view);
    const SymbolDefinition *found = nullptr;
    for (const auto &entry : *snapshot) {
      const auto &def = entry.second;
      if (def.key.modelKey == modelKey && def.key.viewKind == expectedKind) {
        found = &def;
        break;
      }
    }

    if (!found && view == symbols::SymbolView::Left) {
      for (const auto &entry : *snapshot) {
        const auto &def = entry.second;
        if (def.key.modelKey == modelKey && def.key.viewKind == SymbolViewKind::Right) {
          found = &def;
          break;
        }
      }
    }

    if (!found) {
      outLogLines.push_back(std::string("View ") + symbols::ToString(view) +
                            ": no symbol definition in Viewer2D cache");
      symbols::Symbol2D empty;
      empty.view = view;
      outSymbols.push_back(std::move(empty));
      continue;
    }

    symbols::ImageRGBA shapeImg;
    symbols::ImageRGBA lineImg;
    RasterizeSymbol(*found, shapeImg, lineImg);

    SymbolReferenceViews refs;
    refs.view = view;
    refs.shape.width = shapeImg.width;
    refs.shape.height = shapeImg.height;
    refs.shape.rgba = shapeImg.pixels;
    refs.line.width = lineImg.width;
    refs.line.height = lineImg.height;
    refs.line.rgba = lineImg.pixels;
    outReferences.push_back(std::move(refs));

    const auto shapeProcessingImg = BuildProcessingImage(shapeImg);
    const auto lineProcessingImg = BuildProcessingImage(lineImg);

    auto shapeMask = symbols::ExtractShapeMask(shapeProcessingImg);
    auto lineMask = symbols::ExtractLineMask(lineProcessingImg);
    symbols::MorphClose(shapeMask, shapeProcessingImg.width, shapeProcessingImg.height);

    symbols::Symbol2D symbol;
    symbol.view = view;
    symbol.stroke_width_px = 2.0f;
    symbol.fill = symbols::TraceFillPolygons(shapeMask, shapeProcessingImg.width,
                                             shapeProcessingImg.height, 1.5f);
    auto skeleton = symbols::SkeletonizeMask(lineMask, lineProcessingImg.width,
                                             lineProcessingImg.height);
    symbol.strokes = symbols::SkeletonToPolylines(
        skeleton, lineProcessingImg.width, lineProcessingImg.height, 6.0f, 1.0f);
    symbol.bounds = ComputeBoundsFromGeometry(symbol);

    size_t fillVertices = 0;
    for (const auto &poly : symbol.fill)
      fillVertices += poly.outer.size();
    size_t strokePoints = 0;
    for (const auto &stroke : symbol.strokes)
      strokePoints += stroke.points.size();

    std::ostringstream ss;
    ss << "View " << symbols::ToString(view)
       << ": references from Viewer2D symbol pipeline (processing "
       << shapeProcessingImg.width << "x" << shapeProcessingImg.height << "), fill="
       << symbol.fill.size()
       << " polygons, fillVertices=" << fillVertices << ", strokes="
       << symbol.strokes.size() << ", strokePoints=" << strokePoints;
    outLogLines.push_back(ss.str());

    outSymbols.push_back(std::move(symbol));
    anyFound = true;
  }

  return anyFound;
}

} // namespace symboltools
