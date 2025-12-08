/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * File: viewer2dpanel.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of a top-down OpenGL viewer sharing 3D models.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "viewer2dpanel.h"
#include "configmanager.h"
#include "canvas2d.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Pixels per meter at default zoom level.
static constexpr float PIXELS_PER_METER = 25.0f;

namespace {
CanvasStroke MakeGridStroke(float r, float g, float b) {
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = 1.0f;
  return stroke;
}

// Emit grid primitives to a canvas so the command buffer records the same
// visual information shown by the OpenGL renderer. Coordinates are expressed in
// the 2D world space after the camera orientation has been applied.
void EmitGrid(ICanvas2D &canvas, int style, Viewer2DView view, float r, float g,
              float b) {
  const float size = 20.0f;
  const float step = 1.0f;
  auto stroke = MakeGridStroke(r, g, b);

  if (style == 0) {
    for (float i = -size; i <= size; i += step) {
      switch (view) {
      case Viewer2DView::Top:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      case Viewer2DView::Front:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      case Viewer2DView::Side:
        canvas.DrawLine(i, -size, i, size, stroke);
        canvas.DrawLine(-size, i, size, i, stroke);
        break;
      }
    }
  } else if (style == 1) {
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        std::vector<float> pt = {x, y};
        canvas.DrawCircle(pt[0], pt[1], 0.05f, stroke, nullptr);
      }
    }
  } else {
    float half = step * 0.1f;
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        canvas.DrawLine(x - half, y, x + half, y, stroke);
        canvas.DrawLine(x, y - half, x, y + half, stroke);
      }
    }
  }
}
} // namespace

namespace {
Viewer2DPanel *g_instance = nullptr;
}

wxBEGIN_EVENT_TABLE(Viewer2DPanel, wxGLCanvas) EVT_PAINT(Viewer2DPanel::OnPaint)
    EVT_LEFT_DOWN(Viewer2DPanel::OnMouseDown) EVT_LEFT_UP(
        Viewer2DPanel::OnMouseUp) EVT_MOTION(Viewer2DPanel::OnMouseMove)
        EVT_MOUSEWHEEL(Viewer2DPanel::OnMouseWheel)
            EVT_KEY_DOWN(Viewer2DPanel::OnKeyDown)
                EVT_ENTER_WINDOW(Viewer2DPanel::OnMouseEnter)
                    EVT_LEAVE_WINDOW(Viewer2DPanel::OnMouseLeave)
                        EVT_MOUSE_CAPTURE_LOST(Viewer2DPanel::OnCaptureLost)
                            EVT_SIZE(Viewer2DPanel::OnResize) wxEND_EVENT_TABLE()

                            Viewer2DPanel::Viewer2DPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_glContext = new wxGLContext(this);
}

Viewer2DPanel::~Viewer2DPanel() { delete m_glContext; }

Viewer2DPanel *Viewer2DPanel::Instance() { return g_instance; }

void Viewer2DPanel::SetInstance(Viewer2DPanel *panel) { g_instance = panel; }

void Viewer2DPanel::UpdateScene(bool reload) {
  if (reload)
    m_controller.Update();
  Refresh();
}

void Viewer2DPanel::SetRenderMode(Viewer2DRenderMode mode) {
  m_renderMode = mode;
  Refresh();
}

void Viewer2DPanel::SetView(Viewer2DView view) {
  m_view = view;
  Refresh();
}

void Viewer2DPanel::SetLayerColor(const std::string &layer,
                                  const std::string &hex) {
  // Forward the updated color to the shared controller so the 2D view
  // reflects the user's choice immediately.
  m_controller.SetLayerColor(layer, hex);
}

void Viewer2DPanel::LoadViewFromConfig() {
  ConfigManager &cfg = ConfigManager::Get();
  m_offsetX = cfg.GetFloat("view2d_offset_x");
  m_offsetY = cfg.GetFloat("view2d_offset_y");
  m_zoom = cfg.GetFloat("view2d_zoom");
  m_renderMode = static_cast<Viewer2DRenderMode>(
      static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_view = static_cast<Viewer2DView>(
      static_cast<int>(cfg.GetFloat("view2d_view")));
}

void Viewer2DPanel::SaveViewToConfig() const {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("view2d_offset_x", m_offsetX);
  cfg.SetFloat("view2d_offset_y", m_offsetY);
  cfg.SetFloat("view2d_zoom", m_zoom);
  cfg.SetFloat("view2d_render_mode", static_cast<float>(m_renderMode));
  cfg.SetFloat("view2d_view", static_cast<float>(m_view));
}

void Viewer2DPanel::RequestFrameCapture() { m_captureNextFrame = true; }

void Viewer2DPanel::InitGL() {
  SetCurrent(*m_glContext);
  if (!m_glInitialized) {
    glewExperimental = GL_TRUE;
    glewInit();
    m_controller.InitializeGL();
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    m_glInitialized = true;
  }
}

void Viewer2DPanel::Render() {
  int w, h;
  GetClientSize(&w, &h);

  glViewport(0, 0, w, h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float ppm = PIXELS_PER_METER * m_zoom;
  float halfW = static_cast<float>(w) / ppm * 0.5f;
  float halfH = static_cast<float>(h) / ppm * 0.5f;
  float offX = m_offsetX / PIXELS_PER_METER;
  float offY = m_offsetY / PIXELS_PER_METER;
  glOrtho(-halfW - offX, halfW - offX, -halfH - offY, halfH - offY, -100.0f,
          100.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  switch (m_view) {
  case Viewer2DView::Top:
    gluLookAt(0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    break;
  case Viewer2DView::Front:
    gluLookAt(0.0, -10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  case Viewer2DView::Side:
    gluLookAt(-10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  }

  ConfigManager &cfg = ConfigManager::Get();
  bool showGrid = cfg.GetFloat("grid_show") != 0.0f;
  int gridStyle = static_cast<int>(cfg.GetFloat("grid_style"));
  float gridR = cfg.GetFloat("grid_color_r");
  float gridG = cfg.GetFloat("grid_color_g");
  float gridB = cfg.GetFloat("grid_color_b");
  bool drawAbove = cfg.GetFloat("grid_draw_above") != 0.0f;

  std::unique_ptr<ICanvas2D> recordingCanvas;
  if (m_captureNextFrame) {
    m_lastCapturedFrame.Clear();
    recordingCanvas = CreateRecordingCanvas(m_lastCapturedFrame);
    CanvasTransform transform{};
    transform.scale = 1.0f;
    transform.offsetX = -offX;
    transform.offsetY = -offY;
    recordingCanvas->BeginFrame();
    recordingCanvas->SetTransform(transform);
    m_controller.SetCaptureCanvas(recordingCanvas.get(), m_view);
  } else {
    m_controller.SetCaptureCanvas(nullptr, m_view);
  }

  m_controller.RenderScene(true, m_renderMode, m_view, showGrid, gridStyle,
                           gridR, gridG, gridB, drawAbove);

  // Draw labels for all fixtures after rendering the scene so they appear on
  // top of geometry. Scale the label size with the current zoom so they behave
  // like regular scene objects instead of remaining a constant screen size.
  m_controller.DrawAllFixtureLabels(w, h, m_zoom);

  if (recordingCanvas) {
    recordingCanvas->EndFrame();
    m_captureNextFrame = false;
    m_controller.SetCaptureCanvas(nullptr, m_view);
  }

  glFlush();
  SwapBuffers();
}

void Viewer2DPanel::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxPaintDC dc(this);
  InitGL();
  Render();
}

void Viewer2DPanel::OnMouseDown(wxMouseEvent &event) {
  if (event.LeftDown()) {
    CaptureMouse();
    m_dragging = true;
    m_lastMousePos = event.GetPosition();
  }
}

void Viewer2DPanel::OnMouseUp(wxMouseEvent &event) {
  if (event.LeftUp() && m_dragging) {
    m_dragging = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void Viewer2DPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(event)) {
  m_dragging = false;
}

void Viewer2DPanel::OnMouseMove(wxMouseEvent &event) {
  if (m_dragging && event.Dragging()) {
    wxPoint pos = event.GetPosition();
    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;
    m_offsetX += dx / m_zoom;
    m_offsetY += dy / m_zoom;
    m_lastMousePos = pos;
    Refresh();
  }
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent &event) {
  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  float factor = std::pow(1.1f, steps);
  m_zoom *= factor;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  Refresh();
}

void Viewer2DPanel::OnKeyDown(wxKeyEvent &event) {
  if (!m_mouseInside) {
    event.Skip();
    return;
  }

  bool alt = event.AltDown();
  float panStep = 10.0f / m_zoom;

  switch (event.GetKeyCode()) {
  case WXK_LEFT:
    if (alt)
      m_zoom *= 1.1f;
    else
      m_offsetX += panStep;
    break;
  case WXK_RIGHT:
    if (alt)
      m_zoom /= 1.1f;
    else
      m_offsetX -= panStep;
    break;
  case WXK_UP:
    if (alt)
      m_zoom *= 1.1f;
    else
      m_offsetY -= panStep;
    break;
  case WXK_DOWN:
    if (alt)
      m_zoom /= 1.1f;
    else
      m_offsetY += panStep;
    break;
  default:
    event.Skip();
    return;
  }

  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  Refresh();
}

void Viewer2DPanel::OnMouseEnter(wxMouseEvent &event) {
  m_mouseInside = true;
  SetFocus();
  event.Skip();
}

void Viewer2DPanel::OnMouseLeave(wxMouseEvent &event) {
  m_mouseInside = false;
  event.Skip();
}

void Viewer2DPanel::OnResize(wxSizeEvent &event) {
  Refresh();
  event.Skip();
}
