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

float WorldXToScreen(float xMeters, int width, float pixelsPerMeter,
                     float offsetMetersX) {
  return static_cast<float>(width) * 0.5f +
         (xMeters + offsetMetersX) * pixelsPerMeter;
}

float WorldYToScreen(float yMeters, int height, float pixelsPerMeter,
                     float offsetMetersY) {
  return static_cast<float>(height) * 0.5f -
         (yMeters + offsetMetersY) * pixelsPerMeter;
}

bool IsMeterTick(float valueMeters) {
  const float rounded = std::round(valueMeters);
  return std::fabs(valueMeters - rounded) < 0.0001f;
}

void DrawHorizontalRuler(float minX, float maxX, int width,
                         float pixelsPerMeter, float offsetMetersX,
                         float yAxis, float shortTickPixels,
                         float longTickPixels) {
  glBegin(GL_LINES);
  glVertex2f(0.0f, yAxis);
  glVertex2f(static_cast<float>(width), yAxis);

  const float startTick = std::ceil(minX / kTickStepMeters) * kTickStepMeters;
  for (float x = startTick; x <= maxX + 0.0001f; x += kTickStepMeters) {
    const float sx = WorldXToScreen(x, width, pixelsPerMeter, offsetMetersX);
    if (sx < -1.0f || sx > static_cast<float>(width) + 1.0f)
      continue;
    const float tick = IsMeterTick(x) ? longTickPixels : shortTickPixels;
    glVertex2f(sx, yAxis);
    glVertex2f(sx, yAxis + tick);
  }
  glEnd();
}

void DrawVerticalRuler(float minY, float maxY, int height,
                       float pixelsPerMeter, float offsetMetersY, float xAxis,
                       float shortTickPixels, float longTickPixels) {
  glBegin(GL_LINES);
  glVertex2f(xAxis, 0.0f);
  glVertex2f(xAxis, static_cast<float>(height));

  const float startTick = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  for (float y = startTick; y <= maxY + 0.0001f; y += kTickStepMeters) {
    const float sy = WorldYToScreen(y, height, pixelsPerMeter, offsetMetersY);
    if (sy < -1.0f || sy > static_cast<float>(height) + 1.0f)
      continue;
    const float tick = IsMeterTick(y) ? longTickPixels : shortTickPixels;
    glVertex2f(xAxis, sy);
    glVertex2f(xAxis + tick, sy);
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
  const float shortTickPixels = shortTickMeters * pixelsPerMeter;
  const float longTickPixels = longTickMeters * pixelsPerMeter;
  const float yAxis =
      WorldYToScreen(state.axisXPositionMeters, state.height, pixelsPerMeter,
                     offsetMetersY);
  const float xAxis =
      WorldXToScreen(state.axisYPositionMeters, state.width, pixelsPerMeter,
                     offsetMetersX);
  const float halfW = static_cast<float>(state.width) * 0.5f / pixelsPerMeter;
  const float halfH = static_cast<float>(state.height) * 0.5f / pixelsPerMeter;

  const float minX = -halfW - offsetMetersX;
  const float maxX = halfW - offsetMetersX;
  const float minY = -halfH - offsetMetersY;
  const float maxY = halfH - offsetMetersY;

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(state.width), 0.0f,
          static_cast<float>(state.height), -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  if (darkMode)
    glColor3f(1.0f, 1.0f, 1.0f);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.0f);

  DrawHorizontalRuler(minX, maxX, state.width, pixelsPerMeter, offsetMetersX,
                      yAxis, shortTickPixels, longTickPixels);
  DrawVerticalRuler(minY, maxY, state.height, pixelsPerMeter, offsetMetersY,
                    xAxis, shortTickPixels, longTickPixels);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

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

  const float xRulerY = state.axisXPositionMeters;
  const float yRulerX = state.axisYPositionMeters;
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
