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
// macOS uses the OpenGL framework headers; guard includes for cross-platform builds.
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "viewer2dpanel.h"
#include "configmanager.h"
#include "canvas2d.h"
#include "fixturetablepanel.h"
#include "fixturepatchdialog.h"
#include "logger.h"
#include "positionvalueupdate.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer3dpanel.h"
#include <wx/app.h>
#include <wx/utils.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <vector>

// Pixels per meter at default zoom level.
static constexpr float PIXELS_PER_METER = 25.0f;

namespace {
constexpr size_t kMaxCapturePixels = 8192u * 8192u;
constexpr size_t kMaxCaptureBytes = 64u * 1024u * 1024u;

bool TryAllocateCaptureBuffer(std::vector<unsigned char> &pixels, int width,
                              int height) {
  if (width <= 0 || height <= 0)
    return false;
  const size_t totalPixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t totalBytes = totalPixels * 4;
  if (totalPixels > kMaxCapturePixels || totalBytes > kMaxCaptureBytes) {
    Logger::Instance().Log(
        "Viewer2DPanel: capture buffer too large (" +
        std::to_string(width) + "x" + std::to_string(height) + ").");
    return false;
  }
  try {
    pixels.assign(totalBytes, 0);
  } catch (const std::bad_alloc &) {
    Logger::Instance().Log(
        "Viewer2DPanel: capture buffer allocation failed.");
    return false;
  }
  return true;
}

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

std::string FormatMeters(float millimeters) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3)
      << (static_cast<double>(millimeters) / 1000.0);
  return out.str();
}

std::vector<std::string> BuildFixtureSelectionByType(
    const MvrScene &scene, const std::string &typeName) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (typeName.empty() || fixture.typeName == typeName)
      uuids.push_back(uuid);
  }
  return uuids;
}

std::vector<std::string> BuildFixtureSelectionByPosition(
    const MvrScene &scene, const std::string &positionName,
    bool selectNoPosition) {
  std::vector<std::string> uuids;
  uuids.reserve(scene.fixtures.size());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const bool hasPosition = !fixture.positionName.empty();
    if (selectNoPosition) {
      if (!hasPosition)
        uuids.push_back(uuid);
      continue;
    }
    if (positionName.empty() || fixture.positionName == positionName)
      uuids.push_back(uuid);
  }
  return uuids;
}

void ApplyFixtureSelectionToUi(const std::vector<std::string> &selection,
                               Viewer3DController &controller) {
  ConfigManager &cfg = ConfigManager::Get();
  if (selection != cfg.GetSelectedFixtures()) {
    cfg.PushUndoState("fixture selection");
    cfg.SetSelectedFixtures(selection);
  }
  controller.SetSelectedUuids(selection);
  if (FixtureTablePanel::Instance()) {
    if (selection.empty())
      FixtureTablePanel::Instance()->ClearSelection();
    else
      FixtureTablePanel::Instance()->SelectByUuid(selection);
  }
}
} // namespace

namespace {
Viewer2DPanel *g_instance = nullptr;
}

wxBEGIN_EVENT_TABLE(Viewer2DPanel, wxGLCanvas) EVT_PAINT(Viewer2DPanel::OnPaint)
    EVT_LEFT_DOWN(Viewer2DPanel::OnMouseDown) EVT_LEFT_UP(
        Viewer2DPanel::OnMouseUp) EVT_MOTION(Viewer2DPanel::OnMouseMove)
        EVT_LEFT_DCLICK(Viewer2DPanel::OnMouseDClick)
        EVT_MOUSEWHEEL(Viewer2DPanel::OnMouseWheel)
            EVT_RIGHT_UP(Viewer2DPanel::OnRightUp)
            EVT_KEY_DOWN(Viewer2DPanel::OnKeyDown)
                EVT_ENTER_WINDOW(Viewer2DPanel::OnMouseEnter)
                    EVT_LEAVE_WINDOW(Viewer2DPanel::OnMouseLeave)
                        EVT_MOUSE_CAPTURE_LOST(Viewer2DPanel::OnCaptureLost)
                            EVT_TIMER(wxID_ANY, Viewer2DPanel::OnInteractionPauseTimer)
                            EVT_SIZE(Viewer2DPanel::OnResize) wxEND_EVENT_TABLE()

Viewer2DPanel::Viewer2DPanel(wxWindow *parent, bool allowOffscreenRender,
                             bool persistViewState, bool enableSelection)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE),
      m_allowOffscreenRender(allowOffscreenRender),
      m_interactionResumeTimer(this),
      m_persistViewState(persistViewState),
      m_enableSelection(enableSelection) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  m_controller.SetSelectionOutlineEnabled(m_enableSelection);
  m_glContext = new wxGLContext(this);
  if (m_enableSelection) {
    StartDragTableUpdateWorker();
  }
}

Viewer2DPanel::~Viewer2DPanel() {
  if (g_instance == this)
    g_instance = nullptr;
  m_interactionResumeTimer.Stop();
  StopDragTableUpdateWorker();
  delete m_glContext;
}

Viewer2DPanel *Viewer2DPanel::Instance() { return g_instance; }

void Viewer2DPanel::SetInstance(Viewer2DPanel *panel) { g_instance = panel; }

void Viewer2DPanel::UpdateScene(bool reload) {
  if (reload && m_enableSelection && ShouldPauseHeavyTasks())
    return;

  if (reload)
    m_controller.Update();
  if (m_enableSelection) {
    ConfigManager &cfg = ConfigManager::Get();
    std::vector<std::string> selection;
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage()) {
      selection = cfg.GetSelectedFixtures();
    } else if (TrussTablePanel::Instance() &&
               TrussTablePanel::Instance()->IsActivePage()) {
      selection = cfg.GetSelectedTrusses();
    } else if (SceneObjectTablePanel::Instance() &&
               SceneObjectTablePanel::Instance()->IsActivePage()) {
      selection = cfg.GetSelectedSceneObjects();
    }
    m_controller.SetSelectedUuids(selection);
  }
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

void Viewer2DPanel::SetSelectedUuids(
    const std::vector<std::string> &selection) {
  if (!m_enableSelection)
    return;
  m_controller.SetSelectedUuids(selection);
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
  if (!m_persistViewState)
    return;
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("view2d_offset_x", m_offsetX);
  cfg.SetFloat("view2d_offset_y", m_offsetY);
  cfg.SetFloat("view2d_zoom", m_zoom);
  cfg.SetFloat("view2d_render_mode", static_cast<float>(m_renderMode));
  cfg.SetFloat("view2d_view", static_cast<float>(m_view));
}

void Viewer2DPanel::ApplyViewState(float offsetX, float offsetY, float zoom,
                                   Viewer2DView view,
                                   Viewer2DRenderMode renderMode) {
  m_offsetX = offsetX;
  m_offsetY = offsetY;
  m_zoom = zoom;
  m_view = view;
  m_renderMode = renderMode;
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

void Viewer2DPanel::SetLayoutEditOverlay(std::optional<float> aspectRatio,
                                         std::optional<wxSize> viewportSize) {
  m_layoutEditAspect = aspectRatio;
  m_layoutEditViewportSize.reset();
  if (viewportSize && viewportSize->GetWidth() > 0 &&
      viewportSize->GetHeight() > 0) {
    m_layoutEditViewportSize = viewportSize;
    m_layoutEditBaseSize = viewportSize;
  } else {
    m_layoutEditBaseSize.reset();
  }
  m_layoutEditScale = 1.0f;
  Refresh();
}

void Viewer2DPanel::SetLayoutEditOverlayScale(float scale) {
  if (!m_layoutEditAspect)
    return;
  m_layoutEditScale = std::clamp(scale, 0.1f, 10.0f);
  Refresh();
}

std::optional<wxSize> Viewer2DPanel::GetLayoutEditOverlaySize() const {
  if (!m_layoutEditAspect)
    return std::nullopt;
  wxSize baseSize(0, 0);
  if (m_layoutEditBaseSize) {
    baseSize = *m_layoutEditBaseSize;
  } else {
    int w = 0;
    int h = 0;
    const_cast<Viewer2DPanel *>(this)->GetClientSize(&w, &h);
    if (w > 0 && h > 0) {
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
      baseSize = wxSize(static_cast<int>(std::lround(targetWidth)),
                        static_cast<int>(std::lround(targetHeight)));
    }
  }

  if (baseSize.GetWidth() <= 0 || baseSize.GetHeight() <= 0)
    return std::nullopt;

  int width =
      static_cast<int>(std::lround(baseSize.GetWidth() * m_layoutEditScale));
  int height =
      static_cast<int>(std::lround(baseSize.GetHeight() * m_layoutEditScale));
  return wxSize(width, height);
}

Viewer2DViewState Viewer2DPanel::GetViewState() const {
  int w = 0;
  int h = 0;
  const_cast<Viewer2DPanel *>(this)->GetClientSize(&w, &h);
  if (m_layoutEditViewportSize) {
    w = m_layoutEditViewportSize->GetWidth();
    h = m_layoutEditViewportSize->GetHeight();
  }

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
  if (!SetCurrent(*m_glContext)) {
    return;
  }
  if (!m_glInitialized) {
    glewExperimental = GL_TRUE;
    glewInit();
    m_controller.InitializeGL();
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    m_glInitialized = true;
  }
}

void Viewer2DPanel::Render() { RenderInternal(true); }

void Viewer2DPanel::RenderInternal(bool swapBuffers) {
  if (!m_glInitialized) {
    return;
  }
  const bool pauseHeavyTasks = m_enableSelection && ShouldPauseHeavyTasks();
  int w, h;
  GetClientSize(&w, &h);
  if (w <= 0 || h <= 0)
    return;

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
  // Keep controller resources/cache in sync in the 2D viewer as well.
  // The 3D panel triggers this from its own render loop, but the 2D panel
  // renders directly through the shared controller and otherwise misses
  // scene/layer visibility refreshes.
  if (!pauseHeavyTasks)
    m_controller.UpdateResourcesIfDirty();
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
                           gridR, gridG, gridB, drawAbove, true);

  // Draw labels for all fixtures after rendering the scene so they appear on
  // top of geometry. Scale the label size with the current zoom so they behave
  // like regular scene objects instead of remaining a constant screen size.
  if (!pauseHeavyTasks)
    m_controller.DrawAllFixtureLabels(w, h, m_view, m_zoom);

  if (m_layoutEditAspect && *m_layoutEditAspect > 0.0f) {
    if (!m_layoutEditBaseSize || m_layoutEditBaseSize->GetWidth() <= 0 ||
        m_layoutEditBaseSize->GetHeight() <= 0) {
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
      m_layoutEditBaseSize =
          wxSize(static_cast<int>(std::lround(targetWidth)),
                 static_cast<int>(std::lround(targetHeight)));
    }

    if (m_layoutEditBaseSize && m_layoutEditBaseSize->GetWidth() > 0 &&
        m_layoutEditBaseSize->GetHeight() > 0) {
      float targetWidth =
          static_cast<float>(m_layoutEditBaseSize->GetWidth()) *
          m_layoutEditScale;
      float targetHeight =
          static_cast<float>(m_layoutEditBaseSize->GetHeight()) *
          m_layoutEditScale;
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
  }

  if (swapBuffers && m_enableSelection && m_rectSelecting)
    DrawSelectionRectangle(w, h, darkMode);

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
  if (swapBuffers)
    SwapBuffers();
}

bool Viewer2DPanel::RenderToRGBA(std::vector<unsigned char> &pixels, int &width,
                                 int &height) {
  int w = 0;
  int h = 0;
  GetClientSize(&w, &h);
  if (w <= 0 || h <= 0)
    return false;

  if (!TryAllocateCaptureBuffer(pixels, w, h))
    return false;
  width = w;
  height = h;

  bool previousForce = m_forceOffscreenRender;
  m_forceOffscreenRender = true;
  InitGL();
  if (!m_glInitialized) {
    m_forceOffscreenRender = previousForce;
    return false;
  }
  RenderInternal(false);

  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  m_forceOffscreenRender = previousForce;
  return true;
}

void Viewer2DPanel::OnPaint(wxPaintEvent &WXUNUSED(event)) {
  wxPaintDC dc(this);
  InitGL();

  const bool wasInteracting = m_isInteracting;
  const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
  if (wasInteracting && !pauseHeavyTasks && m_dragMode == DragMode::None &&
      !m_rectSelecting) {
    m_controller.Update();
  }

  Render();

  if (!m_enableSelection || !IsShownOnScreen())
    return;

  // Ensure the OpenGL context is current before hit-testing selections.
  SetCurrent(*m_glContext);

  int w, h;
  GetClientSize(&w, &h);

  wxString newLabel;
  wxPoint newPos;
  std::string newUuid;
  bool found = false;
  const bool skipLabelWork =
      wasInteracting && (m_dragMode == DragMode::View || m_dragMode == DragMode::None);

  if (!skipLabelWork && FixtureTablePanel::Instance() &&
      FixtureTablePanel::Instance()->IsActivePage()) {
    found = m_controller.GetFixtureLabelAt(m_lastMousePos.x, m_lastMousePos.y,
                                           w, h, newLabel, newPos, &newUuid);
    if (found) {
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->HighlightTruss(std::string());
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
  } else if (!skipLabelWork && TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage()) {
    found = m_controller.GetTrussLabelAt(m_lastMousePos.x, m_lastMousePos.y, w,
                                         h, newLabel, newPos, &newUuid);
    if (found) {
      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->HighlightFixture(std::string());
      if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
  } else if (!skipLabelWork && SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage()) {
    found = m_controller.GetSceneObjectLabelAt(m_lastMousePos.x,
                                               m_lastMousePos.y, w, h, newLabel,
                                               newPos, &newUuid);
    if (found) {
      if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->HighlightFixture(std::string());
      if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->HighlightTruss(std::string());
    }
  }

  bool highlightChanged = false;
  if (found) {
    highlightChanged = (m_hoverUuid != newUuid);
    m_hasHover = true;
    m_hoverUuid = newUuid;
    m_controller.SetHighlightUuid(m_hoverUuid);
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage())
      FixtureTablePanel::Instance()->HighlightFixture(std::string(m_hoverUuid));
    else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage())
      TrussTablePanel::Instance()->HighlightTruss(std::string(m_hoverUuid));
    else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage())
      SceneObjectTablePanel::Instance()->HighlightObject(std::string(m_hoverUuid));
  } else if (!skipLabelWork && (!m_hasHover || m_mouseMoved)) {
    highlightChanged = m_hasHover;
    m_hasHover = false;
    m_hoverUuid.clear();
    m_controller.SetHighlightUuid("");
    if (FixtureTablePanel::Instance())
      FixtureTablePanel::Instance()->HighlightFixture(std::string());
    if (TrussTablePanel::Instance())
      TrussTablePanel::Instance()->HighlightTruss(std::string());
    if (SceneObjectTablePanel::Instance())
      SceneObjectTablePanel::Instance()->HighlightObject(std::string());
  } else if (skipLabelWork) {
    highlightChanged = m_hasHover;
    m_hasHover = false;
    m_hoverUuid.clear();
    m_controller.SetHighlightUuid("");
  }
  m_mouseMoved = false;

  if (highlightChanged)
    Refresh();
}

std::array<float, 3> Viewer2DPanel::MapDragDelta(float dxMeters,
                                                 float dyMeters) const {
  switch (m_view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    return {dxMeters, dyMeters, 0.0f};
  case Viewer2DView::Front:
    return {dxMeters, 0.0f, dyMeters};
  case Viewer2DView::Side:
    return {0.0f, dxMeters, dyMeters};
  }
  return {dxMeters, dyMeters, 0.0f};
}

void Viewer2DPanel::ApplySelectionDelta(
    const std::array<float, 3> &deltaMeters) {
  if (m_dragSelectionUuids.empty())
    return;

  float dxMm = deltaMeters[0] * 1000.0f;
  float dyMm = deltaMeters[1] * 1000.0f;
  float dzMm = deltaMeters[2] * 1000.0f;
  if (dxMm == 0.0f && dyMm == 0.0f && dzMm == 0.0f)
    return;

  ConfigManager &cfg = ConfigManager::Get();
  if (!m_dragSelectionPushedUndo) {
    cfg.PushUndoState("move selection");
    m_dragSelectionPushedUndo = true;
  }
  auto &scene = cfg.GetScene();
  std::lock_guard<std::mutex> sceneLock(m_dragTableUpdateSceneMutex);

  auto applyDelta = [&](auto &items) {
    for (const auto &uuid : m_dragSelectionUuids) {
      auto it = items.find(uuid);
      if (it == items.end())
        continue;
      it->second.transform.o[0] += dxMm;
      it->second.transform.o[1] += dyMm;
      it->second.transform.o[2] += dzMm;
    }
  };

  switch (m_dragTarget) {
  case DragTarget::Fixtures:
    applyDelta(scene.fixtures);
    ScheduleDragTableUpdate();
    break;
  case DragTarget::Trusses:
    applyDelta(scene.trusses);
    ScheduleDragTableUpdate();
    break;
  case DragTarget::SceneObjects:
    applyDelta(scene.sceneObjects);
    ScheduleDragTableUpdate();
    break;
  default:
    break;
  }
}

void Viewer2DPanel::FinalizeSelectionDrag() {
  StopDragTableUpdates();
  ConfigManager &cfg = ConfigManager::Get();
  switch (m_dragTarget) {
  case DragTarget::Fixtures:
    if (FixtureTablePanel::Instance()) {
      auto selection = cfg.GetSelectedFixtures();
      FixtureTablePanel::Instance()->ReloadData();
      FixtureTablePanel::Instance()->SelectByUuid(selection);
    }
    break;
  case DragTarget::Trusses:
    if (TrussTablePanel::Instance()) {
      auto selection = cfg.GetSelectedTrusses();
      TrussTablePanel::Instance()->ReloadData();
      TrussTablePanel::Instance()->SelectByUuid(selection);
    }
    break;
  case DragTarget::SceneObjects:
    if (SceneObjectTablePanel::Instance()) {
      auto selection = cfg.GetSelectedSceneObjects();
      SceneObjectTablePanel::Instance()->ReloadData();
      SceneObjectTablePanel::Instance()->SelectByUuid(selection);
    }
    break;
  default:
    break;
  }

  if (!ShouldPauseHeavyTasks())
    UpdateScene(true);

  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
}

void Viewer2DPanel::ApplyRectangleSelection(const wxPoint &start,
                                            const wxPoint &end) {
  if (!m_enableSelection)
    return;
  if (!IsShownOnScreen())
    return;

  int w = 0;
  int h = 0;
  GetClientSize(&w, &h);
  if (w <= 0 || h <= 0)
    return;

  SetCurrent(*m_glContext);

  ConfigManager &cfg = ConfigManager::Get();
  if (FixtureTablePanel::Instance() &&
      FixtureTablePanel::Instance()->IsActivePage()) {
    auto selection = m_controller.GetFixturesInScreenRect(
        start.x, start.y, end.x, end.y, w, h);
    if (selection != cfg.GetSelectedFixtures()) {
      cfg.PushUndoState("fixture selection");
      cfg.SetSelectedFixtures(selection);
    }
    m_controller.SetSelectedUuids(selection);
    if (selection.empty())
      FixtureTablePanel::Instance()->ClearSelection();
    else
      FixtureTablePanel::Instance()->SelectByUuid(selection);
  } else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage()) {
    auto selection =
        m_controller.GetTrussesInScreenRect(start.x, start.y, end.x, end.y, w,
                                            h);
    if (selection != cfg.GetSelectedTrusses()) {
      cfg.PushUndoState("truss selection");
      cfg.SetSelectedTrusses(selection);
    }
    m_controller.SetSelectedUuids(selection);
    if (selection.empty())
      TrussTablePanel::Instance()->ClearSelection();
    else
      TrussTablePanel::Instance()->SelectByUuid(selection);
  } else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage()) {
    auto selection = m_controller.GetSceneObjectsInScreenRect(
        start.x, start.y, end.x, end.y, w, h);
    if (selection != cfg.GetSelectedSceneObjects()) {
      cfg.PushUndoState("scene object selection");
      cfg.SetSelectedSceneObjects(selection);
    }
    m_controller.SetSelectedUuids(selection);
    if (selection.empty())
      SceneObjectTablePanel::Instance()->ClearSelection();
    else
      SceneObjectTablePanel::Instance()->SelectByUuid(selection);
  }
}

void Viewer2DPanel::DrawSelectionRectangle(int width, int height,
                                           bool darkMode) {
  if (!m_rectSelecting)
    return;

  int left = std::min(m_rectSelectStart.x, m_rectSelectEnd.x);
  int right = std::max(m_rectSelectStart.x, m_rectSelectEnd.x);
  int top = std::min(m_rectSelectStart.y, m_rectSelectEnd.y);
  int bottom = std::max(m_rectSelectStart.y, m_rectSelectEnd.y);

  float glLeft = static_cast<float>(left);
  float glRight = static_cast<float>(right);
  float glBottom = static_cast<float>(height - bottom);
  float glTop = static_cast<float>(height - top);

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled)
    glDisable(GL_DEPTH_TEST);

  GLboolean stippleEnabled = glIsEnabled(GL_LINE_STIPPLE);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, 0x00FF);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, static_cast<float>(width), 0.0f,
          static_cast<float>(height), -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  if (darkMode)
    glColor3f(1.0f, 1.0f, 1.0f);
  else
    glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.5f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(glLeft, glBottom);
  glVertex2f(glRight, glBottom);
  glVertex2f(glRight, glTop);
  glVertex2f(glLeft, glTop);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (!stippleEnabled)
    glDisable(GL_LINE_STIPPLE);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
}

void Viewer2DPanel::ScheduleDragTableUpdate() {
  if (m_dragSelectionUuids.empty())
    return;
  if (ShouldPauseHeavyTasks())
    return;
  QueueDragTableUpdate(m_dragTarget, m_dragSelectionUuids);
}

void Viewer2DPanel::StopDragTableUpdates() {
  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableUpdateQueued = false;
    m_dragTableUpdateWorkerTarget = DragTarget::None;
    m_dragTableUpdateUuids.clear();
  }
}

void Viewer2DPanel::StartDragTableUpdateWorker() {
  m_dragTableUpdateWorker = std::thread([this]() {
    auto lastUpdate = std::chrono::steady_clock::time_point::min();
    const auto interval =
        std::chrono::milliseconds(kDragTableUpdateIntervalMs);
    while (true) {
      DragTarget target = DragTarget::None;
      std::vector<std::string> uuids;
      {
        std::unique_lock<std::mutex> lock(m_dragTableUpdateMutex);
        m_dragTableUpdateCv.wait(lock, [this]() {
          return m_dragTableWorkerStop || m_dragTableUpdateQueued;
        });
        if (m_dragTableWorkerStop)
          break;

        auto now = std::chrono::steady_clock::now();
        if (lastUpdate != std::chrono::steady_clock::time_point::min()) {
          auto elapsed = now - lastUpdate;
          if (elapsed < interval) {
            m_dragTableUpdateCv.wait_for(lock, interval - elapsed, [this]() {
              return m_dragTableWorkerStop;
            });
            if (m_dragTableWorkerStop)
              break;
          }
        }
        if (m_dragTableWorkerStop)
          break;
        target = m_dragTableUpdateWorkerTarget;
        uuids = std::move(m_dragTableUpdateUuids);
        m_dragTableUpdateQueued = false;
      }
      lastUpdate = std::chrono::steady_clock::now();

      if (uuids.empty())
        continue;

      auto snapshots = BuildDragTablePositionSnapshots(target, uuids);
      if (snapshots.empty())
        continue;

      std::vector<PositionValueUpdate> updates;
      updates.reserve(snapshots.size());
      for (const auto &snapshot : snapshots) {
        updates.push_back(PositionValueUpdate{
            snapshot.uuid, FormatMeters(snapshot.xMm),
            FormatMeters(snapshot.yMm), FormatMeters(snapshot.zMm)});
      }

      if (!wxTheApp)
        continue;

      wxTheApp->CallAfter([target, updates = std::move(updates)]() mutable {
        switch (target) {
        case DragTarget::Fixtures:
          if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->ApplyPositionValueUpdates(updates);
          break;
        case DragTarget::Trusses:
          if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->ApplyPositionValueUpdates(updates);
          break;
        case DragTarget::SceneObjects:
          if (SceneObjectTablePanel::Instance())
            SceneObjectTablePanel::Instance()->ApplyPositionValueUpdates(
                updates);
          break;
        default:
          break;
        }
      });
    }
  });
}

void Viewer2DPanel::StopDragTableUpdateWorker() {
  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableWorkerStop = true;
    m_dragTableUpdateQueued = true;
  }
  m_dragTableUpdateCv.notify_one();
  if (m_dragTableUpdateWorker.joinable())
    m_dragTableUpdateWorker.join();
}

std::vector<Viewer2DPanel::DragTablePositionSnapshot>
Viewer2DPanel::BuildDragTablePositionSnapshots(
    DragTarget target, const std::vector<std::string> &uuids) {
  std::vector<DragTablePositionSnapshot> snapshots;
  snapshots.reserve(uuids.size());

  std::lock_guard<std::mutex> sceneLock(m_dragTableUpdateSceneMutex);
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  switch (target) {
  case DragTarget::Fixtures:
    for (const auto &uuid : uuids) {
      auto it = scene.fixtures.find(uuid);
      if (it == scene.fixtures.end())
        continue;
      auto posArr = it->second.GetPosition();
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  case DragTarget::Trusses:
    for (const auto &uuid : uuids) {
      auto it = scene.trusses.find(uuid);
      if (it == scene.trusses.end())
        continue;
      auto posArr = it->second.transform.o;
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  case DragTarget::SceneObjects:
    for (const auto &uuid : uuids) {
      auto it = scene.sceneObjects.find(uuid);
      if (it == scene.sceneObjects.end())
        continue;
      auto posArr = it->second.transform.o;
      snapshots.push_back({uuid, posArr[0], posArr[1], posArr[2]});
    }
    break;
  default:
    break;
  }

  return snapshots;
}

void Viewer2DPanel::QueueDragTableUpdate(DragTarget target,
                                         std::vector<std::string> uuids) {
  if (uuids.empty())
    return;

  {
    std::lock_guard<std::mutex> lock(m_dragTableUpdateMutex);
    m_dragTableUpdateWorkerTarget = target;
    m_dragTableUpdateUuids = std::move(uuids);
    m_dragTableUpdateQueued = true;
  }
  m_dragTableUpdateCv.notify_one();
}

bool Viewer2DPanel::ShouldPauseHeavyTasks() {
  if (!m_isInteracting)
    return false;

  const auto now = std::chrono::steady_clock::now();
  if ((now - m_lastInteractionTime) < kPauseDelay)
    return true;

  m_isInteracting = false;
  return false;
}

void Viewer2DPanel::MarkInteractionActivity() {
  m_isInteracting = true;
  m_lastInteractionTime = std::chrono::steady_clock::now();
  m_interactionResumeTimer.StartOnce(
      static_cast<int>(kPauseDelay.count()) + 10);
}

void Viewer2DPanel::OnInteractionPauseTimer(wxTimerEvent &WXUNUSED(event)) {
  if (!ShouldPauseHeavyTasks())
    Refresh(false);
}

void Viewer2DPanel::OnMouseDown(wxMouseEvent &event) {
  if (event.LeftDown()) {
    CaptureMouse();
    m_draggedSincePress = false;
    m_dragPressTime = wxGetLocalTimeMillis();
    m_dragMode = DragMode::View;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragSelectionMoved = false;
    m_dragSelectionPushedUndo = false;
    m_lastMousePos = event.GetPosition();
    MarkInteractionActivity();

    if (!m_enableSelection || !IsShownOnScreen())
      return;

    if (event.ControlDown()) {
      m_dragMode = DragMode::RectSelection;
      m_rectSelecting = true;
      m_rectSelectStart = m_lastMousePos;
      m_rectSelectEnd = m_lastMousePos;
      return;
    }

    int w, h;
    GetClientSize(&w, &h);
    if (w <= 0 || h <= 0)
      return;

    SetCurrent(*m_glContext);
    wxString label;
    wxPoint pos;
    std::string uuid;
    bool found = false;
    DragTarget target = DragTarget::None;
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage()) {
      found = m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h,
                                             label, pos, &uuid);
      target = DragTarget::Fixtures;
    } else if (TrussTablePanel::Instance() &&
               TrussTablePanel::Instance()->IsActivePage()) {
      found = m_controller.GetTrussLabelAt(event.GetX(), event.GetY(), w, h,
                                           label, pos, &uuid);
      target = DragTarget::Trusses;
    } else if (SceneObjectTablePanel::Instance() &&
               SceneObjectTablePanel::Instance()->IsActivePage()) {
      found = m_controller.GetSceneObjectLabelAt(event.GetX(), event.GetY(), w,
                                                 h, label, pos, &uuid);
      target = DragTarget::SceneObjects;
    }

    if (found && target != DragTarget::None) {
      ConfigManager &cfg = ConfigManager::Get();
      std::vector<std::string> selection;
      switch (target) {
      case DragTarget::Fixtures:
        selection = cfg.GetSelectedFixtures();
        break;
      case DragTarget::Trusses:
        selection = cfg.GetSelectedTrusses();
        break;
      case DragTarget::SceneObjects:
        selection = cfg.GetSelectedSceneObjects();
        break;
      default:
        break;
      }

      auto it = std::find(selection.begin(), selection.end(), uuid);
      if (selection.size() > 1 || it != selection.end())
        m_dragSelectionUuids = selection;
      else
        m_dragSelectionUuids = {uuid};

      m_dragMode = DragMode::Selection;
      m_dragTarget = target;
    }
  }
}

void Viewer2DPanel::OnMouseDClick(wxMouseEvent &event) {
  int w, h;
  GetClientSize(&w, &h);
  if (w <= 0 || h <= 0 || !IsShownOnScreen())
    return;

  SetCurrent(*m_glContext);
  wxString label;
  wxPoint pos;
  std::string uuid;
  if (!m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label,
                                      pos, &uuid))
    return;

  auto &scene = ConfigManager::Get().GetScene();
  auto it = scene.fixtures.find(uuid);
  if (it == scene.fixtures.end())
    return;

  FixturePatchDialog dlg(this, it->second);
  if (dlg.ShowModal() != wxID_OK)
    return;

  it->second.fixtureId = dlg.GetFixtureId();
  int uni = dlg.GetUniverse();
  int ch = dlg.GetChannel();
  if (uni > 0 && ch > 0)
    it->second.address = wxString::Format("%d.%d", uni, ch).ToStdString();
  else
    it->second.address.clear();

  if (FixtureTablePanel::Instance())
    FixtureTablePanel::Instance()->ReloadData();

  UpdateScene(false);
  Refresh();
}

void Viewer2DPanel::OnMouseUp(wxMouseEvent &event) {
  if (event.LeftUp() && m_dragMode == DragMode::RectSelection) {
    if (HasCapture())
      ReleaseMouse();
    if (m_rectSelecting)
      ApplyRectangleSelection(m_rectSelectStart, m_rectSelectEnd);
    m_rectSelecting = false;
    m_dragMode = DragMode::None;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragSelectionMoved = false;
    m_draggedSincePress = false;
    Refresh();
    return;
  }

  if (event.LeftUp() && m_dragMode != DragMode::None) {
    if (HasCapture())
      ReleaseMouse();
    if (m_dragMode == DragMode::Selection && m_dragSelectionMoved)
      FinalizeSelectionDrag();
    m_dragMode = DragMode::None;
    m_dragAxis = DragAxis::None;
    m_dragTarget = DragTarget::None;
    m_dragSelectionUuids.clear();
    m_dragSelectionMoved = false;
  }

  if (!m_enableSelection) {
    m_draggedSincePress = false;
    return;
  }

  if (event.LeftUp() && !m_draggedSincePress) {
    int w, h;
    GetClientSize(&w, &h);
    if (!IsShownOnScreen()) {
      return;
    }
    SetCurrent(*m_glContext);
    wxString label;
    wxPoint pos;
    std::string uuid;
    bool found = false;
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage())
      found = m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h,
                                             label, pos, &uuid);
    else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage())
      found = m_controller.GetTrussLabelAt(event.GetX(), event.GetY(), w, h,
                                           label, pos, &uuid);
    else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage())
      found = m_controller.GetSceneObjectLabelAt(event.GetX(), event.GetY(), w,
                                                 h, label, pos, &uuid);

    ConfigManager &cfg = ConfigManager::Get();
    if (found) {
      bool additive = event.ShiftDown() || event.ControlDown();
      std::vector<std::string> selection;
      if (FixtureTablePanel::Instance() &&
          FixtureTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = FixtureTablePanel::Instance()->GetSelectedUuids();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end())
            selection.erase(it);
          else
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedFixtures()) {
          cfg.PushUndoState("fixture selection");
          cfg.SetSelectedFixtures(selection);
        }
        m_controller.SetSelectedUuids(selection);
        FixtureTablePanel::Instance()->SelectByUuid(selection);
      } else if (TrussTablePanel::Instance() &&
                 TrussTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = TrussTablePanel::Instance()->GetSelectedUuids();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end())
            selection.erase(it);
          else
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedTrusses()) {
          cfg.PushUndoState("truss selection");
          cfg.SetSelectedTrusses(selection);
        }
        m_controller.SetSelectedUuids(selection);
        TrussTablePanel::Instance()->SelectByUuid(selection);
      } else if (SceneObjectTablePanel::Instance() &&
                 SceneObjectTablePanel::Instance()->IsActivePage()) {
        if (additive)
          selection = SceneObjectTablePanel::Instance()->GetSelectedUuids();
        if (additive) {
          auto it = std::find(selection.begin(), selection.end(), uuid);
          if (it != selection.end())
            selection.erase(it);
          else
            selection.push_back(uuid);
        } else {
          selection = {uuid};
        }
        if (selection != cfg.GetSelectedSceneObjects()) {
          cfg.PushUndoState("scene object selection");
          cfg.SetSelectedSceneObjects(selection);
        }
        m_controller.SetSelectedUuids(selection);
        SceneObjectTablePanel::Instance()->SelectByUuid(selection);
      }
    } else {
      if (FixtureTablePanel::Instance() &&
          FixtureTablePanel::Instance()->IsActivePage()) {
        if (!cfg.GetSelectedFixtures().empty()) {
          cfg.PushUndoState("fixture selection");
          cfg.SetSelectedFixtures({});
        }
        m_controller.SetSelectedUuids({});
        FixtureTablePanel::Instance()->ClearSelection();
      } else if (TrussTablePanel::Instance() &&
                 TrussTablePanel::Instance()->IsActivePage()) {
        if (!cfg.GetSelectedTrusses().empty()) {
          cfg.PushUndoState("truss selection");
          cfg.SetSelectedTrusses({});
        }
        m_controller.SetSelectedUuids({});
        TrussTablePanel::Instance()->ClearSelection();
      } else if (SceneObjectTablePanel::Instance() &&
                 SceneObjectTablePanel::Instance()->IsActivePage()) {
        if (!cfg.GetSelectedSceneObjects().empty()) {
          cfg.PushUndoState("scene object selection");
          cfg.SetSelectedSceneObjects({});
        }
        m_controller.SetSelectedUuids({});
        SceneObjectTablePanel::Instance()->ClearSelection();
      } else {
        m_controller.SetSelectedUuids({});
      }
    }
    Refresh();
  }
  m_draggedSincePress = false;
}


void Viewer2DPanel::OnRightUp(wxMouseEvent &event) {
  if (!m_enableSelection || !event.RightUp()) {
    event.Skip();
    return;
  }

  if (!(FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())) {
    event.Skip();
    return;
  }

  int w, h;
  GetClientSize(&w, &h);
  if (w <= 0 || h <= 0 || !IsShownOnScreen()) {
    event.Skip();
    return;
  }

  SetCurrent(*m_glContext);
  wxString label;
  wxPoint pos;
  std::string hitUuid;
  if (m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label,
                                     pos, &hitUuid)) {
    event.Skip();
    return;
  }

  const auto &scene = ConfigManager::Get().GetScene();
  if (scene.fixtures.empty())
    return;

  std::set<std::string> typeNames;
  std::set<std::string> positionNames;
  bool hasNoPosition = false;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (!fixture.typeName.empty())
      typeNames.insert(fixture.typeName);
    if (fixture.positionName.empty())
      hasNoPosition = true;
    else
      positionNames.insert(fixture.positionName);
  }

  wxMenu filterMenu;
  auto typeSubmenu = std::make_unique<wxMenu>();
  auto positionSubmenu = std::make_unique<wxMenu>();

  constexpr int kSelectTypeAllId = wxID_HIGHEST + 600;
  constexpr int kSelectTypeBaseId = wxID_HIGHEST + 601;
  constexpr int kSelectPositionNoneId = wxID_HIGHEST + 800;
  constexpr int kSelectPositionBaseId = wxID_HIGHEST + 801;

  typeSubmenu->Append(kSelectTypeAllId, "All fixtures");

  std::vector<std::string> orderedTypes;
  orderedTypes.reserve(typeNames.size());
  int nextTypeId = kSelectTypeBaseId;
  for (const auto &name : typeNames) {
    orderedTypes.push_back(name);
    typeSubmenu->Append(nextTypeId++, wxString::FromUTF8(name));
  }

  positionSubmenu->Append(kSelectPositionNoneId, "No position");

  std::vector<std::string> orderedPositions;
  orderedPositions.reserve(positionNames.size());
  int nextPositionId = kSelectPositionBaseId;
  for (const auto &name : positionNames) {
    orderedPositions.push_back(name);
    positionSubmenu->Append(nextPositionId++, wxString::FromUTF8(name));
  }

  filterMenu.AppendSubMenu(typeSubmenu.release(), "Select by fixture type");
  filterMenu.AppendSubMenu(positionSubmenu.release(), "Select by position");

  const int selectedId =
      GetPopupMenuSelectionFromUser(filterMenu, event.GetPosition());

  if (selectedId == wxID_NONE)
    return;

  if (selectedId == kSelectTypeAllId) {
    ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, ""),
                              m_controller);
    Refresh();
    return;
  }

  if (selectedId >= kSelectTypeBaseId &&
      selectedId < kSelectTypeBaseId + static_cast<int>(orderedTypes.size())) {
    const size_t idx = static_cast<size_t>(selectedId - kSelectTypeBaseId);
    ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, orderedTypes[idx]),
                              m_controller);
    Refresh();
    return;
  }

  if (selectedId == kSelectPositionNoneId) {
    if (!hasNoPosition) {
      ApplyFixtureSelectionToUi({}, m_controller);
    } else {
      ApplyFixtureSelectionToUi(
          BuildFixtureSelectionByPosition(scene, "", true), m_controller);
    }
    Refresh();
    return;
  }

  if (selectedId >= kSelectPositionBaseId &&
      selectedId <
          kSelectPositionBaseId + static_cast<int>(orderedPositions.size())) {
    const size_t idx = static_cast<size_t>(selectedId - kSelectPositionBaseId);
    ApplyFixtureSelectionToUi(
        BuildFixtureSelectionByPosition(scene, orderedPositions[idx], false),
        m_controller);
    Refresh();
    return;
  }
}

void Viewer2DPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(event)) {
  m_dragMode = DragMode::None;
  m_dragAxis = DragAxis::None;
  m_dragTarget = DragTarget::None;
  m_dragSelectionUuids.clear();
  m_dragSelectionMoved = false;
  m_rectSelecting = false;
}

void Viewer2DPanel::OnMouseMove(wxMouseEvent &event) {
  wxPoint pos = event.GetPosition();

  if (m_dragMode == DragMode::RectSelection && event.Dragging()) {
    m_rectSelectEnd = pos;
    m_draggedSincePress = true;
    MarkInteractionActivity();
    Refresh();
    return;
  }

  if (m_dragMode == DragMode::Selection && event.Dragging()) {
    if ((wxGetLocalTimeMillis() - m_dragPressTime).ToLong() <
        kSelectionDragDelayMs) {
      m_lastMousePos = pos;
      return;
    }

    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;

    if (dx != 0 || dy != 0) {
      if (m_dragAxis == DragAxis::None) {
        int absDx = std::abs(dx);
        int absDy = std::abs(dy);
        if (absDx >= 3 || absDy >= 3) {
          m_dragAxis =
              absDx >= absDy ? DragAxis::Horizontal : DragAxis::Vertical;
        }
      }

      if (m_dragAxis == DragAxis::Horizontal)
        dy = 0;
      else if (m_dragAxis == DragAxis::Vertical)
        dx = 0;

      if (dx != 0 || dy != 0) {
        float ppm = PIXELS_PER_METER * m_zoom;
        if (ppm > 0.0f) {
          float dxMeters = static_cast<float>(dx) / ppm;
          float dyMeters = static_cast<float>(-dy) / ppm;
          ApplySelectionDelta(MapDragDelta(dxMeters, dyMeters));
          m_draggedSincePress = true;
          MarkInteractionActivity();
          m_dragSelectionMoved = true;
          Refresh();
        }
      }
    }

    m_lastMousePos = pos;
    return;
  }

  if (m_dragMode == DragMode::View && event.Dragging()) {
    int dx = pos.x - m_lastMousePos.x;
    int dy = pos.y - m_lastMousePos.y;
    m_offsetX += dx / m_zoom;
    m_offsetY += dy / m_zoom;
    m_lastMousePos = pos;
    m_draggedSincePress = true;
    MarkInteractionActivity();
    if (m_persistViewState)
      SaveViewToConfig();
    Refresh();
    return;
  }

  if (m_enableSelection) {
    m_mouseMoved = true;
    m_lastMousePos = pos;
    Refresh();
  }
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent &event) {
  MarkInteractionActivity();

  int rotation = event.GetWheelRotation();
  int deltaWheel = event.GetWheelDelta();
  float steps = 0.0f;
  if (deltaWheel != 0)
    steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
  float factor = std::pow(1.1f, steps);
  m_zoom *= factor;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  if (m_persistViewState)
    SaveViewToConfig();
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
  if (m_persistViewState)
    SaveViewToConfig();
  Refresh();
}

void Viewer2DPanel::OnMouseEnter(wxMouseEvent &event) {
  m_mouseInside = true;
  SetFocus();
  event.Skip();
}

void Viewer2DPanel::OnMouseLeave(wxMouseEvent &event) {
  m_mouseInside = false;
  if (m_enableSelection) {
    m_hasHover = false;
    m_hoverUuid.clear();
    m_controller.SetHighlightUuid("");
    if (FixtureTablePanel::Instance())
      FixtureTablePanel::Instance()->HighlightFixture(std::string());
    if (TrussTablePanel::Instance())
      TrussTablePanel::Instance()->HighlightTruss(std::string());
    if (SceneObjectTablePanel::Instance())
      SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    Refresh();
  }
  event.Skip();
}

void Viewer2DPanel::OnResize(wxSizeEvent &event) {
  if (m_layoutEditAspect && !m_layoutEditViewportSize) {
    m_layoutEditBaseSize.reset();
  }
  Refresh();
  event.Skip();
}
