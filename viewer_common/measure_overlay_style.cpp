#include "measure_overlay_style.h"

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

#include <cmath>

namespace viewer_common {

// Draws a CAD-like measure overlay in the current orthographic screen-space GL context.
void DrawMeasureOverlayStyle(float x0, float y0, float x1, float y1,
                             bool darkMode) {
  float vx = x1 - x0;
  float vy = y1 - y0;
  const float len = std::sqrt(vx * vx + vy * vy);
  if (len < 1.0f)
    return;

  vx /= len;
  vy /= len;
  const float nx = -vy;
  const float ny = vx;
  const float offset = 14.0f;
  const float tx0 = x0 + nx * offset;
  const float ty0 = y0 + ny * offset;
  const float tx1 = x1 + nx * offset;
  const float ty1 = y1 + ny * offset;

  const float cr = darkMode ? 0.95f : 0.92f;
  const float cg = darkMode ? 0.10f : 0.12f;
  const float cb = darkMode ? 0.10f : 0.12f;

  glColor3f(cr, cg, cb);
  glLineWidth(1.0f);
  glBegin(GL_LINES);
  glVertex2f(x0, y0); glVertex2f(tx0, ty0);
  glVertex2f(x1, y1); glVertex2f(tx1, ty1);
  glEnd();

  glLineWidth(2.0f);
  glBegin(GL_LINES);
  glVertex2f(tx0, ty0); glVertex2f(tx1, ty1);
  glEnd();

  const float arrow = 7.0f;
  glBegin(GL_LINES);
  glVertex2f(tx0, ty0); glVertex2f(tx0 + vx * arrow + nx * (arrow * 0.45f),
                                   ty0 + vy * arrow + ny * (arrow * 0.45f));
  glVertex2f(tx0, ty0); glVertex2f(tx0 + vx * arrow - nx * (arrow * 0.45f),
                                   ty0 + vy * arrow - ny * (arrow * 0.45f));
  glVertex2f(tx1, ty1); glVertex2f(tx1 - vx * arrow + nx * (arrow * 0.45f),
                                   ty1 - vy * arrow + ny * (arrow * 0.45f));
  glVertex2f(tx1, ty1); glVertex2f(tx1 - vx * arrow - nx * (arrow * 0.45f),
                                   ty1 - vy * arrow - ny * (arrow * 0.45f));
  glEnd();
}

} // namespace viewer_common
