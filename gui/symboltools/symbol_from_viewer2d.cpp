#include "symboltools/symbol_from_viewer2d.h"

#include "canvas2d.h"
#include "configmanager.h"
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

namespace symboltools {
namespace {

// Keep this moderate to avoid UI stalls when skeletonizing on the main thread.
constexpr int kRenderResolution = 384;
constexpr float kPadding = 24.0f;

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

Viewer2DView ToViewerView(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return Viewer2DView::Front;
  case symbols::SymbolView::Top:
    return Viewer2DView::Top;
  case symbols::SymbolView::Bottom:
    return Viewer2DView::Bottom;
  case symbols::SymbolView::Left:
    return Viewer2DView::Side;
  }
  return Viewer2DView::Top;
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
  CanvasState current;
  std::vector<CanvasState> stack;

  for (const auto &cmd : def.localCommands.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      const auto p0 = ApplyCanvasState(current, line->x0, line->y0);
      const auto p1 = ApplyCanvasState(current, line->x1, line->y1);
      auto a = ToImage(tf, p0.x, p0.y, shapeImg.height);
      auto c = ToImage(tf, p1.x, p1.y, shapeImg.height);
      const int thick = std::clamp(
          static_cast<int>(std::ceil(line->stroke.width * current.scale * tf.scale * 0.25f)),
          1, 2);
      DrawLine(shapeImg, a, c, thick, 0, 0, 0, 255);
      DrawLine(lineImg, a, c, thick, 0, 0, 0, 255);
    } else if (const auto *poly = std::get_if<PolylineCommand>(&cmd)) {
      if (poly->points.size() < 4)
        continue;
      const int thick = std::clamp(
          static_cast<int>(std::ceil(poly->stroke.width * current.scale * tf.scale * 0.25f)),
          1, 2);
      for (size_t i = 0; i + 3 < poly->points.size(); i += 2) {
        const auto p0 = ApplyCanvasState(current, poly->points[i], poly->points[i + 1]);
        const auto p1 = ApplyCanvasState(current, poly->points[i + 2], poly->points[i + 3]);
        auto a = ToImage(tf, p0.x, p0.y, shapeImg.height);
        auto c = ToImage(tf, p1.x, p1.y, shapeImg.height);
        DrawLine(shapeImg, a, c, thick, 0, 0, 0, 255);
        DrawLine(lineImg, a, c, thick, 0, 0, 0, 255);
      }
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      std::vector<symbols::Point2D> pts;
      pts.reserve(poly->points.size() / 2);
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        const auto p = ApplyCanvasState(current, poly->points[i], poly->points[i + 1]);
        pts.push_back(ToImage(tf, p.x, p.y, shapeImg.height));
      }
      if (poly->hasFill) {
        FillPolygon(shapeImg, pts, 0, 0, 0, 255);
        FillPolygon(lineImg, pts, 255, 255, 255, 255);
      }
      const int thick = std::clamp(
          static_cast<int>(std::ceil(poly->stroke.width * current.scale * tf.scale * 0.25f)),
          1, 2);
      for (size_t i = 0; i < pts.size(); ++i) {
        const auto &a = pts[i];
        const auto &c = pts[(i + 1) % pts.size()];
        DrawLine(shapeImg, a, c, thick, 0, 0, 0, 255);
        DrawLine(lineImg, a, c, thick, 0, 0, 0, 255);
      }
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      std::vector<symbols::Point2D> pts = {
          ToImage(tf, ApplyCanvasState(current, rect->x, rect->y).x,
                  ApplyCanvasState(current, rect->x, rect->y).y, shapeImg.height),
          ToImage(tf, ApplyCanvasState(current, rect->x + rect->w, rect->y).x,
                  ApplyCanvasState(current, rect->x + rect->w, rect->y).y,
                  shapeImg.height),
          ToImage(tf,
                  ApplyCanvasState(current, rect->x + rect->w, rect->y + rect->h).x,
                  ApplyCanvasState(current, rect->x + rect->w, rect->y + rect->h).y,
                  shapeImg.height),
          ToImage(tf, ApplyCanvasState(current, rect->x, rect->y + rect->h).x,
                  ApplyCanvasState(current, rect->x, rect->y + rect->h).y,
                  shapeImg.height)};
      if (rect->hasFill) {
        FillPolygon(shapeImg, pts, 0, 0, 0, 255);
        FillPolygon(lineImg, pts, 255, 255, 255, 255);
      }
      const int thick = std::clamp(
          static_cast<int>(std::ceil(rect->stroke.width * current.scale * tf.scale * 0.25f)),
          1, 2);
      for (size_t i = 0; i < pts.size(); ++i) {
        DrawLine(shapeImg, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
        DrawLine(lineImg, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
      }
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      const auto center = ApplyCanvasState(current, circle->cx, circle->cy);
      const float radius = circle->radius * current.scale;
      auto pts = BuildCircle(tf, shapeImg.height, center.x, center.y, radius);
      if (circle->hasFill) {
        FillPolygon(shapeImg, pts, 0, 0, 0, 255);
        FillPolygon(lineImg, pts, 255, 255, 255, 255);
      }
      const int thick = std::clamp(
          static_cast<int>(std::ceil(circle->stroke.width * current.scale * tf.scale * 0.25f)),
          1, 2);
      for (size_t i = 0; i < pts.size(); ++i) {
        DrawLine(shapeImg, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
        DrawLine(lineImg, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0, 0, 255);
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
  }
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


symbols::ImageRGBA MakeReferenceFromViewerImage(const std::vector<unsigned char> &pixels,
                                                int width, int height,
                                                bool shapeReference) {
  symbols::ImageRGBA image;
  image.width = width;
  image.height = height;
  image.pixels.assign(static_cast<size_t>(width * height * 4), 0);
  if (pixels.size() < image.pixels.size())
    return image;

  for (int y = 0; y < height; ++y) {
    const int srcY = height - 1 - y;
    for (int x = 0; x < width; ++x) {
      const size_t srcIdx = static_cast<size_t>((srcY * width + x) * 4);
      const size_t dstIdx = static_cast<size_t>((y * width + x) * 4);
      const uint8_t r = pixels[srcIdx];
      const uint8_t g = pixels[srcIdx + 1];
      const uint8_t b = pixels[srcIdx + 2];
      const uint8_t a = pixels[srcIdx + 3];
      const int dark = static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b);
      const bool isLine = dark < 540;

      if (shapeReference) {
        if (isLine) {
          image.pixels[dstIdx] = 0;
          image.pixels[dstIdx + 1] = 0;
          image.pixels[dstIdx + 2] = 0;
          image.pixels[dstIdx + 3] = 255;
        }
      } else {
        image.pixels[dstIdx] = 255;
        image.pixels[dstIdx + 1] = 255;
        image.pixels[dstIdx + 2] = 255;
        image.pixels[dstIdx + 3] = a;
        if (isLine) {
          image.pixels[dstIdx] = 0;
          image.pixels[dstIdx + 1] = 0;
          image.pixels[dstIdx + 2] = 0;
          image.pixels[dstIdx + 3] = 255;
        }
      }
    }
  }
  return image;
}

class ScopedFixtureIsolation {
public:
  ScopedFixtureIsolation(ConfigManager &cfg, const std::string &fixtureUuid)
      : cfg_(cfg), backup_(cfg.GetScene()) {
    auto &scene = cfg_.GetScene();
    auto fixtureIt = scene.fixtures.find(fixtureUuid);
    if (fixtureIt == scene.fixtures.end())
      return;

    MvrScene isolated;
    isolated.basePath = scene.basePath;
    isolated.provider = scene.provider;
    isolated.providerVersion = scene.providerVersion;
    isolated.versionMajor = scene.versionMajor;
    isolated.versionMinor = scene.versionMinor;
    isolated.layers = scene.layers;
    isolated.positions = scene.positions;
    isolated.symdefFiles = scene.symdefFiles;
    isolated.symdefTypes = scene.symdefTypes;
    isolated.symdefMatrices = scene.symdefMatrices;
    isolated.symdefGeometries = scene.symdefGeometries;
    isolated.fixtures.emplace(fixtureIt->first, fixtureIt->second);
    scene = std::move(isolated);
    active_ = true;
  }

  ~ScopedFixtureIsolation() {
    if (active_)
      cfg_.GetScene() = backup_;
  }

  bool Active() const { return active_; }

private:
  ConfigManager &cfg_;
  MvrScene backup_;
  bool active_ = false;
};

} // namespace

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      ConfigManager &configManager,
                                      const std::string &fixtureUuid,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines,
                                      std::vector<SymbolReferenceViews> &outReferences) {
  outSymbols.clear();
  outLogLines.clear();
  outReferences.clear();

  if (fixtureUuid.empty()) {
    outLogLines.push_back("No representative fixture UUID available.");
    return false;
  }

  ScopedFixtureIsolation fixtureIsolation(configManager, fixtureUuid);
  if (!fixtureIsolation.Active()) {
    outLogLines.push_back("Could not isolate fixture for Viewer2D reference capture.");
    return false;
  }

  const auto previousMode = panel.GetRenderMode();
  const auto previousView = panel.GetView();

  panel.SetRenderMode(Viewer2DRenderMode::White);
  std::vector<symbols::ImageRGBA> viewShapeImages;
  std::vector<symbols::ImageRGBA> viewLineImages;
  viewShapeImages.reserve(4);
  viewLineImages.reserve(4);
  for (const auto view : symbols::AllSymbolViews()) {
    panel.SetView(ToViewerView(view));
    panel.UpdateScene(true);
    panel.CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {}, true, false);

    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (panel.RenderToRGBA(pixels, width, height) && width > 0 && height > 0) {
      viewShapeImages.push_back(MakeReferenceFromViewerImage(pixels, width, height, true));
      viewLineImages.push_back(MakeReferenceFromViewerImage(pixels, width, height, false));
    } else {
      viewShapeImages.push_back({});
      viewLineImages.push_back({});
    }
  }

  const auto snapshot = panel.GetBottomSymbolCacheSnapshot();
  panel.SetView(previousView);
  panel.SetRenderMode(previousMode);

  if (!snapshot) {
    outLogLines.push_back("No symbol snapshot available from Viewer2D pipeline.");
    return false;
  }

  bool anyFound = false;
  size_t viewIndex = 0;
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
      ++viewIndex;
      continue;
    }

    symbols::ImageRGBA shapeImg;
    symbols::ImageRGBA lineImg;
    if (viewIndex < viewShapeImages.size()) {
      shapeImg = viewShapeImages[viewIndex];
      lineImg = viewLineImages[viewIndex];
    }
    if (shapeImg.pixels.empty() || lineImg.pixels.empty())
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

    auto shapeMask = symbols::ExtractShapeMask(shapeImg);
    auto lineMask = symbols::ExtractLineMask(lineImg);
    symbols::MorphClose(shapeMask, shapeImg.width, shapeImg.height);

    symbols::Symbol2D symbol;
    symbol.view = view;
    symbol.stroke_width_px = 2.0f;
    symbol.fill = symbols::TraceFillPolygons(shapeMask, shapeImg.width, shapeImg.height,
                                             1.5f);
    auto skeleton = symbols::SkeletonizeMask(lineMask, lineImg.width, lineImg.height);
    symbol.strokes = symbols::SkeletonToPolylines(skeleton, lineImg.width,
                                                  lineImg.height, 6.0f, 1.0f);
    symbol.bounds = ComputeBoundsFromGeometry(symbol);

    size_t fillVertices = 0;
    for (const auto &poly : symbol.fill)
      fillVertices += poly.outer.size();
    size_t strokePoints = 0;
    for (const auto &stroke : symbol.strokes)
      strokePoints += stroke.points.size();

    std::ostringstream ss;
    ss << "View " << symbols::ToString(view)
       << ": masks from Viewer2D symbol image, fill=" << symbol.fill.size()
       << " polygons, fillVertices=" << fillVertices << ", strokes="
       << symbol.strokes.size() << ", strokePoints=" << strokePoints;
    outLogLines.push_back(ss.str());

    outSymbols.push_back(std::move(symbol));
    anyFound = true;
    ++viewIndex;
  }

  return anyFound;
}

} // namespace symboltools
