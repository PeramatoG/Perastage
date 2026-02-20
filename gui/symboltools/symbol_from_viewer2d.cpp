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
#include "viewer2dcommandrenderer.h"
#include "viewer2dpanel.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace symboltools {
namespace {

// Keep this moderate to avoid UI stalls when skeletonizing on the main thread.
constexpr int kRenderResolution = 384;
constexpr float kPadding = 24.0f;
constexpr int kMaxProcessingResolution = 384;

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

void DrawRasterLine(symbols::ImageRGBA &img, symbols::Point2D a,
                    symbols::Point2D b, int thickness, uint8_t r, uint8_t g,
                    uint8_t bl, uint8_t alpha) {
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

enum class ReferencePassMode {
  ShapeBlack,
  LineWhiteFill,
};

class SymbolRasterBackend : public viewer2d::IViewer2DCommandBackend {
public:
  SymbolRasterBackend(symbols::ImageRGBA &img, ReferencePassMode mode)
      : image_(img), mode_(mode) {}

  void DrawLine(const viewer2d::Viewer2DRenderPoint &p0,
                const viewer2d::Viewer2DRenderPoint &p1,
                const CanvasStroke &stroke, double strokeWidthPx) override {
    if (!ShouldDrawStroke(stroke, strokeWidthPx))
      return;
    DrawRasterLine(image_, {static_cast<float>(p0.x), static_cast<float>(p0.y)},
                   {static_cast<float>(p1.x), static_cast<float>(p1.y)},
                   StrokeThickness(strokeWidthPx), 0, 0, 0, 255);
  }

  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                    const CanvasStroke &stroke, double strokeWidthPx) override {
    if (points.size() < 2 || !ShouldDrawStroke(stroke, strokeWidthPx))
      return;
    const int thick = StrokeThickness(strokeWidthPx);
    for (size_t i = 1; i < points.size(); ++i) {
      DrawRasterLine(
          image_,
          {static_cast<float>(points[i - 1].x), static_cast<float>(points[i - 1].y)},
          {static_cast<float>(points[i].x), static_cast<float>(points[i].y)},
          thick, 0, 0, 0, 255);
    }
  }

  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                   const CanvasStroke &stroke, const CanvasFill *fill,
                   double strokeWidthPx) override {
    if (points.empty())
      return;

    std::vector<symbols::Point2D> pts;
    pts.reserve(points.size());
    for (const auto &p : points)
      pts.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});

    if (ShouldDrawFill(fill)) {
      const uint8_t fillGray = (mode_ == ReferencePassMode::ShapeBlack) ? 0 : 255;
      FillPolygon(image_, pts, fillGray, fillGray, fillGray, 255);
    }

    if (!ShouldDrawStroke(stroke, strokeWidthPx))
      return;

    const int thick = StrokeThickness(strokeWidthPx);
    for (size_t i = 0; i < pts.size(); ++i)
      DrawRasterLine(image_, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0,
                     0, 255);
  }

  void DrawCircle(const viewer2d::Viewer2DRenderPoint &center, double radiusPx,
                  const CanvasStroke &stroke, const CanvasFill *fill,
                  double strokeWidthPx) override {
    constexpr int kSegments = 48;
    constexpr float kTwoPi = 6.28318530717958647692f;
    std::vector<symbols::Point2D> pts;
    pts.reserve(kSegments);
    for (int i = 0; i < kSegments; ++i) {
      const float angle =
          (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
      pts.push_back({static_cast<float>(center.x + std::cos(angle) * radiusPx),
                     static_cast<float>(center.y + std::sin(angle) * radiusPx)});
    }
    if (ShouldDrawFill(fill)) {
      const uint8_t fillGray = (mode_ == ReferencePassMode::ShapeBlack) ? 0 : 255;
      FillPolygon(image_, pts, fillGray, fillGray, fillGray, 255);
    }
    if (!ShouldDrawStroke(stroke, strokeWidthPx))
      return;
    const int thick = StrokeThickness(strokeWidthPx);
    for (size_t i = 0; i < pts.size(); ++i)
      DrawRasterLine(image_, pts[i], pts[(i + 1) % pts.size()], thick, 0, 0,
                     0, 255);
  }

  void DrawText(const viewer2d::Viewer2DRenderText &text) override {
    (void)text;
  }

private:
  bool ShouldDrawStroke(const CanvasStroke &stroke, double strokeWidthPx) const {
    return stroke.color.a > 0.001f && strokeWidthPx > 0.0;
  }

  bool ShouldDrawFill(const CanvasFill *fill) const {
    return fill != nullptr && fill->color.a > 0.001f;
  }

  int StrokeThickness(double strokeWidthPx) const {
    return std::clamp(static_cast<int>(std::ceil(strokeWidthPx * 0.25)), 1, 2);
  }

  symbols::ImageRGBA &image_;
  ReferencePassMode mode_;
};

void RasterizeSymbol(const SymbolDefinition &def,
                     const SymbolDefinitionSnapshot *snapshot,
                     symbols::ImageRGBA &shapeImg, symbols::ImageRGBA &lineImg) {
  shapeImg.width = kRenderResolution;
  shapeImg.height = kRenderResolution;
  shapeImg.pixels.assign(static_cast<size_t>(kRenderResolution * kRenderResolution * 4), 0);
  lineImg = shapeImg;

  const float spanX = std::max(1e-4f, def.bounds.max.x - def.bounds.min.x);
  const float spanY = std::max(1e-4f, def.bounds.max.y - def.bounds.min.y);
  const double drawW = static_cast<double>(shapeImg.width) - 2.0 * kPadding;
  const double drawH = static_cast<double>(shapeImg.height) - 2.0 * kPadding;
  const double scale = std::max(0.001, std::min(drawW / spanX, drawH / spanY));

  viewer2d::Viewer2DRenderMapping mapping;
  mapping.minX = def.bounds.min.x;
  mapping.minY = def.bounds.min.y;
  mapping.scale = scale;
  mapping.offsetX = (static_cast<double>(shapeImg.width) - spanX * scale) * 0.5;
  mapping.offsetY = (static_cast<double>(shapeImg.height) - spanY * scale) * 0.5;
  mapping.drawHeight = spanY * scale;

  SymbolRasterBackend shapeBackend(shapeImg, ReferencePassMode::ShapeBlack);
  viewer2d::Viewer2DCommandRenderer shapeRenderer(mapping, shapeBackend, snapshot);
  shapeRenderer.Render(def.localCommands);

  SymbolRasterBackend lineBackend(lineImg, ReferencePassMode::LineWhiteFill);
  viewer2d::Viewer2DCommandRenderer lineRenderer(mapping, lineBackend, snapshot);
  lineRenderer.Render(def.localCommands);
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

class ScopedFixtureIsolation {
public:
  ScopedFixtureIsolation(ConfigManager &cfg, Viewer2DPanel &panel,
                         const std::string &fixtureUuid)
      : cfg_(cfg), panel_(panel) {
    auto &scene = cfg_.GetScene();
    previousFixtures_ = scene.fixtures;
    previousTrusses_ = scene.trusses;
    previousSupports_ = scene.supports;
    previousSceneObjects_ = scene.sceneObjects;

    if (!fixtureUuid.empty()) {
      auto it = scene.fixtures.find(fixtureUuid);
      if (it != scene.fixtures.end()) {
        const auto fixture = it->second;
        scene.fixtures.clear();
        scene.fixtures.emplace(fixtureUuid, fixture);
      }
    }

    scene.trusses.clear();
    scene.supports.clear();
    scene.sceneObjects.clear();
    panel_.UpdateScene(true);
  }

  ~ScopedFixtureIsolation() {
    auto &scene = cfg_.GetScene();
    scene.fixtures = std::move(previousFixtures_);
    scene.trusses = std::move(previousTrusses_);
    scene.supports = std::move(previousSupports_);
    scene.sceneObjects = std::move(previousSceneObjects_);
    panel_.UpdateScene(true);
  }

private:
  ConfigManager &cfg_;
  Viewer2DPanel &panel_;
  std::unordered_map<std::string, Fixture> previousFixtures_;
  std::unordered_map<std::string, Truss> previousTrusses_;
  std::unordered_map<std::string, Support> previousSupports_;
  std::unordered_map<std::string, SceneObject> previousSceneObjects_;
};

bool CaptureFixtureViewReference(Viewer2DPanel &panel, symbols::SymbolView view,
                                 SymbolReferenceViews &outReferences,
                                 std::string &outMessage) {
  const auto previousView = panel.GetView();

  Viewer2DView targetView = Viewer2DView::Top;
  switch (view) {
  case symbols::SymbolView::Front:
    targetView = Viewer2DView::Front;
    break;
  case symbols::SymbolView::Top:
    targetView = Viewer2DView::Top;
    break;
  case symbols::SymbolView::Bottom:
    targetView = Viewer2DView::Bottom;
    break;
  case symbols::SymbolView::Left:
    targetView = Viewer2DView::Side;
    break;
  }

  panel.SetView(targetView);
  // RenderToRGBA already performs an explicit offscreen render. Avoid forcing
  // an onscreen CaptureFrameNow pass here because it can block inside
  // SwapBuffers on some drivers/toolchains.

  std::vector<unsigned char> pixels;
  int width = 0;
  int height = 0;
  const bool ok = panel.RenderToRGBA(pixels, width, height);
  panel.SetView(previousView);
  if (!ok || width <= 0 || height <= 0 || pixels.empty()) {
    outMessage = std::string("View ") + symbols::ToString(view) +
                 ": failed to capture Viewer2D reference image";
    return false;
  }

  outReferences.view = view;
  outReferences.line.width = width;
  outReferences.line.height = height;
  outReferences.line.rgba = std::move(pixels);
  outReferences.shape = outReferences.line;

  std::ostringstream ss;
  ss << "View " << symbols::ToString(view)
     << ": captured isolated fixture from Viewer2D (" << width << "x"
     << height << ")";
  outMessage = ss.str();
  return true;
}

} // namespace

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      ConfigManager &configManager,
                                      const std::string &fixtureUuid,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines,
                                      std::vector<SymbolReferenceViews> &outReferences) {
  (void)modelKey;
  outSymbols.clear();
  outLogLines.clear();
  outReferences.clear();

  const auto previousMode = panel.GetRenderMode();
  const auto previousView = panel.GetView();
  panel.SetRenderMode(Viewer2DRenderMode::White);

  ScopedFixtureIsolation fixtureIsolation(configManager, panel, fixtureUuid);

  bool anyCaptured = false;
  for (const auto view : symbols::AllSymbolViews()) {
    SymbolReferenceViews refs;
    std::string message;
    if (!CaptureFixtureViewReference(panel, view, refs, message)) {
      outLogLines.push_back(message);
      continue;
    }
    outReferences.push_back(std::move(refs));
    outLogLines.push_back(message);
    anyCaptured = true;
  }

  panel.SetView(previousView);
  panel.SetRenderMode(previousMode);
  outLogLines.push_back(
      "Reference-only mode enabled: vectorization is intentionally bypassed.");
  return anyCaptured;
}

} // namespace symboltools
