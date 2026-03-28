#include "viewer2d_ruler_overlay.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <sstream>

namespace viewer2d {
namespace {
constexpr float kPixelsPerMeter = 25.0f;
constexpr float kTickStepMeters = 0.5f;
constexpr float kRulerLabelFontSize = 3.0f;
constexpr float kRulerLabelOffsetMeters = 0.12f;

bool IsMeterTick(float valueMeters) {
  const float rounded = std::round(valueMeters);
  return std::fabs(valueMeters - rounded) < 0.0001f;
}

struct WorldPoint {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

WorldPoint MapViewCoordinatesToWorld(float u, float v, Viewer2DView view) {
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {u, v, 0.0f};
  case Viewer2DView::Front:
    return {u, 0.0f, v};
  case Viewer2DView::Side:
    return {0.0f, u, v};
  }
  return {u, v, 0.0f};
}

void EmitViewLine(float u0, float v0, float u1, float v1, Viewer2DView view) {
  const WorldPoint p0 = MapViewCoordinatesToWorld(u0, v0, view);
  const WorldPoint p1 = MapViewCoordinatesToWorld(u1, v1, view);
  glVertex3f(p0.x, p0.y, p0.z);
  glVertex3f(p1.x, p1.y, p1.z);
}

void DrawHorizontalRuler(float minU, float maxU, float vAxis,
                         float shortTickMeters, float longTickMeters,
                         Viewer2DView view) {
  glBegin(GL_LINES);
  EmitViewLine(minU, vAxis, maxU, vAxis, view);

  const float startTick = std::ceil(minU / kTickStepMeters) * kTickStepMeters;
  for (float u = startTick; u <= maxU + 0.0001f; u += kTickStepMeters) {
    const float tick = IsMeterTick(u) ? longTickMeters : shortTickMeters;
    EmitViewLine(u, vAxis, u, vAxis + tick, view);
  }
  glEnd();
}

void DrawVerticalRuler(float minV, float maxV, float uAxis,
                       float shortTickMeters, float longTickMeters,
                       Viewer2DView view) {
  glBegin(GL_LINES);
  EmitViewLine(uAxis, minV, uAxis, maxV, view);

  const float startTick = std::ceil(minV / kTickStepMeters) * kTickStepMeters;
  const float tickDirection = VerticalTickDirection(view);
  for (float v = startTick; v <= maxV + 0.0001f; v += kTickStepMeters) {
    const float tick = IsMeterTick(v) ? longTickMeters : shortTickMeters;
    EmitViewLine(uAxis, v, uAxis + tick * tickDirection, v, view);
  }
  glEnd();
}

CanvasStroke BuildRulerStroke(bool darkMode) {
  CanvasStroke stroke;
  stroke.color = darkMode ? CanvasColor{1.0f, 1.0f, 1.0f, 1.0f}
                          : CanvasColor{0.0f, 0.0f, 0.0f, 1.0f};
  stroke.width = 1.0f;
  return stroke;
}

struct ActiveRulers {
  float horizontalAxisMeters = 0.0f;
  float verticalAxisMeters = 0.0f;
};

ActiveRulers ResolveActiveRulers(const RulerOverlayViewState &state) {
  switch (state.view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {state.xRulerPositionMeters, state.yRulerPositionMeters};
  case Viewer2DView::Front:
    return {state.xRulerPositionMeters, state.zRulerPositionMeters};
  case Viewer2DView::Side:
    return {state.yRulerPositionMeters, state.zRulerPositionMeters};
  }
  return {state.xRulerPositionMeters, state.yRulerPositionMeters};
}

std::string FormatRulerOffsetLabel(float valueMeters) {
  if (std::fabs(valueMeters) < 0.0001f)
    return "0 m";

  std::ostringstream out;
  if (std::fabs(valueMeters - std::round(valueMeters)) < 0.0001f) {
    out << static_cast<int>(std::lround(valueMeters));
  } else {
    out.setf(std::ios::fixed);
    out.precision(1);
    out << valueMeters;
  }
  out << " m";
  return out.str();
}

CanvasTextStyle BuildRulerLabelStyle(const CanvasStroke &stroke) {
  CanvasTextStyle style;
  style.fontSize = kRulerLabelFontSize;
  style.color = stroke.color;
  return style;
}

float VerticalTickDirection(Viewer2DView view) {
  // In Side view the vertical ruler represents Z, and its ticks must point to
  // the opposite side to match the requested orientation.
  return view == Viewer2DView::Side ? -1.0f : 1.0f;
}

float WorldToScreenX(float worldX, const RulerOverlayViewState &state,
                     float pixelsPerMeter) {
  (void)pixelsPerMeter;
  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  return static_cast<float>(state.width) * 0.5f +
         (worldX + offsetMetersX) * kPixelsPerMeter * state.zoom;
}

float WorldToScreenY(float worldY, const RulerOverlayViewState &state,
                     float pixelsPerMeter) {
  (void)pixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  return static_cast<float>(state.height) * 0.5f -
         (worldY + offsetMetersY) * kPixelsPerMeter * state.zoom;
}
} // namespace

void DrawRulerOverlay(const RulerOverlayViewState &state, bool darkMode) {
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);
  const float yAxis = activeRulers.horizontalAxisMeters;
  const float xAxis = activeRulers.verticalAxisMeters;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;

  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  if (darkMode)
    glColor3f(1.0f, 1.0f, 1.0f);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.0f);

  DrawHorizontalRuler(minX, maxX, yAxis, shortTickMeters, longTickMeters,
                      state.view);
  DrawVerticalRuler(minY, maxY, xAxis, shortTickMeters, longTickMeters,
                    state.view);

  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

void EmitRulerToCanvas(const RulerOverlayViewState &state, bool darkMode,
                       ICanvas2D &canvas) {
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;
  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  auto stroke = BuildRulerStroke(darkMode);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);

  const float xRulerY = activeRulers.horizontalAxisMeters;
  const float yRulerX = activeRulers.verticalAxisMeters;
  canvas.DrawLine(minX, xRulerY, maxX, xRulerY, stroke);
  canvas.DrawLine(yRulerX, minY, yRulerX, maxY, stroke);

  const float startX = std::ceil(minX / kTickStepMeters) * kTickStepMeters;
  const auto textStyle = BuildRulerLabelStyle(stroke);
  for (float x = startX; x <= maxX + 0.0001f; x += kTickStepMeters) {
    const bool isLongTick = IsMeterTick(x);
    const float tick = isLongTick ? longTickMeters : shortTickMeters;
    canvas.DrawLine(x, xRulerY, x, xRulerY + tick, stroke);
    if (isLongTick) {
      const std::string label = FormatRulerOffsetLabel(x - yRulerX);
      canvas.DrawText(x, xRulerY + tick + kRulerLabelOffsetMeters, label,
                      textStyle);
    }
  }

  const float startY = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  const float verticalTickDirection = VerticalTickDirection(state.view);
  for (float y = startY; y <= maxY + 0.0001f; y += kTickStepMeters) {
    const bool isLongTick = IsMeterTick(y);
    const float tick = isLongTick ? longTickMeters : shortTickMeters;
    canvas.DrawLine(yRulerX, y, yRulerX + tick * verticalTickDirection, y,
                    stroke);
    if (isLongTick) {
      const std::string label = FormatRulerOffsetLabel(y - xRulerY);
      canvas.DrawText(
          yRulerX + (tick + kRulerLabelOffsetMeters) * verticalTickDirection,
          y, label, textStyle);
    }
  }
}

std::vector<RulerScreenLabel>
BuildRulerScreenLabels(const RulerOverlayViewState &state) {
  std::vector<RulerScreenLabel> labels;
  if (state.width <= 0 || state.height <= 0 || state.zoom <= 0.0f)
    return labels;

  const float pixelsPerMeter = kPixelsPerMeter * state.zoom;
  if (pixelsPerMeter <= 0.0f)
    return labels;

  const float offsetMetersX = state.offsetPixelsX / kPixelsPerMeter;
  const float offsetMetersY = state.offsetPixelsY / kPixelsPerMeter;
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;
  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;
  const float shortTickMeters = std::max(state.smallTickMeters, 0.01f);
  const float longTickMeters =
      std::max(std::max(state.largeTickMeters, shortTickMeters),
               shortTickMeters);
  const ActiveRulers activeRulers = ResolveActiveRulers(state);
  const float xRulerY = activeRulers.horizontalAxisMeters;
  const float yRulerX = activeRulers.verticalAxisMeters;

  const float startX = std::ceil(minX / kTickStepMeters) * kTickStepMeters;
  for (float x = startX; x <= maxX + 0.0001f; x += kTickStepMeters) {
    if (!IsMeterTick(x))
      continue;
    const float worldY = xRulerY + longTickMeters + kRulerLabelOffsetMeters;
    labels.push_back({WorldToScreenX(x, state, pixelsPerMeter),
                      WorldToScreenY(worldY, state, pixelsPerMeter),
                      FormatRulerOffsetLabel(x - yRulerX), true, false});
  }

  const float startY = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  const float verticalTickDirection = VerticalTickDirection(state.view);
  for (float y = startY; y <= maxY + 0.0001f; y += kTickStepMeters) {
    if (!IsMeterTick(y))
      continue;
    const float worldX = yRulerX +
                         (longTickMeters + kRulerLabelOffsetMeters) *
                             verticalTickDirection;
    labels.push_back({WorldToScreenX(worldX, state, pixelsPerMeter),
                      WorldToScreenY(y, state, pixelsPerMeter),
                      FormatRulerOffsetLabel(y - xRulerY), false, true});
  }
  return labels;
}

} // namespace viewer2d
