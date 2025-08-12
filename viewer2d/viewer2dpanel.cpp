/*
 * File: viewer2dpanel.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of a top-down OpenGL viewer sharing 3D models.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glew.h>
#include <GL/glu.h>

#include "viewer2dpanel.h"
#include <algorithm>
#include <cmath>

// Pixels per meter at default zoom level.
static constexpr float PIXELS_PER_METER = 25.0f;

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
  gluLookAt(0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  m_controller.RenderScene(true, m_renderMode);

  // Draw labels for all fixtures after rendering the scene so they appear on
  // top of geometry. Scale the label size with the current zoom so they behave
  // like regular scene objects instead of remaining a constant screen size.
  m_controller.DrawAllFixtureLabels(w, h, m_zoom);

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
      m_offsetY += panStep;
    break;
  case WXK_DOWN:
    if (alt)
      m_zoom /= 1.1f;
    else
      m_offsetY -= panStep;
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
