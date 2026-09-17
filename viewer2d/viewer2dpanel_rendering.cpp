/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Panel-owned OpenGL overlay rendering for the 2D viewer.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "configmanager.h"
#include "render/viewer2d_render_frame_plan.h"
#include "viewer2d_ruler_overlay.h"
#include "viewer2dpanel.h"

#include <cmath>
#include <optional>
#include <vector>

// Builds a stable frame-decision snapshot from UI state and configuration.
viewer2d::render::RenderFramePlan Viewer2DPanel::BuildRenderFramePlan() {
  ConfigManager &cfg = ConfigManager::Get();
  viewer2d::render::RenderFramePlanInput input;
  input.configuredDarkMode = cfg.GetFloat("view2d_dark_mode") != 0.0f;
  input.configuredShowGrid = cfg.GetFloat("grid_show") != 0.0f;
  input.configuredShowRuler = cfg.GetFloat("ruler_show") != 0.0f;
  input.selectionEnabled = m_enableSelection;
  input.pauseHeavyTasks = m_enableSelection && ShouldPauseHeavyTasks();
  input.expensiveVisualInteractionActive = IsExpensiveVisualInteractionActive();
  input.captureRequested = m_captureNextFrame;
  input.captureIncludeGrid = m_captureIncludeGrid;
  input.useSimplifiedFootprints = m_useSimplifiedFootprints;
  if (m_renderOverrides) {
    input.overrides.darkMode = m_renderOverrides->darkMode;
    input.overrides.showGrid = m_renderOverrides->showGrid;
    input.overrides.showRuler = m_renderOverrides->showRuler;
    input.overrides.drawFixtureLabels = m_renderOverrides->drawFixtureLabels;
    input.overrides.forceBottomViewForTopFixtures =
        m_renderOverrides->forceBottomViewForTopFixtures;
    input.overrides.symbolCaptureRenderProfile =
        m_renderOverrides->symbolCaptureRenderProfile;
    input.overrides.symbolCaptureIncludeCoplanarEdges =
        m_renderOverrides->symbolCaptureIncludeCoplanarEdges;
  }
  return viewer2d::render::BuildRenderFramePlan(input);
}

// Builds ruler state from the current viewport and configured ruler profile.
viewer2d::RulerOverlayViewState
Viewer2DPanel::BuildRulerOverlayViewState(int width, int height,
                                          bool useImperialUnits) const {
  ConfigManager &cfg = ConfigManager::Get();
  return viewer2d::render::BuildRulerOverlayViewState(
      {width,
       height,
       m_zoom,
       m_offsetX,
       m_offsetY,
       cfg.GetFloat("ruler_tick_small_m"),
       cfg.GetFloat("ruler_tick_large_m"),
       cfg.GetFloat("ruler_axis_x_position"),
       cfg.GetFloat("ruler_axis_y_position"),
       cfg.GetFloat("ruler_axis_z_position"),
       {cfg.GetFloat("ruler_axis_x_color_r"),
        cfg.GetFloat("ruler_axis_x_color_g"),
        cfg.GetFloat("ruler_axis_x_color_b"), 1.0f},
       {cfg.GetFloat("ruler_axis_y_color_r"),
        cfg.GetFloat("ruler_axis_y_color_g"),
        cfg.GetFloat("ruler_axis_y_color_b"), 1.0f},
       {cfg.GetFloat("ruler_axis_z_color_r"),
        cfg.GetFloat("ruler_axis_z_color_g"),
        cfg.GetFloat("ruler_axis_z_color_b"), 1.0f},
       useImperialUnits,
       m_view});
}

// Draws the layout-edit bounds while keeping OpenGL state panel-owned.
void Viewer2DPanel::DrawLayoutEditOverlay(int width, int height) {
  if (!m_layoutEditAspect)
    return;

  const auto geometry = viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {static_cast<float>(width), static_cast<float>(height),
       *m_layoutEditAspect, m_layoutEditScale,
       m_layoutEditBaseSize
           ? std::optional<float>(m_layoutEditBaseSize->GetWidth())
           : std::nullopt,
       m_layoutEditBaseSize
           ? std::optional<float>(m_layoutEditBaseSize->GetHeight())
           : std::nullopt});
  if (!geometry)
    return;

  if (!m_layoutEditBaseSize || m_layoutEditBaseSize->GetWidth() <= 0 ||
      m_layoutEditBaseSize->GetHeight() <= 0) {
    m_layoutEditBaseSize = wxSize(static_cast<int>(geometry->baseWidth),
                                  static_cast<int>(geometry->baseHeight));
  }

  const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height),
          -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glColor3f(1.0f, 0.0f, 0.0f);
  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(geometry->left, geometry->bottom);
  glVertex2f(geometry->left + geometry->width, geometry->bottom);
  glVertex2f(geometry->left + geometry->width,
             geometry->bottom + geometry->height);
  glVertex2f(geometry->left, geometry->bottom + geometry->height);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

// Draws ruler geometry and submits its screen-space labels and capture
// commands.
void Viewer2DPanel::DrawRulerOverlayFrame(
    const viewer2d::RulerOverlayViewState &rulerState, bool darkMode,
    ICanvas2D *recordingCanvas) {
  viewer2d::DrawRulerOverlay(rulerState, darkMode);
  const auto rulerLabels =
      viewer2d::BuildRulerScreenLabels(rulerState, darkMode);
  if (!rulerLabels.empty()) {
    std::vector<OverlayTextLabel> overlayLabels;
    overlayLabels.reserve(rulerLabels.size());
    for (const auto &label : rulerLabels) {
      overlayLabels.push_back({label.xPixels, label.yPixels, label.text,
                               label.centerOnX, label.centerOnY, 3.0f * m_zoom,
                               true, label.color.r, label.color.g,
                               label.color.b});
    }
    m_controller.DrawOverlayTextLabels(overlayLabels, darkMode);
  }
  if (recordingCanvas)
    viewer2d::EmitRulerToCanvas(rulerState, darkMode, *recordingCanvas);
}
