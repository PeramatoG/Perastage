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
#include "viewer3dcontroller.h"

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

void DrawFillPolygon(Viewer3DController &controller,
                     const std::vector<std::array<float, 3>> &points,
                     const RgbColor &color) {
  if (points.size() < 3)
    return;

  if (!controller.IsCaptureOnly()) {
    controller.SetGLColor(color.r, color.g, color.b);
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
  controller.RecordPolygon(points, stroke, &fill);
}

void DrawOutlineLoop(Viewer3DController &controller,
                     const std::vector<std::array<float, 3>> &points,
                     const RgbColor &color) {
  if (points.size() < 2)
    return;

  if (!controller.IsCaptureOnly()) {
    glLineWidth(kLineWidth);
    controller.SetGLColor(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (const auto &p : points)
      glVertex3f(p[0], p[1], p[2]);
    glEnd();
    glLineWidth(1.0f);
  }

  CanvasStroke stroke;
  stroke.width = kLineWidth;
  stroke.color = {color.r, color.g, color.b, 1.0f};
  for (size_t i = 0; i < points.size(); ++i)
    controller.RecordLine(points[i], points[(i + 1) % points.size()], stroke);
}

void DrawSegment(Viewer3DController &controller, const std::array<float, 3> &a,
                 const std::array<float, 3> &b, const RgbColor &color) {
  if (!controller.IsCaptureOnly()) {
    glLineWidth(kLineWidth);
    controller.SetGLColor(color.r, color.g, color.b);
    glBegin(GL_LINES);
    glVertex3f(a[0], a[1], a[2]);
    glVertex3f(b[0], b[1], b[2]);
    glEnd();
    glLineWidth(1.0f);
  }

  CanvasStroke stroke;
  stroke.width = kLineWidth;
  stroke.color = {color.r, color.g, color.b, 1.0f};
  controller.RecordLine(a, b, stroke);
}

void DrawSquareSymbol(Viewer3DController &controller, float cx, float cy,
                      float z, const RgbColor &color) {
  const float minX = cx - kHalfSizeMeters;
  const float maxX = cx + kHalfSizeMeters;
  const float minY = cy - kHalfSizeMeters;
  const float maxY = cy + kHalfSizeMeters;
  const float midX = cx;
  const float midY = cy;

  DrawFillPolygon(controller,
                  {MakePoint(midX, midY, z), MakePoint(maxX, midY, z),
                   MakePoint(maxX, maxY, z), MakePoint(midX, maxY, z)},
                  color);
  DrawFillPolygon(controller,
                  {MakePoint(minX, minY, z), MakePoint(midX, minY, z),
                   MakePoint(midX, midY, z), MakePoint(minX, midY, z)},
                  color);

  DrawOutlineLoop(controller,
                  {MakePoint(minX, minY, z), MakePoint(maxX, minY, z),
                   MakePoint(maxX, maxY, z), MakePoint(minX, maxY, z)},
                  color);
  DrawSegment(controller, MakePoint(cx, minY, z), MakePoint(cx, maxY, z), color);
  DrawSegment(controller, MakePoint(minX, cy, z), MakePoint(maxX, cy, z), color);
}

void DrawDiamondSymbol(Viewer3DController &controller, float cx, float cy,
                       float z, const RgbColor &color) {
  const auto top = MakePoint(cx, cy + kHalfSizeMeters, z);
  const auto right = MakePoint(cx + kHalfSizeMeters, cy, z);
  const auto bottom = MakePoint(cx, cy - kHalfSizeMeters, z);
  const auto left = MakePoint(cx - kHalfSizeMeters, cy, z);
  const auto center = MakePoint(cx, cy, z);

  DrawFillPolygon(controller, {center, right, top}, color);
  DrawFillPolygon(controller, {center, left, bottom}, color);
  DrawOutlineLoop(controller, {top, right, bottom, left}, color);
  DrawSegment(controller, left, right, color);
  DrawSegment(controller, bottom, top, color);
}

void DrawCircleSymbol(Viewer3DController &controller, float cx, float cy,
                      float z, const RgbColor &color) {
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

  DrawFillPolygon(controller, topRight, color);
  DrawFillPolygon(controller, bottomLeft, color);
  DrawOutlineLoop(controller, ring, color);
  DrawSegment(controller, MakePoint(cx - kHalfSizeMeters, cy, z),
              MakePoint(cx + kHalfSizeMeters, cy, z), color);
  DrawSegment(controller, MakePoint(cx, cy - kHalfSizeMeters, z),
              MakePoint(cx, cy + kHalfSizeMeters, z), color);
}

void DrawTriangleSymbol(Viewer3DController &controller, float cx, float cy,
                        float z, const RgbColor &color) {
  const auto top = MakePoint(cx, cy + kHalfSizeMeters, z);
  const auto left = MakePoint(cx - kHalfSizeMeters, cy - kHalfSizeMeters, z);
  const auto right = MakePoint(cx + kHalfSizeMeters, cy - kHalfSizeMeters, z);
  const auto center = MakePoint(cx, cy - kHalfSizeMeters / 6.0f, z);

  DrawFillPolygon(controller, {top, MakePoint(cx - 0.06f, cy + 0.03f, z), center},
                  color);
  DrawFillPolygon(controller, {left, MakePoint(cx - 0.08f, cy - 0.05f, z), center},
                  color);

  DrawOutlineLoop(controller, {top, right, left}, color);
  DrawSegment(controller, top, center, color);
  DrawSegment(controller, left, center, color);
  DrawSegment(controller, right, center, color);
}

void DrawPentagonSymbol(Viewer3DController &controller, float cx, float cy,
                        float z, const RgbColor &color) {
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

  DrawFillPolygon(controller, {top, rightTop, center}, color);
  DrawFillPolygon(controller, {leftBottom, rightBottom, center}, color);

  DrawOutlineLoop(controller, {top, rightTop, rightBottom, leftBottom, leftTop}, color);
  DrawSegment(controller, top, center, color);
  DrawSegment(controller, leftTop, center, color);
  DrawSegment(controller, rightTop, center, color);
  DrawSegment(controller, leftBottom, center, color);
  DrawSegment(controller, rightBottom, center, color);
}

void DrawHoistSymbol(Viewer3DController &controller, const Support &support,
                     const RgbColor &color) {
  const HoistShape shape = ResolveHoistShape(support.capacityKg);
  const float cx = support.transform.o[0] * RENDER_SCALE;
  const float cy = support.transform.o[1] * RENDER_SCALE;
  const float z = support.transform.o[2] * RENDER_SCALE;

  switch (shape) {
  case HoistShape::Ton2:
    DrawSquareSymbol(controller, cx, cy, z, color);
    break;
  case HoistShape::Ton1:
    DrawCircleSymbol(controller, cx, cy, z, color);
    break;
  case HoistShape::TonHalf:
    DrawTriangleSymbol(controller, cx, cy, z, color);
    break;
  case HoistShape::TonQuarter:
    DrawDiamondSymbol(controller, cx, cy, z, color);
    break;
  case HoistShape::TonEighth:
    DrawPentagonSymbol(controller, cx, cy, z, color);
    break;
  }
}

} // namespace

void Render(Viewer3DController &controller, const RenderFrameContext &context) {
  if (!context.is2DViewer)
    return;
  if (context.view != Viewer2DView::Top && context.view != Viewer2DView::Bottom)
    return;

  const auto &supports = ConfigManager::Get().GetScene().supports;
  for (const auto &[uuid, support] : supports) {
    (void)uuid;
    if (!IsLayerVisible(context.hiddenLayers, support.layer))
      continue;

    const std::string functionValue =
        support.hoistFunction.empty() ? support.function : support.hoistFunction;
    DrawHoistSymbol(controller, support, ResolveHoistFunctionColor(functionValue));
  }
}

} // namespace HoistSymbolRenderer
