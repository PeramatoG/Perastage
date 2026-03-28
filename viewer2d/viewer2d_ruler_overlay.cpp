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
constexpr float kLongTickPixels = 16.0f;
constexpr float kShortTickPixels = 9.0f;
constexpr float kAxisPaddingPixels = 2.0f;

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
                         float pixelsPerMeter, float offsetMetersX) {
  const float yAxis = kAxisPaddingPixels;
  glBegin(GL_LINES);
  glVertex2f(0.0f, yAxis);
  glVertex2f(static_cast<float>(width), yAxis);

  const float startTick = std::ceil(minX / kTickStepMeters) * kTickStepMeters;
  for (float x = startTick; x <= maxX + 0.0001f; x += kTickStepMeters) {
    const float sx = WorldXToScreen(x, width, pixelsPerMeter, offsetMetersX);
    if (sx < -1.0f || sx > static_cast<float>(width) + 1.0f)
      continue;
    const float tick = IsMeterTick(x) ? kLongTickPixels : kShortTickPixels;
    glVertex2f(sx, yAxis);
    glVertex2f(sx, yAxis + tick);
  }
  glEnd();
}

void DrawVerticalRuler(float minY, float maxY, int height,
                       float pixelsPerMeter, float offsetMetersY) {
  const float xAxis = kAxisPaddingPixels;
  glBegin(GL_LINES);
  glVertex2f(xAxis, 0.0f);
  glVertex2f(xAxis, static_cast<float>(height));

  const float startTick = std::ceil(minY / kTickStepMeters) * kTickStepMeters;
  for (float y = startTick; y <= maxY + 0.0001f; y += kTickStepMeters) {
    const float sy = WorldYToScreen(y, height, pixelsPerMeter, offsetMetersY);
    if (sy < -1.0f || sy > static_cast<float>(height) + 1.0f)
      continue;
    const float tick = IsMeterTick(y) ? kLongTickPixels : kShortTickPixels;
    glVertex2f(xAxis, sy);
    glVertex2f(xAxis + tick, sy);
  }
  glEnd();
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

  DrawHorizontalRuler(minX, maxX, state.width, pixelsPerMeter,
                      offsetMetersX);
  DrawVerticalRuler(minY, maxY, state.height, pixelsPerMeter, offsetMetersY);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

} // namespace viewer2d
