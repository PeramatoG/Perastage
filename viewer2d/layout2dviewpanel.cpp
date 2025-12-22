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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "layout2dviewpanel.h"
#include "layouts/LayoutManager.h"

#include <algorithm>
#include <cmath>
#include <wx/wx.h>

namespace {
static constexpr float PIXELS_PER_METER = 25.0f;
}

Layout2DViewPanel *Layout2DViewPanel::s_instance = nullptr;

wxBEGIN_EVENT_TABLE(Layout2DViewPanel, wxGLCanvas)
    EVT_PAINT(Layout2DViewPanel::OnPaint)
    EVT_LEFT_DOWN(Layout2DViewPanel::OnMouseDown)
    EVT_LEFT_UP(Layout2DViewPanel::OnMouseUp)
    EVT_MOTION(Layout2DViewPanel::OnMouseMove)
    EVT_MOUSEWHEEL(Layout2DViewPanel::OnMouseWheel)
    EVT_KEY_DOWN(Layout2DViewPanel::OnKeyDown)
    EVT_ENTER_WINDOW(Layout2DViewPanel::OnMouseEnter)
    EVT_LEAVE_WINDOW(Layout2DViewPanel::OnMouseLeave)
    EVT_MOUSE_CAPTURE_LOST(Layout2DViewPanel::OnCaptureLost)
    EVT_SIZE(Layout2DViewPanel::OnResize)
wxEND_EVENT_TABLE()

Layout2DViewPanel::Layout2DViewPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE) {
  SetInstance(this);
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_glContext = new wxGLContext(this);
}

Layout2DViewPanel::~Layout2DViewPanel() { delete m_glContext; }

Layout2DViewPanel *Layout2DViewPanel::Instance() { return s_instance; }

void Layout2DViewPanel::SetInstance(Layout2DViewPanel *panel) {
  s_instance = panel;
}

void Layout2DViewPanel::SetActiveLayout(
    const std::string &layoutName,
    const layouts::Layout2DViewDefinition &view) {
  m_layoutName = layoutName;
  ApplyViewDefinition(view);
}

void Layout2DViewPanel::UpdateScene(bool reload) {
  if (reload)
    m_controller.Update();
  Refresh();
}

void Layout2DViewPanel::SetView(Viewer2DView view) {
  m_view = view;
  m_viewDefinition.camera.view = static_cast<int>(view);
  SyncCameraState();
  Refresh();
}

const layouts::Layout2DViewRenderOptions &
Layout2DViewPanel::GetRenderOptions() const {
  return m_viewDefinition.renderOptions;
}

void Layout2DViewPanel::UpdateRenderOptions(
    const std::function<void(layouts::Layout2DViewRenderOptions &)> &updater) {
  updater(m_viewDefinition.renderOptions);
  m_renderMode = static_cast<Viewer2DRenderMode>(
      m_viewDefinition.renderOptions.renderMode);
  Refresh();
}

std::unordered_set<std::string> Layout2DViewPanel::GetHiddenLayers() const {
  return m_hiddenLayers;
}

void Layout2DViewPanel::SetHiddenLayers(
    const std::unordered_set<std::string> &layers) {
  m_hiddenLayers = layers;
  m_viewDefinition.layers.hiddenLayers.assign(layers.begin(), layers.end());
  std::sort(m_viewDefinition.layers.hiddenLayers.begin(),
            m_viewDefinition.layers.hiddenLayers.end());
  m_controller.SetHiddenLayersOverride(m_hiddenLayers);
  Refresh();
}

layouts::Layout2DViewDefinition Layout2DViewPanel::GetViewDefinition() const {
  layouts::Layout2DViewDefinition view = m_viewDefinition;
  int w = 0;
  int h = 0;
  const_cast<Layout2DViewPanel *>(this)->GetClientSize(&w, &h);
  view.camera.offsetPixelsX = m_offsetX;
  view.camera.offsetPixelsY = m_offsetY;
  view.camera.zoom = m_zoom;
  view.camera.view = static_cast<int>(m_view);
  view.camera.viewportWidth = w;
  view.camera.viewportHeight = h;
  view.renderOptions.renderMode = static_cast<int>(m_renderMode);
  view.layers.hiddenLayers.assign(m_hiddenLayers.begin(), m_hiddenLayers.end());
  std::sort(view.layers.hiddenLayers.begin(), view.layers.hiddenLayers.end());
  return view;
}

void Layout2DViewPanel::ApplyViewDefinition(
    const layouts::Layout2DViewDefinition &view) {
  m_viewDefinition = view;
  m_offsetX = view.camera.offsetPixelsX;
  m_offsetY = view.camera.offsetPixelsY;
  m_zoom = view.camera.zoom;
  m_renderMode =
      static_cast<Viewer2DRenderMode>(view.renderOptions.renderMode);
  m_view = static_cast<Viewer2DView>(view.camera.view);
  m_hiddenLayers.clear();
  for (const auto &layer : view.layers.hiddenLayers)
    m_hiddenLayers.insert(layer);
  m_controller.SetHiddenLayersOverride(m_hiddenLayers);
  Refresh();
}

void Layout2DViewPanel::InitGL() {
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

void Layout2DViewPanel::UpdateFrameFromLayout() {
  if (m_layoutName.empty())
    return;

  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    if (layout.name != m_layoutName)
      continue;

    if (layout.view2dViews.empty())
      return;

    const layouts::Layout2DViewDefinition *selected = nullptr;
    for (const auto &view : layout.view2dViews) {
      if (view.camera.view == static_cast<int>(m_view)) {
        selected = &view;
        break;
      }
    }
    if (!selected)
      selected = &layout.view2dViews.front();

    m_viewDefinition.frame = selected->frame;
    return;
  }
}

void Layout2DViewPanel::SyncCameraState() {
  int w = 0;
  int h = 0;
  GetClientSize(&w, &h);
  m_viewDefinition.camera.offsetPixelsX = m_offsetX;
  m_viewDefinition.camera.offsetPixelsY = m_offsetY;
  m_viewDefinition.camera.zoom = m_zoom;
  m_viewDefinition.camera.viewportWidth = w;
  m_viewDefinition.camera.viewportHeight = h;
  m_viewDefinition.camera.view = static_cast<int>(m_view);
}

void Layout2DViewPanel::DrawFrameOverlay(int width, int height) {
  const float frameW = static_cast<float>(m_viewDefinition.frame.width);
  const float frameH = static_cast<float>(m_viewDefinition.frame.height);
  if (frameW <= 0.0f || frameH <= 0.0f)
    return;

  const float ratio = frameW / frameH;
  float maxW = static_cast<float>(width) * 0.9f;
  float maxH = static_cast<float>(height) * 0.9f;
  float rectW = maxW;
  float rectH = rectW / ratio;
  if (rectH > maxH) {
    rectH = maxH;
    rectW = rectH * ratio;
  }

  const float x = (static_cast<float>(width) - rectW) * 0.5f;
  const float y = (static_cast<float>(height) - rectH) * 0.5f;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glColor3f(1.0f, 0.0f, 0.0f);
  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(x, y);
  glVertex2f(x + rectW, y);
  glVertex2f(x + rectW, y + rectH);
  glVertex2f(x, y + rectH);
  glEnd();
  glLineWidth(1.0f);
  glEnable(GL_DEPTH_TEST);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void Layout2DViewPanel::Render() {
  int w = 0;
  int h = 0;
  GetClientSize(&w, &h);

  UpdateFrameFromLayout();

  glViewport(0, 0, w, h);

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
  case Viewer2DView::Bottom:
    gluLookAt(0.0, 0.0, -10.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    break;
  case Viewer2DView::Front:
    gluLookAt(0.0, -10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  case Viewer2DView::Side:
    gluLookAt(-10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    break;
  }

  const auto &options = m_viewDefinition.renderOptions;
  m_controller.SetDarkMode(options.darkMode);
  if (options.darkMode)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  else
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_controller.SetHiddenLayersOverride(m_hiddenLayers);
  m_controller.RenderScene(true, m_renderMode, m_view, options.showGrid,
                           options.gridStyle, options.gridColorR,
                           options.gridColorG, options.gridColorB,
                           options.gridDrawAbove);

  m_controller.DrawAllFixtureLabels(w, h, m_zoom, m_view, options);

  DrawFrameOverlay(w, h);

  glFlush();
  SwapBuffers();
}

void Layout2DViewPanel::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxPaintDC dc(this);
  InitGL();
  Render();
}

void Layout2DViewPanel::OnMouseDown(wxMouseEvent &event) {
  if (event.LeftDown()) {
    CaptureMouse();
    m_dragging = true;
    m_lastMousePos = event.GetPosition();
  }
}

void Layout2DViewPanel::OnMouseUp(wxMouseEvent &event) {
  if (event.LeftUp() && m_dragging) {
    m_dragging = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void Layout2DViewPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  m_dragging = false;
}

void Layout2DViewPanel::OnMouseMove(wxMouseEvent &event) {
  if (m_dragging && event.Dragging()) {
    wxPoint pos = event.GetPosition();
    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;
    m_offsetX += dx / m_zoom;
    m_offsetY += dy / m_zoom;
    m_lastMousePos = pos;
    SyncCameraState();
    Refresh();
  }
}

void Layout2DViewPanel::OnMouseWheel(wxMouseEvent &event) {
  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  float factor = std::pow(1.1f, steps);
  m_zoom *= factor;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  SyncCameraState();
  Refresh();
}

void Layout2DViewPanel::OnKeyDown(wxKeyEvent &event) {
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
  SyncCameraState();
  Refresh();
}

void Layout2DViewPanel::OnMouseEnter(wxMouseEvent &event) {
  m_mouseInside = true;
  SetFocus();
  event.Skip();
}

void Layout2DViewPanel::OnMouseLeave(wxMouseEvent &event) {
  m_mouseInside = false;
  event.Skip();
}

void Layout2DViewPanel::OnResize(wxSizeEvent &event) {
  SyncCameraState();
  Refresh();
  event.Skip();
}
