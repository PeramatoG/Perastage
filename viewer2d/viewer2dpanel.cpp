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
#include <map>
#include <sstream>
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
      case Viewer2DView::Bottom:
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

std::string BuildFixtureDebugReport(const CommandBuffer &buffer,
                                    const std::string &debugKey) {
  if (debugKey.empty())
    return {};

  size_t polygonCount = 0;
  size_t filledPolygons = 0;
  size_t strokedPolygons = 0;
  std::map<int, size_t> histogram;

  auto getMeta = [&](size_t idx) -> CommandMetadata {
    if (idx < buffer.metadata.size())
      return buffer.metadata[idx];
    return {};
  };

  for (size_t i = 0; i < buffer.commands.size(); ++i) {
    if (i >= buffer.sources.size())
      break;
    if (buffer.sources[i] != debugKey)
      continue;

    const auto &cmd = buffer.commands[i];
    const auto meta = getMeta(i);
    auto addEntry = [&](int vertices) {
      ++polygonCount;
      ++histogram[vertices];
      if (meta.hasFill)
        ++filledPolygons;
      if (meta.hasStroke)
        ++strokedPolygons;
    };

    if (std::holds_alternative<PolygonCommand>(cmd)) {
      const auto &poly = std::get<PolygonCommand>(cmd);
      addEntry(static_cast<int>(poly.points.size() / 2));
    } else if (std::holds_alternative<RectangleCommand>(cmd)) {
      addEntry(4);
    }
  }

  if (polygonCount == 0)
    return {};

  std::ostringstream out;
  out << "Fixture capture debug ['" << debugKey << "']: polygons="
      << polygonCount << ", filled=" << filledPolygons
      << ", stroked=" << strokedPolygons << "\nVertex histogram:";
  for (const auto &[verts, count] : histogram)
    out << ' ' << verts << "->" << count;

  return out.str();
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

                            Viewer2DPanel::Viewer2DPanel(wxWindow *parent,
                                                         bool allowOffscreenRender)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE),
      m_allowOffscreenRender(allowOffscreenRender) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_glContext = new wxGLContext(this);
}

Viewer2DPanel::~Viewer2DPanel() {
  if (g_instance == this)
    g_instance = nullptr;
  delete m_glContext;
}

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

void Viewer2DPanel::CaptureFrameAsync(
    std::function<void(CommandBuffer, Viewer2DViewState)> callback,
    bool useSimplifiedFootprints, bool includeGridInCapture) {
  m_captureCallback = std::move(callback);
  m_useSimplifiedFootprints = useSimplifiedFootprints;
  if (!m_useSimplifiedFootprints &&
      (m_view == Viewer2DView::Front || m_view == Viewer2DView::Side)) {
    // Front/side printouts should still use cached fixture symbols so repeated
    // fixtures are emitted as reusable PDF symbols like in top/bottom views.
    m_useSimplifiedFootprints = true;
  }
  m_captureIncludeGrid = includeGridInCapture;
  RequestFrameCapture();
  Refresh();
}

void Viewer2DPanel::CaptureFrameNow(
    std::function<void(CommandBuffer, Viewer2DViewState)> callback,
    bool useSimplifiedFootprints, bool includeGridInCapture) {
  CaptureFrameAsync(std::move(callback), useSimplifiedFootprints,
                    includeGridInCapture);
  if (m_allowOffscreenRender) {
    m_forceOffscreenRender = true;
    InitGL();
    Render();
    m_forceOffscreenRender = false;
    return;
  }
  if (IsShownOnScreen()) {
    Update();
  } else {
    m_forceOffscreenRender = true;
    InitGL();
    Render();
    m_forceOffscreenRender = false;
  }
}

void Viewer2DPanel::CaptureFrameNowWithBitmap(
    std::function<void(CommandBuffer, Viewer2DViewState, wxBitmap)> callback,
    bool useSimplifiedFootprints, bool includeGridInCapture) {
  CaptureFrameNow(
      [this, callback = std::move(callback)](CommandBuffer buffer,
                                             Viewer2DViewState state) mutable {
        wxBitmap bitmap = ReadBackBitmap(state.viewportWidth,
                                         state.viewportHeight);
        callback(std::move(buffer), state, bitmap);
      },
      useSimplifiedFootprints, includeGridInCapture);
}

void Viewer2DPanel::SetLayoutEditOverlay(std::optional<float> aspectRatio) {
  m_layoutEditAspect = aspectRatio;
  Refresh();
}

Viewer2DViewState Viewer2DPanel::GetViewState() const {
  int w = 0;
  int h = 0;
  const_cast<Viewer2DPanel *>(this)->GetClientSize(&w, &h);

  Viewer2DViewState state{};
  state.offsetPixelsX = m_offsetX;
  state.offsetPixelsY = m_offsetY;
  state.zoom = m_zoom;
  state.viewportWidth = w;
  state.viewportHeight = h;
  state.view = m_view;
  return state;
}

std::shared_ptr<const SymbolDefinitionSnapshot>
Viewer2DPanel::GetBottomSymbolCacheSnapshot() const {
  return m_controller.GetBottomSymbolCacheSnapshot();
}

void Viewer2DPanel::InitGL() {
  if (!IsShownOnScreen() && !m_forceOffscreenRender &&
      !m_allowOffscreenRender) {
    return;
  }
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

  ConfigManager &cfg = ConfigManager::Get();
  bool darkMode = cfg.GetFloat("view2d_dark_mode") != 0.0f;
  m_controller.SetDarkMode(darkMode);
  if (darkMode)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  else
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bool showGrid = cfg.GetFloat("grid_show") != 0.0f;
  int gridStyle = static_cast<int>(cfg.GetFloat("grid_style"));
  float gridR = cfg.GetFloat("grid_color_r");
  float gridG = cfg.GetFloat("grid_color_g");
  float gridB = cfg.GetFloat("grid_color_b");
  bool drawAbove = cfg.GetFloat("grid_draw_above") != 0.0f;

  std::unique_ptr<ICanvas2D> recordingCanvas;
  if (m_captureNextFrame) {
    m_lastCapturedFrame.Clear();
    recordingCanvas = CreateRecordingCanvas(m_lastCapturedFrame, false);
    // The recorded commands operate in the same world-space coordinates used by
    // the OpenGL renderer. We keep the transform identity so the exporter can
    // apply the viewport offsets/zoom exactly once using the captured
    // ViewState, matching the 2D on-screen projection.
    CanvasTransform transform{};
    transform.scale = 1.0f;
    transform.offsetX = 0.0f;
    transform.offsetY = 0.0f;
    recordingCanvas->BeginFrame();
    recordingCanvas->SetTransform(transform);
    m_controller.SetCaptureCanvas(recordingCanvas.get(), m_view,
                                  m_captureIncludeGrid,
                                  m_useSimplifiedFootprints);
  } else {
    m_controller.SetCaptureCanvas(nullptr, m_view);
  }

  m_controller.RenderScene(true, m_renderMode, m_view, showGrid, gridStyle,
                           gridR, gridG, gridB, drawAbove);

  // Draw labels for all fixtures after rendering the scene so they appear on
  // top of geometry. Scale the label size with the current zoom so they behave
  // like regular scene objects instead of remaining a constant screen size.
  m_controller.DrawAllFixtureLabels(w, h, m_zoom);

  if (m_layoutEditAspect && *m_layoutEditAspect > 0.0f) {
    float aspect = *m_layoutEditAspect;
    float padding = static_cast<float>(std::min(w, h)) * 0.1f;
    float maxWidth = static_cast<float>(w) - padding * 2.0f;
    float maxHeight = static_cast<float>(h) - padding * 2.0f;
    float targetWidth = maxWidth;
    float targetHeight = targetWidth / aspect;
    if (targetHeight > maxHeight) {
      targetHeight = maxHeight;
      targetWidth = targetHeight * aspect;
    }
    float left = (static_cast<float>(w) - targetWidth) * 0.5f;
    float bottom = (static_cast<float>(h) - targetHeight) * 0.5f;

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
      glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h), -1.0f,
            1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 0.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, bottom);
    glVertex2f(left + targetWidth, bottom);
    glVertex2f(left + targetWidth, bottom + targetHeight);
    glVertex2f(left, bottom + targetHeight);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if (depthEnabled)
      glEnable(GL_DEPTH_TEST);
  }

  if (recordingCanvas) {
    recordingCanvas->EndFrame();
    m_captureNextFrame = false;
    m_controller.SetCaptureCanvas(nullptr, m_view);

    m_lastFixtureDebugReport.clear();
    auto debugKey =
        ConfigManager::Get().GetValue("print_viewer2d_fixture_debug_key");
    if (!debugKey || debugKey->empty()) {
      debugKey = ConfigManager::Get().GetValue("print_plan_fixture_debug_key");
    }
    if (debugKey && !debugKey->empty()) {
      m_lastFixtureDebugReport =
          BuildFixtureDebugReport(m_lastCapturedFrame, *debugKey);
        if (!m_lastFixtureDebugReport.empty()) {
          wxLogMessage("%s", wxString::FromUTF8(m_lastFixtureDebugReport));
        }
    }

    if (m_captureCallback) {
      // Capture buffer and state copies before invoking the callback to avoid
      // lifetime issues once the next frame is rendered.
      auto callback = std::move(m_captureCallback);
      CommandBuffer bufferCopy = m_lastCapturedFrame;
      Viewer2DViewState stateCopy = GetViewState();

      callback(std::move(bufferCopy), stateCopy);
    }
  }

  glFlush();
  SwapBuffers();
}

wxBitmap Viewer2DPanel::ReadBackBitmap(int width, int height) const {
  if (width <= 0 || height <= 0)
    return wxBitmap();
  wxImage image(width, height, false);
  std::vector<unsigned char> pixels(
      static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE,
               pixels.data());
  unsigned char *data = image.GetData();
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      size_t srcIndex =
          (static_cast<size_t>(y) * static_cast<size_t>(width) +
           static_cast<size_t>(x)) *
          3;
      size_t dstIndex =
          (static_cast<size_t>(height - 1 - y) * static_cast<size_t>(width) +
           static_cast<size_t>(x)) *
          3;
      data[dstIndex] = pixels[srcIndex];
      data[dstIndex + 1] = pixels[srcIndex + 1];
      data[dstIndex + 2] = pixels[srcIndex + 2];
    }
  }
  return wxBitmap(image);
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
