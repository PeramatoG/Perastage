#include "magnet_anchor_overlay.h"

#include <GL/glew.h>
#include <array>
#include <cmath>

namespace viewer_common {

// Draws paired Magnet anchor references in framebuffer coordinates.
void DrawMagnetAnchorOverlay(float sourceX, float sourceY, float targetX,
                             float targetY, int framebufferWidth,
                             int framebufferHeight, bool darkMode) {
  constexpr int kSegments = 24;
  constexpr float kRadius = 8.0f;
  constexpr float kPi = 3.14159265358979323846f;
  const float outline = darkMode ? 0.1f : 0.15f;
  const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(framebufferWidth), 0.0f,
          static_cast<float>(framebufferHeight), -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glLineWidth(2.0f);
  glColor3f(0.1f, 0.85f, 1.0f);
  glBegin(GL_LINES);
  glVertex2f(sourceX, sourceY);
  glVertex2f(targetX, targetY);
  glEnd();

  const std::array<std::array<float, 2>, 2> points{
      std::array<float, 2>{sourceX, sourceY},
      std::array<float, 2>{targetX, targetY}};
  for (const auto &point : points) {
    glColor3f(outline, outline, outline);
    glLineWidth(4.0f);
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < kSegments; ++index) {
      const float angle = 2.0f * kPi * index / kSegments;
      glVertex2f(point[0] + std::cos(angle) * kRadius,
                 point[1] + std::sin(angle) * kRadius);
    }
    glEnd();
    glColor3f(0.1f, 0.85f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < kSegments; ++index) {
      const float angle = 2.0f * kPi * index / kSegments;
      glVertex2f(point[0] + std::cos(angle) * kRadius,
                 point[1] + std::sin(angle) * kRadius);
    }
    glEnd();
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

} // namespace viewer_common
