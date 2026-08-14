#include "magnet_anchor_overlay.h"

#include <GL/glew.h>
#include <cmath>

namespace viewer_common {

// Draws continuous fixture attachment polylines in framebuffer coordinates.
void DrawFixtureAttachmentPathOverlay(
    const std::vector<FixtureAttachmentScreenPath> &paths,
    int framebufferWidth, int framebufferHeight) {
  if (paths.empty())
    return;
  const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, framebufferWidth, 0.0f, framebufferHeight, -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glColor3f(1.0f, 0.1f, 0.1f);
  glLineWidth(3.0f);
  for (const auto &path : paths) {
    glBegin(GL_LINE_STRIP);
    for (const auto &point : path.points)
      glVertex2f(point[0], point[1]);
    glEnd();
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

// Draws all Magnet anchor references in framebuffer coordinates.
void DrawMagnetAnchorOverlay(
    const std::vector<MagnetAnchorScreenReference> &references,
    int framebufferWidth, int framebufferHeight) {
  constexpr int kSegments = 24;
  constexpr float kRadius = 8.0f;
  constexpr float kPi = 3.14159265358979323846f;
  if (references.empty())
    return;
  GLfloat previousLineWidth = 1.0f;
  GLfloat previousColor[4] = {};
  glGetFloatv(GL_LINE_WIDTH, &previousLineWidth);
  glGetFloatv(GL_CURRENT_COLOR, previousColor);
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

  glLineWidth(3.0f);
  glColor3f(1.0f, 0.1f, 0.1f);
  glBegin(GL_LINES);
  for (const auto &reference : references) {
    if (!reference.hasDirection)
      continue;
    const float length = std::hypot(reference.directionX, reference.directionY);
    if (length < 0.01f)
      continue;
    constexpr float kDirectionHalfLength = 14.0f;
    const float dx = reference.directionX / length * kDirectionHalfLength;
    const float dy = reference.directionY / length * kDirectionHalfLength;
    glVertex2f(reference.x - dx, reference.y - dy);
    glVertex2f(reference.x + dx, reference.y + dy);
  }
  glEnd();

  for (const auto &reference : references) {
    glColor3f(1.0f, 0.1f, 0.1f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < kSegments; ++index) {
      const float angle = 2.0f * kPi * index / kSegments;
      glVertex2f(reference.x + std::cos(angle) * kRadius,
                 reference.y + std::sin(angle) * kRadius);
    }
    glEnd();
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
  glLineWidth(previousLineWidth);
  glColor4fv(previousColor);
}

} // namespace viewer_common
