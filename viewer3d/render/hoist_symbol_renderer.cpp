#include "hoist_symbol_renderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "configmanager.h"
#include "support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace HoistSymbolRenderer {
namespace {

constexpr float kSymbolSizeMeters = 0.5f;
constexpr float kHalfSizeMeters = kSymbolSizeMeters * 0.5f;
constexpr float kLineWidth = 1.2f;
constexpr float kSelectionLineWidth = 2.4f;
constexpr float kPi = 3.14159265358979323846f;

struct RgbColor {
  float r = 1.0f;
  float g = 0.0f;
  float b = 1.0f;
};

enum class HoistShape { Ton2, Ton1, TonHalf, TonQuarter, TonEighth };

std::string NormalizedLayerName(const std::string &layer) {
  return layer.empty() ? std::string(DEFAULT_LAYER_NAME) : layer;
}

bool IsLayerVisible(const std::unordered_set<std::string> &hiddenLayers,
                    const std::string &layer) {
  return hiddenLayers.find(NormalizedLayerName(layer)) == hiddenLayers.end();
}

RgbColor ResolveHoistFunctionColor(const std::string &hoistFunctionRaw) {
  const std::string hoistFunction = NormalizeHoistFunction(hoistFunctionRaw);
  if (hoistFunction == "Audio")
    return {1.0f, 0.0f, 0.0f};
  if (hoistFunction == "Video")
    return {0.0f, 1.0f, 0.0f};
  if (hoistFunction == "Scenic")
    return {0.0f, 0.0f, 1.0f};
  if (hoistFunction == "Extra")
    return {0.56f, 0.0f, 1.0f};
  if (hoistFunction == "Other")
    return {0.78f, 0.64f, 0.78f};
  return {1.0f, 0.0f, 1.0f}; // Lighting (default)
}

int HexNibbleValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

bool TryParseHexColor(const std::string &hex, RgbColor &out) {
  if (hex.size() != 7 || hex[0] != '#')
    return false;

  const int rHi = HexNibbleValue(hex[1]);
  const int rLo = HexNibbleValue(hex[2]);
  const int gHi = HexNibbleValue(hex[3]);
  const int gLo = HexNibbleValue(hex[4]);
  const int bHi = HexNibbleValue(hex[5]);
  const int bLo = HexNibbleValue(hex[6]);
  if (rHi < 0 || rLo < 0 || gHi < 0 || gLo < 0 || bHi < 0 || bLo < 0)
    return false;

  const float inv255 = 1.0f / 255.0f;
  out.r = static_cast<float>((rHi << 4) | rLo) * inv255;
  out.g = static_cast<float>((gHi << 4) | gLo) * inv255;
  out.b = static_cast<float>((bHi << 4) | bLo) * inv255;
  return true;
}

RgbColor ResolveHoistLayerColor(const MvrScene &scene, const Support &support) {
  const std::string normalizedLayer = NormalizedLayerName(support.layer);
  for (const auto &[layerUuid, layer] : scene.layers) {
    (void)layerUuid;
    if (layer.name != normalizedLayer)
      continue;
    RgbColor parsed;
    if (TryParseHexColor(layer.color, parsed))
      return parsed;
    break;
  }

  const std::string functionValue =
      support.hoistFunction.empty() ? support.function : support.hoistFunction;
  return ResolveHoistFunctionColor(functionValue);
}

HoistShape ResolveHoistShape(float capacityKg) {
  if (capacityKg >= 1500.0f)
    return HoistShape::Ton2;
  if (capacityKg >= 750.0f)
    return HoistShape::Ton1;
  if (capacityKg >= 375.0f)
    return HoistShape::TonHalf;
  if (capacityKg >= 187.5f)
    return HoistShape::TonQuarter;
  return HoistShape::TonEighth;
}

std::array<float, 3> MakePoint(float x, float y, float z) { return {x, y, z}; }

RgbColor WhiteColor() { return {1.0f, 1.0f, 1.0f}; }
RgbColor HighlightColor() { return {0.0f, 1.0f, 0.0f}; }
RgbColor SelectedColor() { return {0.0f, 1.0f, 1.0f}; }

void DrawFillPolygon(IRenderContext &renderContext,
                     const std::vector<std::array<float, 3>> &points,
                     const RgbColor &color) {
  if (points.size() < 3)
    return;

  if (!renderContext.IsCaptureOnly()) {
    renderContext.SetGLColor(color.r, color.g, color.b);
    glBegin(GL_TRIANGLE_FAN);
    for (const auto &p : points)
      glVertex3f(p[0], p[1], p[2]);
    glEnd();
  }

  CanvasStroke stroke;
  stroke.width = 0.0f;
  stroke.color = {color.r, color.g, color.b, 1.0f};
  CanvasFill fill;
  fill.color = {color.r, color.g, color.b, 1.0f};
  renderContext.RecordPolygon(points, stroke, &fill);
}

void DrawOutlineLoop(IRenderContext &renderContext,
                     const std::vector<std::array<float, 3>> &points,
                     const RgbColor &color, float lineWidth = kLineWidth) {
  if (points.size() < 2)
    return;

  if (!renderContext.IsCaptureOnly()) {
    glLineWidth(lineWidth);
    renderContext.SetGLColor(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (const auto &p : points)
      glVertex3f(p[0], p[1], p[2]);
    glEnd();
    glLineWidth(1.0f);
  }

  CanvasStroke stroke;
  stroke.width = lineWidth;
  stroke.color = {color.r, color.g, color.b, 1.0f};
  for (size_t i = 0; i < points.size(); ++i)
    renderContext.RecordLine(points[i], points[(i + 1) % points.size()], stroke);
}

void DrawSegment(IRenderContext &renderContext, const std::array<float, 3> &a,
                 const std::array<float, 3> &b, const RgbColor &color,
                 float lineWidth = kLineWidth) {
  if (!renderContext.IsCaptureOnly()) {
    glLineWidth(lineWidth);
    renderContext.SetGLColor(color.r, color.g, color.b);
    glBegin(GL_LINES);
    glVertex3f(a[0], a[1], a[2]);
    glVertex3f(b[0], b[1], b[2]);
    glEnd();
    glLineWidth(1.0f);
  }

  CanvasStroke stroke;
  stroke.width = lineWidth;
  stroke.color = {color.r, color.g, color.b, 1.0f};
  renderContext.RecordLine(a, b, stroke);
}

void DrawSquareSymbol(IRenderContext &renderContext, float cx, float cy,
                      float z, const RgbColor &color,
                      const RgbColor &outlineColor, float outlineWidth) {
  const float minX = cx - kHalfSizeMeters;
  const float maxX = cx + kHalfSizeMeters;
  const float minY = cy - kHalfSizeMeters;
  const float maxY = cy + kHalfSizeMeters;
  const float midX = cx;
  const float midY = cy;

  DrawFillPolygon(renderContext,
                  {MakePoint(minX, minY, z), MakePoint(maxX, minY, z),
                   MakePoint(maxX, maxY, z), MakePoint(minX, maxY, z)},
                  WhiteColor());
  DrawFillPolygon(renderContext,
                  {MakePoint(midX, midY, z), MakePoint(maxX, midY, z),
                   MakePoint(maxX, maxY, z), MakePoint(midX, maxY, z)},
                  color);
  DrawFillPolygon(renderContext,
                  {MakePoint(minX, minY, z), MakePoint(midX, minY, z),
                   MakePoint(midX, midY, z), MakePoint(minX, midY, z)},
                  color);

  DrawOutlineLoop(renderContext,
                  {MakePoint(minX, minY, z), MakePoint(maxX, minY, z),
                   MakePoint(maxX, maxY, z), MakePoint(minX, maxY, z)},
                  outlineColor, outlineWidth);
  DrawSegment(renderContext, MakePoint(cx, minY, z), MakePoint(cx, maxY, z),
              outlineColor, outlineWidth);
  DrawSegment(renderContext, MakePoint(minX, cy, z), MakePoint(maxX, cy, z),
              outlineColor, outlineWidth);
}

void DrawDiamondSymbol(IRenderContext &renderContext, float cx, float cy,
                       float z, const RgbColor &color,
                       const RgbColor &outlineColor, float outlineWidth) {
  const auto top = MakePoint(cx, cy + kHalfSizeMeters, z);
  const auto right = MakePoint(cx + kHalfSizeMeters, cy, z);
  const auto bottom = MakePoint(cx, cy - kHalfSizeMeters, z);
  const auto left = MakePoint(cx - kHalfSizeMeters, cy, z);
  const auto center = MakePoint(cx, cy, z);

  DrawFillPolygon(renderContext, {top, right, bottom, left}, WhiteColor());
  DrawFillPolygon(renderContext, {center, right, top}, color);
  DrawFillPolygon(renderContext, {center, left, bottom}, color);
  DrawOutlineLoop(renderContext, {top, right, bottom, left}, outlineColor,
                  outlineWidth);
  DrawSegment(renderContext, left, right, outlineColor, outlineWidth);
  DrawSegment(renderContext, bottom, top, outlineColor, outlineWidth);
}

void DrawCircleSymbol(IRenderContext &renderContext, float cx, float cy,
                      float z, const RgbColor &color,
                      const RgbColor &outlineColor, float outlineWidth) {
  constexpr int kSegments = 40;
  std::vector<std::array<float, 3>> ring;
  ring.reserve(static_cast<size_t>(kSegments));
  for (int i = 0; i < kSegments; ++i) {
    const float t = 2.0f * kPi *
                    (static_cast<float>(i) / static_cast<float>(kSegments));
    ring.push_back(
        MakePoint(cx + std::cos(t) * kHalfSizeMeters, cy + std::sin(t) * kHalfSizeMeters, z));
  }

  constexpr int kQuarterSegments = 11;
  std::vector<std::array<float, 3>> topRight;
  topRight.reserve(static_cast<size_t>(kQuarterSegments + 2));
  topRight.push_back(MakePoint(cx, cy, z));
  for (int i = 0; i <= kQuarterSegments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kQuarterSegments) *
                    (kPi * 0.5f);
    topRight.push_back(MakePoint(cx + std::cos(t) * kHalfSizeMeters,
                                 cy + std::sin(t) * kHalfSizeMeters, z));
  }

  std::vector<std::array<float, 3>> bottomLeft;
  bottomLeft.reserve(static_cast<size_t>(kQuarterSegments + 2));
  bottomLeft.push_back(MakePoint(cx, cy, z));
  for (int i = 0; i <= kQuarterSegments; ++i) {
    const float t = kPi +
                    static_cast<float>(i) / static_cast<float>(kQuarterSegments) *
                        (kPi * 0.5f);
    bottomLeft.push_back(MakePoint(cx + std::cos(t) * kHalfSizeMeters,
                                   cy + std::sin(t) * kHalfSizeMeters, z));
  }

  DrawFillPolygon(renderContext, ring, WhiteColor());
  DrawFillPolygon(renderContext, topRight, color);
  DrawFillPolygon(renderContext, bottomLeft, color);
  DrawOutlineLoop(renderContext, ring, outlineColor, outlineWidth);
  DrawSegment(renderContext, MakePoint(cx - kHalfSizeMeters, cy, z),
              MakePoint(cx + kHalfSizeMeters, cy, z), outlineColor,
              outlineWidth);
  DrawSegment(renderContext, MakePoint(cx, cy - kHalfSizeMeters, z),
              MakePoint(cx, cy + kHalfSizeMeters, z), outlineColor,
              outlineWidth);
}

void DrawTriangleSymbol(IRenderContext &renderContext, float cx, float cy,
                        float z, const RgbColor &color,
                        const RgbColor &outlineColor, float outlineWidth) {
  const auto top = MakePoint(cx, cy + kHalfSizeMeters, z);
  const auto left = MakePoint(cx - kHalfSizeMeters, cy - kHalfSizeMeters, z);
  const auto right = MakePoint(cx + kHalfSizeMeters, cy - kHalfSizeMeters, z);
  const auto center = MakePoint(cx, cy - kHalfSizeMeters * 0.05f, z);
  const auto baseMid = MakePoint(cx, cy - kHalfSizeMeters, z);
  const float crossY = center[1];

  const auto pointOnEdgeAtY = [z](const std::array<float, 3> &a,
                                  const std::array<float, 3> &b, float y) {
    const float dy = b[1] - a[1];
    if (std::fabs(dy) < 1e-6f)
      return MakePoint(a[0], y, z);
    const float t = (y - a[1]) / dy;
    return MakePoint(a[0] + (b[0] - a[0]) * t, y, z);
  };
  const auto leftCross = pointOnEdgeAtY(top, left, crossY);
  const auto rightCross = pointOnEdgeAtY(top, right, crossY);

  DrawFillPolygon(renderContext, {top, right, left}, WhiteColor());
  // Same fill pattern for all symbols: top-right + bottom-left.
  DrawFillPolygon(renderContext, {top, rightCross, center}, color);
  DrawFillPolygon(renderContext, {leftCross, left, baseMid, center}, color);

  DrawOutlineLoop(renderContext, {top, right, left}, outlineColor, outlineWidth);
  DrawSegment(renderContext, leftCross, rightCross, outlineColor, outlineWidth);
  DrawSegment(renderContext, top, baseMid, outlineColor, outlineWidth);
}

void DrawPentagonSymbol(IRenderContext &renderContext, float cx, float cy,
                        float z, const RgbColor &color,
                        const RgbColor &outlineColor, float outlineWidth) {
  const auto top = MakePoint(cx, cy + kHalfSizeMeters, z);
  const auto rightTop = MakePoint(cx + kHalfSizeMeters * 0.85f,
                                  cy + kHalfSizeMeters * 0.25f, z);
  const auto rightBottom =
      MakePoint(cx + kHalfSizeMeters * 0.55f, cy - kHalfSizeMeters, z);
  const auto leftBottom =
      MakePoint(cx - kHalfSizeMeters * 0.55f, cy - kHalfSizeMeters, z);
  const auto leftTop =
      MakePoint(cx - kHalfSizeMeters * 0.85f, cy + kHalfSizeMeters * 0.25f, z);
  const auto center = MakePoint(cx, cy - kHalfSizeMeters * 0.15f, z);
  const auto bottomMid =
      MakePoint((leftBottom[0] + rightBottom[0]) * 0.5f,
                (leftBottom[1] + rightBottom[1]) * 0.5f, z);

  const auto pointOnEdgeAtY = [z](const std::array<float, 3> &a,
                                  const std::array<float, 3> &b, float y) {
    const float dy = b[1] - a[1];
    if (std::fabs(dy) < 1e-6f)
      return MakePoint(a[0], y, z);
    const float t = (y - a[1]) / dy;
    return MakePoint(a[0] + (b[0] - a[0]) * t, y, z);
  };
  const auto leftCross = pointOnEdgeAtY(leftTop, leftBottom, center[1]);
  const auto rightCross = pointOnEdgeAtY(rightTop, rightBottom, center[1]);

  DrawFillPolygon(renderContext, {top, rightTop, rightBottom, leftBottom, leftTop},
                  WhiteColor());
  DrawFillPolygon(renderContext, {top, rightTop, rightCross, center}, color);
  DrawFillPolygon(renderContext, {leftCross, leftBottom, bottomMid, center}, color);

  DrawOutlineLoop(renderContext,
                  {top, rightTop, rightBottom, leftBottom, leftTop},
                  outlineColor, outlineWidth);
  DrawSegment(renderContext, leftCross, rightCross, outlineColor, outlineWidth);
  DrawSegment(renderContext, top, bottomMid, outlineColor, outlineWidth);
}

void DrawHoistSymbol(IRenderContext &renderContext, const Support &support,
                     const RgbColor &color, const RgbColor &outlineColor,
                     float outlineWidth) {
  const HoistShape shape = ResolveHoistShape(support.capacityKg);
  const float cx = support.transform.o[0] * RENDER_SCALE;
  const float cy = support.transform.o[1] * RENDER_SCALE;
  const float z = support.transform.o[2] * RENDER_SCALE;

  switch (shape) {
  case HoistShape::Ton2:
    DrawSquareSymbol(renderContext, cx, cy, z, color, outlineColor,
                     outlineWidth);
    break;
  case HoistShape::Ton1:
    DrawCircleSymbol(renderContext, cx, cy, z, color, outlineColor,
                     outlineWidth);
    break;
  case HoistShape::TonHalf:
    DrawTriangleSymbol(renderContext, cx, cy, z, color, outlineColor,
                       outlineWidth);
    break;
  case HoistShape::TonQuarter:
    DrawDiamondSymbol(renderContext, cx, cy, z, color, outlineColor,
                      outlineWidth);
    break;
  case HoistShape::TonEighth:
    DrawPentagonSymbol(renderContext, cx, cy, z, color, outlineColor,
                       outlineWidth);
    break;
  }
}

} // namespace

void Render(IRenderContext &renderContext, const RenderFrameContext &context) {
  if (!context.is2DViewer)
    return;
  if (context.view != Viewer2DView::Top && context.view != Viewer2DView::Bottom)
    return;

  const MvrScene &scene = ConfigManager::Get().GetScene();
  const auto &supports = scene.supports;
  for (const auto &[uuid, support] : supports) {
    if (!IsLayerVisible(context.hiddenLayers, support.layer))
      continue;

    const RgbColor color = ResolveHoistLayerColor(scene, support);
    const bool isHighlighted = renderContext.IsUuidHighlighted(uuid);
    const bool isSelected = renderContext.IsUuidSelected(uuid);
    const bool useOutlineColors =
        context.is2DViewer && renderContext.IsSelectionOutlineEnabled2D();
    const RgbColor outlineColor =
        useOutlineColors ? (isHighlighted ? HighlightColor()
                                          : (isSelected ? SelectedColor() : color))
                         : color;
    const float outlineWidth =
        useOutlineColors && (isHighlighted || isSelected)
            ? kSelectionLineWidth
            : kLineWidth;

    DrawHoistSymbol(renderContext, support, color, outlineColor, outlineWidth);
  }
}

} // namespace HoistSymbolRenderer
