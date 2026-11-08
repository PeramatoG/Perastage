/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GL/glew.h>

#include "configmanager.h"
#include "position_insertion_overlay.h"
#include "selection_movement_settings.h"
#include "viewer2dpanel.h"

void Viewer2DPanel::DrawInsertionPointOverlay(int width, int height,
                                              bool onscreenFrame,
                                              bool captureActive) {
  if (!viewer2d::ShowInsertionPointOverlay(onscreenFrame, m_enableSelection,
                                           captureActive, m_forceOffscreenRender))
    return;
  const ConfigManager &config = ConfigManager::Get();
  const auto snapshot = position_selection::Read(
      config.GetScene(), position_selection::SelectionFrom(config),
      selection_movement_settings::LoadInteractiveTransformPolicy(config));
  const auto markers = viewer2d::BuildInsertionPointMarkers(
      snapshot, {width, height, m_zoom, m_offsetX, m_offsetY, m_view});
  if (markers.empty()) return;

  glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT |
                GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  // Seven physical framebuffer pixels, independent of zoom and logical DPI.
  for (const auto &marker : markers) {
    const auto square = [&](float radius, float r, float g, float b) {
      glColor3f(r, g, b);
      glBegin(GL_QUADS);
      glVertex2f(marker.x - radius, marker.y - radius);
      glVertex2f(marker.x + radius, marker.y - radius);
      glVertex2f(marker.x + radius, marker.y + radius);
      glVertex2f(marker.x - radius, marker.y + radius);
      glEnd();
    };
    square(3.5f, 1.0f, 1.0f, 1.0f);
    square(2.5f, 1.0f, 0.0f, 0.0f);
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopAttrib();
}
