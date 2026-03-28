#include "viewer2d_ruler_overlay.h"

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

#include <algorithm>
#include <cmath>

namespace viewer2d {
namespace {
constexpr float kPixelsPerMeter = 25.0f;
constexpr float kTickStepMeters = 0.5f;

bool IsMeterTick(float valueMeters) {
  const float rounded = std::round(valueMeters);
  return std::fabs(valueMeters - rounded) < 0.0001f;
}

void DrawHorizontalRuler(float minX, float maxX, float yAxis,
                         float shortTickMeters, float longTickMeters) {
  glBegin(GL_LINES);
  glVertex2f(minX, yAxis);
  glVertex2f(maxX, yAxis);

  const float startTick = std::ceil(minX / kTickStepMeters) * kTickStepMeters;
  for (float x = startTick; x <= maxX + 0.0001f; x += kTickStepMeters) {
    const float tick = IsMeterTick(x) ? longTickMeters : shortTickMeters;
    glVertex2f(x, yAxis);
    glVertex2f(x, yAxis + tick);
  }
  glEnd();
}

void DrawVerticalRuler(float minY, float maxY, float xAxis,
                       float shortTickMeters, float longTickMeters) {
  glBegin(GL_LINES);
  glVertex2f(xAxis, minY);
  glVertex2f(xAxis, maxY);

  const float startTick = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  for (float y = startTick; y <= maxY + 0.0001f; y += kTickStepMeters) {
    const float tick = IsMeterTick(y) ? longTickMeters : shortTickMeters;
    glVertex2f(xAxis, y);
    glVertex2f(xAxis + tick, y);
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

  DrawHorizontalRuler(minX, maxX, yAxis, shortTickMeters, longTickMeters);
  DrawVerticalRuler(minY, maxY, xAxis, shortTickMeters, longTickMeters);

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
  for (float x = startX; x <= maxX + 0.0001f; x += kTickStepMeters) {
    const float tick = IsMeterTick(x) ? longTickMeters : shortTickMeters;
    canvas.DrawLine(x, xRulerY, x, xRulerY + tick, stroke);
  }

  const float startY = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  for (float y = startY; y <= maxY + 0.0001f; y += kTickStepMeters) {
    const float tick = IsMeterTick(y) ? longTickMeters : shortTickMeters;
    canvas.DrawLine(yRulerX, y, yRulerX + tick, y, stroke);
  }
}

} // namespace viewer2d
