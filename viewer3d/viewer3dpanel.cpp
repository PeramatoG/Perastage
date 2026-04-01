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
 * File: viewer3dpanel.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of the 3D viewer panel using OpenGL.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GL/glew.h>
// macOS ships OpenGL headers in the framework; include them conditionally.
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "viewer3dpanel.h"
#include "mainwindow.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "fixturepatchdialog.h"
#include "viewer2dpanel.h"
#include "viewer3dviewfit.h"
#include "viewer3d_render_style.h"
#include <wx/dcclient.h>
#include <wx/event.h>
#include <wx/log.h>
#include <chrono>
#include <array>
#include <algorithm>
#include <memory>
#include <cmath>
#include <set>

wxDEFINE_EVENT(wxEVT_VIEWER_REFRESH, wxThreadEvent);
wxBEGIN_EVENT_TABLE(Viewer3DPanel, wxGLCanvas)
EVT_PAINT(Viewer3DPanel::OnPaint)
EVT_SIZE(Viewer3DPanel::OnResize)
EVT_LEFT_DOWN(Viewer3DPanel::OnMouseDown)
EVT_LEFT_UP(Viewer3DPanel::OnMouseUp)
EVT_MOTION(Viewer3DPanel::OnMouseMove)
EVT_LEFT_DCLICK(Viewer3DPanel::OnMouseDClick)
EVT_MOUSEWHEEL(Viewer3DPanel::OnMouseWheel)
EVT_RIGHT_UP(Viewer3DPanel::OnRightUp)
EVT_KEY_DOWN(Viewer3DPanel::OnKeyDown)
EVT_ENTER_WINDOW(Viewer3DPanel::OnMouseEnter)
EVT_LEAVE_WINDOW(Viewer3DPanel::OnMouseLeave)
EVT_MOUSE_CAPTURE_LOST(Viewer3DPanel::OnCaptureLost)
EVT_THREAD(wxEVT_VIEWER_REFRESH, Viewer3DPanel::OnThreadRefresh)
wxEND_EVENT_TABLE()


namespace {
constexpr auto kPauseDelay = std::chrono::milliseconds(200);

bool IsFastInteractionModeEnabled()
{
    return ConfigManager::Get().GetFloat("viewer3d_fast_interaction_mode") >= 0.5f;
}

Viewer3DRenderStyle ResolveRenderStyleFromPreferences() {
    return ResolveViewer3DRenderStyle(ConfigManager::Get());
}

bool IsWireframeRenderStyle(Viewer3DRenderStyle style) {
    return style == Viewer3DRenderStyle::Wireframe;
}

Viewer2DRenderMode ToSceneRenderMode(Viewer3DRenderStyle style) {
    switch (style) {
        case Viewer3DRenderStyle::Wireframe:
            return Viewer2DRenderMode::Wireframe;
        case Viewer3DRenderStyle::ByDeviceType:
            return Viewer2DRenderMode::ByFixtureType;
        case Viewer3DRenderStyle::ByLayer:
            return Viewer2DRenderMode::ByLayer;
        case Viewer3DRenderStyle::ByUniverse:
            return Viewer2DRenderMode::ByUniverse;
        case Viewer3DRenderStyle::WhiteModel:
        case Viewer3DRenderStyle::Textured:
        case Viewer3DRenderStyle::Standard:
        default:
            return Viewer2DRenderMode::White;
    }
}

void ApplyViewer3DClearColorForStyle(Viewer3DRenderStyle style) {
    if (style == Viewer3DRenderStyle::Wireframe ||
        IsWhiteModelRenderStyle(style)) {
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
        return;
    }
    if (style == Viewer3DRenderStyle::Textured) {
        glClearColor(0.05f, 0.04f, 0.08f, 1.0f);
        return;
    }
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
}

float ComputeGridPlaneHorizonNdcY() {
    GLdouble model[16] = {0.0};
    GLdouble projection[16] = {0.0};
    GLint viewport[4] = {0, 0, 0, 0};
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (viewport[3] <= 0)
        return -0.05f;

    constexpr int kSamples = 48;
    constexpr double kProbeRadius = 2000.0;
    bool found = false;
    double topMostY = -1e9;
    for (int i = 0; i < kSamples; ++i) {
        const double angle = (static_cast<double>(i) / static_cast<double>(kSamples)) *
                             2.0 * 3.14159265358979323846;
        const double x = std::cos(angle) * kProbeRadius;
        const double y = std::sin(angle) * kProbeRadius;
        GLdouble sx = 0.0, sy = 0.0, sz = 0.0;
        if (gluProject(x, y, 0.0, model, projection, viewport, &sx, &sy, &sz) == GL_TRUE &&
            sz >= 0.0 && sz <= 1.0) {
            topMostY = std::max(topMostY, sy);
            found = true;
        }
    }

    if (!found)
        return -0.05f;

    const double ndcY = (topMostY / static_cast<double>(viewport[3])) * 2.0 - 1.0;
    return static_cast<float>(std::clamp(ndcY, -0.85, 0.85));
}

void DrawTexturedStyleBackgroundGradient(float horizonNdcY) {
    const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
    const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean texture2DWasEnabled = glIsEnabled(GL_TEXTURE_2D);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const std::array<std::pair<float, std::array<float, 3>>, 6> bands = {{
        {1.0f, {0.02f, 0.05f, 0.18f}},
        {0.55f, {0.10f, 0.20f, 0.40f}},
        {horizonNdcY + 0.16f, {0.42f, 0.28f, 0.48f}},
        {horizonNdcY, {0.96f, 0.56f, 0.26f}},
        {horizonNdcY - 0.28f, {0.62f, 0.28f, 0.19f}},
        {-1.0f, {0.18f, 0.09f, 0.11f}},
    }};
    for (size_t i = 0; i + 1 < bands.size(); ++i) {
        glBegin(GL_QUADS);
        glColor3f(bands[i].second[0], bands[i].second[1], bands[i].second[2]);
        glVertex2f(-1.0f, bands[i].first);
        glVertex2f(1.0f, bands[i].first);
        glColor3f(bands[i + 1].second[0], bands[i + 1].second[1], bands[i + 1].second[2]);
        glVertex2f(1.0f, bands[i + 1].first);
        glVertex2f(-1.0f, bands[i + 1].first);
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if (lightingWasEnabled)
        glEnable(GL_LIGHTING);
    if (depthTestWasEnabled)
        glEnable(GL_DEPTH_TEST);
    if (cullFaceWasEnabled)
        glEnable(GL_CULL_FACE);
    if (texture2DWasEnabled)
        glEnable(GL_TEXTURE_2D);
}

void DrawTexturedGroundPlaneBackdrop() {
    const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
    const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean texture2DWasEnabled = glIsEnabled(GL_TEXTURE_2D);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);

    constexpr int kSegments = 64;
    constexpr float kRadius = 2500.0f;
    constexpr float kPi = 3.14159265358979323846f;

    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.22f, 0.16f, 0.14f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(kSegments)) *
                            2.0f * kPi;
        const float x = std::cos(angle) * kRadius;
        const float y = std::sin(angle) * kRadius;
        glColor3f(0.54f, 0.33f, 0.24f);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();

    if (lightingWasEnabled)
        glEnable(GL_LIGHTING);
    if (depthTestWasEnabled)
        glEnable(GL_DEPTH_TEST);
    if (cullFaceWasEnabled)
        glEnable(GL_CULL_FACE);
    if (texture2DWasEnabled)
        glEnable(GL_TEXTURE_2D);
}

std::vector<std::string> BuildFixtureSelectionByType(
    const MvrScene& scene, const std::string& typeName)
{
    std::vector<std::string> uuids;
    uuids.reserve(scene.fixtures.size());
    for (const auto& [uuid, fixture] : scene.fixtures) {
        if (typeName.empty() || fixture.typeName == typeName)
            uuids.push_back(uuid);
    }
    return uuids;
}

std::vector<std::string> BuildFixtureSelectionByPosition(
    const MvrScene& scene, const std::string& positionName, bool selectNoPosition)
{
    std::vector<std::string> uuids;
    uuids.reserve(scene.fixtures.size());
    for (const auto& [uuid, fixture] : scene.fixtures) {
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

void ApplyFixtureSelectionToUi(const std::vector<std::string>& selection,
                               Viewer3DPanel* panel,
                               Viewer3DController& controller)
{
    ConfigManager& cfg = ConfigManager::Get();
    if (selection != cfg.GetSelectedFixtures()) {
        cfg.PushUndoState("fixture selection");
        cfg.SetSelectedFixtures(selection);
    }
    controller.SetSelectedUuids(selection);
    if (panel)
        panel->SetSelectedFixtures(selection);
    if (FixtureTablePanel::Instance()) {
        if (selection.empty())
            FixtureTablePanel::Instance()->ClearSelection();
        else
            FixtureTablePanel::Instance()->SelectByUuid(selection);
    }
}

struct GlCanvasSelection {
    const int* attribs = nullptr;
};

int GetRequestedViewerAASamples()
{
    const int quality = std::clamp(static_cast<int>(std::lround(ConfigManager::Get().GetFloat("viewer3d_aa_quality"))), 0, 2);
    switch (quality) {
    case 2:
        return 4;
    case 1:
        return 2;
    default:
        return 0;
    }
}

GlCanvasSelection SelectGlCanvasAttributes()
{
    static const int attrsNoMsaa[] = {
        WX_GL_RGBA,
        WX_GL_DOUBLEBUFFER,
        WX_GL_DEPTH_SIZE, 24,
        0
    };
    static const int attrs2x[] = {
        WX_GL_RGBA,
        WX_GL_DOUBLEBUFFER,
        WX_GL_DEPTH_SIZE, 24,
        WX_GL_SAMPLE_BUFFERS, 1,
        WX_GL_SAMPLES, 2,
        0
    };
    static const int attrs4x[] = {
        WX_GL_RGBA,
        WX_GL_DOUBLEBUFFER,
        WX_GL_DEPTH_SIZE, 24,
        WX_GL_SAMPLE_BUFFERS, 1,
        WX_GL_SAMPLES, 4,
        0
    };

    const int requested = GetRequestedViewerAASamples();
    if (requested >= 4 && wxGLCanvas::IsDisplaySupported(attrs4x))
        return {attrs4x};
    if (requested >= 2 && wxGLCanvas::IsDisplaySupported(attrs2x))
        return {attrs2x};
    if (requested >= 4 && wxGLCanvas::IsDisplaySupported(attrs2x))
        return {attrs2x};
    return {attrsNoMsaa};
}
}

Viewer3DPanel::Viewer3DPanel(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, SelectGlCanvasAttributes().attribs,
                 wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
    m_glContext(new wxGLContext(this))
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    m_threadRunning = true;
    m_refreshThread = std::thread(&Viewer3DPanel::RefreshLoop, this);
}

Viewer3DPanel::~Viewer3DPanel()
{
    if (HasCapture())
        ReleaseMouse();
    StopRefreshThread();
    delete m_glContext;
    SetInstance(nullptr);
}

void Viewer3DPanel::StopRefreshThread()
{
    m_threadRunning = false;
    if (m_refreshThread.joinable())
        m_refreshThread.join();
}

// Initializes OpenGL basic settings
void Viewer3DPanel::InitGL()
{
    if (!IsShownOnScreen()) {
        return;
    }
    SetCurrent(*m_glContext);
    if (!m_glInitialized) {
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            wxLogError("GLEW initialization failed: %s",
                       reinterpret_cast<const char*>(glewGetErrorString(err)));
        }

        GLint sampleBuffers = 0;
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
        m_hasSampleBuffers = sampleBuffers > 0;

        m_controller.InitializeGL();
        m_glInitialized = true;
    }

    glEnable(GL_DEPTH_TEST);
    if (m_hasSampleBuffers)
        glEnable(GL_MULTISAMPLE);
    else
        glDisable(GL_MULTISAMPLE);
    ApplyViewer3DClearColorForStyle(ResolveRenderStyleFromPreferences());
}

// Paint event handler
void Viewer3DPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    if (!IsShownOnScreen()) {
        return;
    }
    InitGL();

    const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
    m_controller.ResetDebugPerFrameCounters();
    m_controller.UpdateFrameStateLightweight();
    if (m_sceneSyncPending) {
        m_controller.UpdateResourcesIfDirty();
        m_sceneSyncPending = false;
    } else if (!pauseHeavyTasks && !m_cameraMoving) {
        m_controller.UpdateResourcesIfDirty();
    }

    const int updateResourcesCallsPerFrame =
        m_controller.GetDebugUpdateResourcesCallsPerFrame();
    if (updateResourcesCallsPerFrame > 1) {
        wxLogTrace("viewer3d_perf",
                   "UpdateResourcesIfDirty called %d times in a frame",
                   updateResourcesCallsPerFrame);
    }

    static auto s_lastCameraUpdate = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - s_lastCameraUpdate).count();
    s_lastCameraUpdate = now;
    m_camera.Update(dt);

    Render();

    // Ensure the OpenGL context is current before drawing overlays
    SetCurrent(*m_glContext);

    int w, h;
    GetClientSize(&w, &h);

    wxString newLabel;
    wxPoint newPos;
    std::string newUuid;
    bool found = false;

    const bool skipLabelsWhenMoving =
        ConfigManager::Get().GetFloat("viewer3d_skip_labels_when_moving") >= 0.5f;
    const bool skipLabelWork = m_cameraMoving &&
        (IsFastInteractionModeEnabled() || skipLabelsWhenMoving);

    if (!skipLabelWork && FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage()) {
        found = m_controller.GetFixtureLabelAt(m_lastMousePos.x, m_lastMousePos.y,
            w, h, newLabel, newPos, &newUuid);
        if (found) {
            if (TrussTablePanel::Instance())
                TrussTablePanel::Instance()->HighlightTruss(std::string());
            if (SceneObjectTablePanel::Instance())
                SceneObjectTablePanel::Instance()->HighlightObject(std::string());
        }
    }
    else if (!skipLabelWork && TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage()) {
        found = m_controller.GetTrussLabelAt(m_lastMousePos.x, m_lastMousePos.y,
            w, h, newLabel, newPos, &newUuid);
        if (found) {
            if (FixtureTablePanel::Instance())
                FixtureTablePanel::Instance()->HighlightFixture(std::string());
            if (SceneObjectTablePanel::Instance())
                SceneObjectTablePanel::Instance()->HighlightObject(std::string());
        }
    }
    else if (!skipLabelWork && SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage()) {
        found = m_controller.GetSceneObjectLabelAt(m_lastMousePos.x, m_lastMousePos.y,
            w, h, newLabel, newPos, &newUuid);
        if (found) {
            if (FixtureTablePanel::Instance())
                FixtureTablePanel::Instance()->HighlightFixture(std::string());
            if (TrussTablePanel::Instance())
                TrussTablePanel::Instance()->HighlightTruss(std::string());
        }
    }

    if (found) {
        m_hasHover = true;
        m_hoverText = newLabel;
        m_hoverPos = newPos;
        m_hoverUuid = newUuid;
        m_controller.SetHighlightUuid(m_hoverUuid);
        if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
            FixtureTablePanel::Instance()->HighlightFixture(std::string(m_hoverUuid));
        else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
            TrussTablePanel::Instance()->HighlightTruss(std::string(m_hoverUuid));
        else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
            SceneObjectTablePanel::Instance()->HighlightObject(std::string(m_hoverUuid));
    }
    else if (!skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
        m_controller.SetHighlightUuid("");
        if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->HighlightFixture(std::string());
        if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->HighlightTruss(std::string());
        if (SceneObjectTablePanel::Instance())
            SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    } else if (skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
        m_controller.SetHighlightUuid("");
    }
    m_mouseMoved = false;

    // Draw labels before swapping buffers to avoid losing them.
    if (!pauseHeavyTasks && !skipLabelWork) {
        if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
            m_controller.DrawFixtureLabels(w, h);
        else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
            m_controller.DrawTrussLabels(w, h);
        else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
            m_controller.DrawSceneObjectLabels(w, h);
    }

    if (m_rectSelecting)
        DrawSelectionRectangle(w, h);

    SwapBuffers(); // Swap after drawing labels to ensure they are visible
}

// Resize event handler
void Viewer3DPanel::OnResize(wxSizeEvent& event)
{
    Refresh();
}

// Renders the full 3D scene
void Viewer3DPanel::Render()
{
    if (!IsShownOnScreen()) {
        return;
    }
    SetCurrent(*m_glContext);

    int width, height;
    GetClientSize(&width, &height);

    const Viewer3DRenderStyle renderStyle = ResolveRenderStyleFromPreferences();
    ApplyViewer3DClearColorForStyle(renderStyle);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ApplyCameraMatrices(width, height);
    if (renderStyle == Viewer3DRenderStyle::Textured) {
        DrawTexturedStyleBackgroundGradient(ComputeGridPlaneHorizonNdcY());
        DrawTexturedGroundPlaneBackdrop();
    }

    m_controller.SetCameraMoving(m_cameraMoving);
    m_controller.RenderScene(IsWireframeRenderStyle(renderStyle),
                             ToSceneRenderMode(renderStyle));

    glFlush();
}

void Viewer3DPanel::ApplyCameraMatrices(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr double kFovYDegrees = 45.0;
    constexpr double kNearPlane = 0.05;
    constexpr double kFarPlane = 2000.0;
    gluPerspective(kFovYDegrees, static_cast<double>(width) / height,
                   kNearPlane, kFarPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    m_camera.Apply();
}

// Handles mouse button press
void Viewer3DPanel::OnMouseDown(wxMouseEvent& event)
{
    if (event.LeftDown() || event.MiddleDown() || event.RightDown())
    {
        if (event.LeftDown() && event.ControlDown()) {
            m_rectSelecting = true;
            m_controller.SetInteracting(true);
            m_isInteracting = true;
            m_cameraMoving = true;
            m_lastInteractionTime = std::chrono::steady_clock::now();
            m_rectSelectStart = event.GetPosition();
            m_rectSelectEnd = m_rectSelectStart;
            m_draggedSincePress = false;
            CaptureMouse();
            return;
        }

        if (event.ShiftDown() || event.MiddleDown())
            m_mode = InteractionMode::Pan;
        else
            m_mode = InteractionMode::Orbit;

        m_dragging = true;
        m_controller.SetInteracting(true);
        m_isInteracting = true;
        m_cameraMoving = true;
        m_lastInteractionTime = std::chrono::steady_clock::now();
        m_draggedSincePress = false;
        m_lastMousePos = event.GetPosition();
        SetFocus();
        CaptureMouse();
    }
}

// Handles mouse button release
void Viewer3DPanel::OnMouseUp(wxMouseEvent& event)
{
    if (event.LeftUp() && m_rectSelecting)
    {
        if (HasCapture())
            ReleaseMouse();
        ApplyRectangleSelection(m_rectSelectStart, m_rectSelectEnd);
        m_rectSelecting = false;
        m_dragging = false;
        m_lastInteractionTime = std::chrono::steady_clock::now();
        m_mode = InteractionMode::None;
        m_draggedSincePress = false;
        Refresh();
        return;
    }

    if (m_dragging && (event.LeftUp() || event.MiddleUp() || event.RightUp()))
    {
        m_dragging = false;
        m_isInteracting = false;
        m_cameraMoving = false;
        m_controller.SetInteracting(false);
        m_controller.SetCameraMoving(false);
        m_mode = InteractionMode::None;
        if (HasCapture())
            ReleaseMouse();
        Refresh();
    }

    if (event.LeftUp() && !m_draggedSincePress)
    {
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
        if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
            found = m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label, pos, &uuid);
        else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
            found = m_controller.GetTrussLabelAt(event.GetX(), event.GetY(), w, h, label, pos, &uuid);
        else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
            found = m_controller.GetSceneObjectLabelAt(event.GetX(), event.GetY(), w, h, label, pos, &uuid);

        ConfigManager& cfg = ConfigManager::Get();
        if (found)
        {
            bool additive = event.ShiftDown() || event.ControlDown();
            std::vector<std::string> selection;
            if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
            {
                if (additive)
                    selection = FixtureTablePanel::Instance()->GetSelectedUuids();
                if (additive)
                {
                    auto it = std::find(selection.begin(), selection.end(), uuid);
                    if (it != selection.end())
                        selection.erase(it);
                    else
                        selection.push_back(uuid);
                }
                else
                    selection = {uuid};
                if (selection != cfg.GetSelectedFixtures()) {
                    cfg.PushUndoState("fixture selection");
                    cfg.SetSelectedFixtures(selection);
                }
                SetSelectedFixtures(selection);
                FixtureTablePanel::Instance()->SelectByUuid(selection);
            }
            else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
            {
                if (additive)
                    selection = TrussTablePanel::Instance()->GetSelectedUuids();
                if (additive)
                {
                    auto it = std::find(selection.begin(), selection.end(), uuid);
                    if (it != selection.end())
                        selection.erase(it);
                    else
                        selection.push_back(uuid);
                }
                else
                    selection = {uuid};
                if (selection != cfg.GetSelectedTrusses()) {
                    cfg.PushUndoState("truss selection");
                    cfg.SetSelectedTrusses(selection);
                }
                SetSelectedFixtures(selection);
                TrussTablePanel::Instance()->SelectByUuid(selection);
            }
            else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
            {
                if (additive)
                    selection = SceneObjectTablePanel::Instance()->GetSelectedUuids();
                if (additive)
                {
                    auto it = std::find(selection.begin(), selection.end(), uuid);
                    if (it != selection.end())
                        selection.erase(it);
                    else
                        selection.push_back(uuid);
                }
                else
                    selection = {uuid};
                if (selection != cfg.GetSelectedSceneObjects()) {
                    cfg.PushUndoState("scene object selection");
                    cfg.SetSelectedSceneObjects(selection);
                }
                SetSelectedFixtures(selection);
                SceneObjectTablePanel::Instance()->SelectByUuid(selection);
            }
        }
        else
        {
            if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage()) {
                if (!cfg.GetSelectedFixtures().empty()) {
                    cfg.PushUndoState("fixture selection");
                    cfg.SetSelectedFixtures({});
                }
                SetSelectedFixtures({});
                FixtureTablePanel::Instance()->ClearSelection();
            }
            else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage()) {
                if (!cfg.GetSelectedTrusses().empty()) {
                    cfg.PushUndoState("truss selection");
                    cfg.SetSelectedTrusses({});
                }
                SetSelectedFixtures({});
                TrussTablePanel::Instance()->ClearSelection();
            }
            else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage()) {
                if (!cfg.GetSelectedSceneObjects().empty()) {
                    cfg.PushUndoState("scene object selection");
                    cfg.SetSelectedSceneObjects({});
                }
                SetSelectedFixtures({});
                SceneObjectTablePanel::Instance()->ClearSelection();
            }
            else {
                SetSelectedFixtures({});
            }
        }
    }
    m_draggedSincePress = false;
}


void Viewer3DPanel::OnRightUp(wxMouseEvent& event)
{
    if (m_draggedSincePress)
        return;

    int w, h;
    GetClientSize(&w, &h);
    if (w <= 0 || h <= 0 || !IsShownOnScreen()) {
        event.Skip();
        return;
    }

    const bool fixturePageActive =
        FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage();
    const auto& scene = ConfigManager::Get().GetScene();
    std::set<std::string> typeNames;
    std::set<std::string> positionNames;
    bool hasNoPosition = false;
    bool showSelectionSubmenus = false;
    if (fixturePageActive && !scene.fixtures.empty()) {
        SetCurrent(*m_glContext);
        wxString label;
        wxPoint pos;
        std::string hitUuid;
        if (!m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label, pos,
                                            &hitUuid)) {
            for (const auto& [uuid, fixture] : scene.fixtures) {
                if (!fixture.typeName.empty())
                    typeNames.insert(fixture.typeName);
                if (fixture.positionName.empty())
                    hasNoPosition = true;
                else
                    positionNames.insert(fixture.positionName);
            }
            showSelectionSubmenus = true;
        }
    }

    wxMenu rootMenu;
    auto typeSubmenu = std::make_unique<wxMenu>();
    auto positionSubmenu = std::make_unique<wxMenu>();
    auto renderStyleSubmenu = std::make_unique<wxMenu>();

    constexpr int kSelectTypeAllId = wxID_HIGHEST + 900;
    constexpr int kSelectTypeBaseId = wxID_HIGHEST + 901;
    constexpr int kSelectPositionNoneId = wxID_HIGHEST + 1100;
    constexpr int kSelectPositionBaseId = wxID_HIGHEST + 1101;
    constexpr int kRenderStyleStandardId = wxID_HIGHEST + 1200;
    constexpr int kRenderStyleWhiteModelId = wxID_HIGHEST + 1201;
    constexpr int kRenderStyleTexturedId = wxID_HIGHEST + 1202;
    constexpr int kRenderStyleWireframeId = wxID_HIGHEST + 1203;
    constexpr int kRenderStyleByDeviceTypeId = wxID_HIGHEST + 1204;
    constexpr int kRenderStyleByLayerId = wxID_HIGHEST + 1205;
    constexpr int kRenderStyleByUniverseId = wxID_HIGHEST + 1206;

    typeSubmenu->Append(kSelectTypeAllId, "All fixtures");
    std::vector<std::string> orderedTypes;
    orderedTypes.reserve(typeNames.size());
    int nextTypeId = kSelectTypeBaseId;
    for (const auto& typeName : typeNames) {
        orderedTypes.push_back(typeName);
        typeSubmenu->Append(nextTypeId++, wxString::FromUTF8(typeName));
    }

    positionSubmenu->Append(kSelectPositionNoneId, "No position");
    std::vector<std::string> orderedPositions;
    orderedPositions.reserve(positionNames.size());
    int nextPositionId = kSelectPositionBaseId;
    for (const auto& positionName : positionNames) {
        orderedPositions.push_back(positionName);
        positionSubmenu->Append(nextPositionId++, wxString::FromUTF8(positionName));
    }

    renderStyleSubmenu->AppendRadioItem(kRenderStyleStandardId, "Standard");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWhiteModelId, "White model");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleTexturedId, "Textured");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWireframeId, "Wireframe");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleByDeviceTypeId, "By device type");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleByLayerId, "By layer");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleByUniverseId, "By universe");
    const Viewer3DRenderStyle activeRenderStyle = ResolveRenderStyleFromPreferences();
    switch (activeRenderStyle) {
        case Viewer3DRenderStyle::Wireframe:
            renderStyleSubmenu->Check(kRenderStyleWireframeId, true);
            break;
        case Viewer3DRenderStyle::ByDeviceType:
            renderStyleSubmenu->Check(kRenderStyleByDeviceTypeId, true);
            break;
        case Viewer3DRenderStyle::ByLayer:
            renderStyleSubmenu->Check(kRenderStyleByLayerId, true);
            break;
        case Viewer3DRenderStyle::ByUniverse:
            renderStyleSubmenu->Check(kRenderStyleByUniverseId, true);
            break;
        case Viewer3DRenderStyle::WhiteModel:
            renderStyleSubmenu->Check(kRenderStyleWhiteModelId, true);
            break;
        case Viewer3DRenderStyle::Textured:
            renderStyleSubmenu->Check(kRenderStyleTexturedId, true);
            break;
        case Viewer3DRenderStyle::Standard:
        default:
            renderStyleSubmenu->Check(kRenderStyleStandardId, true);
            break;
    }

    if (showSelectionSubmenus) {
        rootMenu.AppendSubMenu(typeSubmenu.release(), "Select by fixture type");
        rootMenu.AppendSubMenu(positionSubmenu.release(), "Select by position");
        rootMenu.AppendSeparator();
    }
    rootMenu.AppendSubMenu(renderStyleSubmenu.release(), "Render style");

    const int selectedId = GetPopupMenuSelectionFromUser(rootMenu, event.GetPosition());
    if (selectedId == wxID_NONE)
        return;

    auto applyRenderStyleSelection = [this](Viewer3DRenderStyle style) {
        ConfigManager::Get().SetValue("viewer3d_render_style", ToConfigValue(style));
        Refresh();
    };

    if (selectedId == kRenderStyleStandardId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::Standard);
        return;
    }
    if (selectedId == kRenderStyleWhiteModelId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::WhiteModel);
        return;
    }
    if (selectedId == kRenderStyleTexturedId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::Textured);
        return;
    }
    if (selectedId == kRenderStyleWireframeId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::Wireframe);
        return;
    }
    if (selectedId == kRenderStyleByDeviceTypeId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::ByDeviceType);
        return;
    }
    if (selectedId == kRenderStyleByLayerId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::ByLayer);
        return;
    }
    if (selectedId == kRenderStyleByUniverseId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::ByUniverse);
        return;
    }

    if (selectedId == kSelectTypeAllId) {
        ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, ""), this,
                                  m_controller);
        Refresh();
        return;
    }

    if (selectedId >= kSelectTypeBaseId &&
        selectedId < kSelectTypeBaseId + static_cast<int>(orderedTypes.size())) {
        const size_t idx = static_cast<size_t>(selectedId - kSelectTypeBaseId);
        ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, orderedTypes[idx]), this,
                                  m_controller);
        Refresh();
        return;
    }

    if (selectedId == kSelectPositionNoneId) {
        if (!hasNoPosition) {
            ApplyFixtureSelectionToUi({}, this, m_controller);
        } else {
            ApplyFixtureSelectionToUi(BuildFixtureSelectionByPosition(scene, "", true),
                                      this, m_controller);
        }
        Refresh();
        return;
    }

    if (selectedId >= kSelectPositionBaseId &&
        selectedId < kSelectPositionBaseId + static_cast<int>(orderedPositions.size())) {
        const size_t idx = static_cast<size_t>(selectedId - kSelectPositionBaseId);
        ApplyFixtureSelectionToUi(
            BuildFixtureSelectionByPosition(scene, orderedPositions[idx], false), this,
            m_controller);
        Refresh();
        return;
    }
}

void Viewer3DPanel::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    m_dragging = false;
    m_isInteracting = false;
    m_cameraMoving = false;
    m_controller.SetInteracting(false);
    m_controller.SetCameraMoving(false);
    m_mode = InteractionMode::None;
    m_rectSelecting = false;
}

void Viewer3DPanel::ApplyRectangleSelection(const wxPoint& start,
                                            const wxPoint& end)
{
    int w, h;
    GetClientSize(&w, &h);
    if (w <= 0 || h <= 0 || !IsShownOnScreen()) {
        return;
    }

    SetCurrent(*m_glContext);

    ConfigManager& cfg = ConfigManager::Get();
    if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
    {
        auto selection = m_controller.GetFixturesInScreenRect(start.x, start.y, end.x, end.y, w, h);
        if (selection != cfg.GetSelectedFixtures()) {
            cfg.PushUndoState("fixture selection");
            cfg.SetSelectedFixtures(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            FixtureTablePanel::Instance()->ClearSelection();
        else
            FixtureTablePanel::Instance()->SelectByUuid(selection);
    }
    else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
    {
        auto selection = m_controller.GetTrussesInScreenRect(start.x, start.y, end.x, end.y, w, h);
        if (selection != cfg.GetSelectedTrusses()) {
            cfg.PushUndoState("truss selection");
            cfg.SetSelectedTrusses(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            TrussTablePanel::Instance()->ClearSelection();
        else
            TrussTablePanel::Instance()->SelectByUuid(selection);
    }
    else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
    {
        auto selection =
            m_controller.GetSceneObjectsInScreenRect(start.x, start.y, end.x, end.y, w, h);
        if (selection != cfg.GetSelectedSceneObjects()) {
            cfg.PushUndoState("scene object selection");
            cfg.SetSelectedSceneObjects(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            SceneObjectTablePanel::Instance()->ClearSelection();
        else
            SceneObjectTablePanel::Instance()->SelectByUuid(selection);
    }
}

void Viewer3DPanel::DrawSelectionRectangle(int width, int height)
{
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
    glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
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

// Handles mouse movement (orbit or pan)
void Viewer3DPanel::OnMouseMove(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();

    if (m_rectSelecting && event.Dragging())
    {
        m_rectSelectEnd = pos;
        m_draggedSincePress = true;
        Refresh();
        return;
    }

    if (m_dragging && event.Dragging())
    {
        int dx = pos.x - m_lastMousePos.x;
        int dy = pos.y - m_lastMousePos.y;

        m_draggedSincePress = true;
        m_isInteracting = true;
        m_cameraMoving = true;
        m_lastInteractionTime = std::chrono::steady_clock::now();

        if (m_mode == InteractionMode::Orbit &&
            (event.LeftIsDown() || event.RightIsDown()))
        {
            m_camera.targetYaw += dx * 0.5f;
            m_camera.targetPitch += -dy * 0.5f;
            m_camera.targetPitch = std::clamp(m_camera.targetPitch, -89.0f, 89.0f);
        }
        else if (m_mode == InteractionMode::Pan &&
                 (event.MiddleIsDown() || event.RightIsDown() || event.ShiftDown()))
        {
            m_camera.Pan(-dx * 0.01f, dy * 0.01f);
        }
    }

    m_lastMousePos = pos;

    // Mark that the mouse has moved so OnPaint can update hover info
    m_mouseMoved = true;

    Refresh();
}

// Handles mouse wheel (zoom)
void Viewer3DPanel::OnMouseWheel(wxMouseEvent& event)
{
    SetFocus();

    // wxWidgets may report multiple wheel detents in a single event.
    // Use the ratio of the rotation to the wheel delta to scale zoom
    // steps accordingly so that large scrolls result in proportionally
    // larger zoom changes.
    int rotation = event.GetWheelRotation();
    int deltaWheel = event.GetWheelDelta();
    float steps = 0.0f;
    if (deltaWheel != 0)
        steps = -static_cast<float>(rotation) / static_cast<float>(deltaWheel);
    m_controller.SetInteracting(true);
    m_isInteracting = true;
    m_cameraMoving = true;
    m_lastInteractionTime = std::chrono::steady_clock::now();

    m_camera.Zoom(steps);

    Refresh();
}

void Viewer3DPanel::OnMouseDClick(wxMouseEvent& event)
{
    int w, h;
    GetClientSize(&w, &h);
    SetCurrent(*m_glContext);
    wxString label;
    wxPoint pos;
    std::string uuid;
    const bool cameraWasMoving = m_cameraMoving;
    if (cameraWasMoving)
        m_controller.SetCameraMoving(false);

    const bool found =
        m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label,
                                       pos, &uuid);

    if (cameraWasMoving)
        m_controller.SetCameraMoving(true);

    if (!found && !m_hoverUuid.empty())
        uuid = m_hoverUuid;
    if (uuid.empty())
        return;

    auto& scene = ConfigManager::Get().GetScene();
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

    if (FixtureTablePanel::Instance()) {
        FixtureTablePanel::Instance()->ReloadData();
    }

    Refresh();
}

void Viewer3DPanel::OnKeyDown(wxKeyEvent& event)
{
    if (!m_mouseInside && !HasFocus()) { event.Skip(); return; }

    bool shift = event.ShiftDown();
    bool alt = event.AltDown();

    switch (event.GetKeyCode()) {
        case WXK_LEFT:
            if (shift)
                m_camera.Pan(-0.1f, 0.0f);
            else if (alt)
                m_camera.Zoom(-1.0f);
            else
                m_camera.targetYaw += -5.0f;
            break;
        case WXK_RIGHT:
            if (shift)
                m_camera.Pan(0.1f, 0.0f);
            else if (alt)
                m_camera.Zoom(1.0f);
            else
                m_camera.targetYaw += 5.0f;
            break;
        case WXK_UP:
            if (shift)
                m_camera.Pan(0.0f, 0.1f);
            else if (alt)
                m_camera.Zoom(-1.0f);
            else
                m_camera.targetPitch = std::clamp(m_camera.targetPitch + 5.0f, -89.0f, 89.0f);
            break;
        case WXK_DOWN:
            if (shift)
                m_camera.Pan(0.0f, -0.1f);
            else if (alt)
                m_camera.Zoom(1.0f);
            else
                m_camera.targetPitch = std::clamp(m_camera.targetPitch - 5.0f, -89.0f, 89.0f);
            break;
        case WXK_NUMPAD1: // Front
            SetStandardView(Viewer2DView::Front);
            break;
        case WXK_NUMPAD3: // Right
            SetStandardView(Viewer2DView::Side);
            break;
        case WXK_NUMPAD7: // Top
            SetStandardView(Viewer2DView::Top);
            break;
        case WXK_NUMPAD5: // Reset/isometric
            m_camera.Reset();
            break;
        case WXK_DELETE:
        case WXK_NUMPAD_DELETE: {
            if (MainWindow::Instance()) {
                wxCommandEvent deleteEvent(wxEVT_MENU, ID_Edit_Delete);
                MainWindow::Instance()->GetEventHandler()->ProcessEvent(deleteEvent);
                return;
            }
            event.Skip();
            return;
        }
        case 'F':
        case 'f': {
            int width = 0;
            int height = 0;
            GetClientSize(&width, &height);
            if (!viewer3d::FrameSceneInCamera(m_controller, width, height, m_camera)) {
                event.Skip();
                return;
            }
            m_isInteracting = true;
            m_cameraMoving = true;
            m_lastInteractionTime = std::chrono::steady_clock::now();
            m_controller.SetInteracting(true);
            break;
        }
        default:
            event.Skip();
            return;
    }

    Refresh();
}

void Viewer3DPanel::SetStandardView(Viewer2DView view) {
    switch (view) {
        case Viewer2DView::Top:
            m_camera.SetOrientation(0.0f, 89.0f);
            break;
        case Viewer2DView::Front:
            m_camera.SetOrientation(0.0f, 0.0f);
            break;
        case Viewer2DView::Side:
            m_camera.SetOrientation(90.0f, 0.0f);
            break;
        default:
            return;
    }
    Refresh();
}

void Viewer3DPanel::OnMouseEnter(wxMouseEvent& event)
{
    m_mouseInside = true;
    SetFocus();
    event.Skip();
}

void Viewer3DPanel::OnMouseLeave(wxMouseEvent& event)
{
    m_mouseInside = false;
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
    event.Skip();
}

// Updates the controller with current scene data
void Viewer3DPanel::UpdateScene()
{
    m_sceneSyncPending = true;

    if (ShouldPauseHeavyTasks() || m_cameraMoving)
        return;

    m_controller.UpdateResourcesIfDirty();
    m_sceneSyncPending = false;
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->UpdateScene();
}

void Viewer3DPanel::SetSelectedFixtures(const std::vector<std::string>& uuids)
{
    m_controller.SetSelectedUuids(uuids);
    Refresh();
}

void Viewer3DPanel::SetLayerColor(const std::string& layer, const std::string& hex)
{
    m_controller.SetLayerColor(layer, hex);
}

std::shared_ptr<const SymbolDefinitionSnapshot>
Viewer3DPanel::GetBottomSymbolCacheSnapshot() const
{
    return m_controller.GetBottomSymbolCacheSnapshot();
}

static Viewer3DPanel* s_instance = nullptr;

Viewer3DPanel* Viewer3DPanel::Instance()
{
    return s_instance;
}

void Viewer3DPanel::SetInstance(Viewer3DPanel* panel)
{
    s_instance = panel;
}

void Viewer3DPanel::RefreshLoop()
{
    using namespace std::chrono_literals;
    while (m_threadRunning)
    {
        wxThreadEvent* evt = new wxThreadEvent(wxEVT_VIEWER_REFRESH);
        wxQueueEvent(this, evt);
        std::this_thread::sleep_for(16ms);
    }
}

void Viewer3DPanel::OnThreadRefresh(wxThreadEvent& event)
{
    Refresh();
}


bool Viewer3DPanel::ShouldPauseHeavyTasks()
{
    if (!m_isInteracting)
        return false;

    const auto now = std::chrono::steady_clock::now();
    if ((now - m_lastInteractionTime) < kPauseDelay)
        return true;

    m_isInteracting = false;
    m_cameraMoving = false;
    m_controller.SetInteracting(false);
    m_controller.SetCameraMoving(false);

    const bool fastInteractionMode = IsFastInteractionModeEnabled();

    if (fastInteractionMode) {
        m_controller.UpdateResourcesIfDirty();
        m_sceneSyncPending = false;
        m_mouseMoved = true;
    }

    return false;
}

void Viewer3DPanel::LoadCameraFromConfig()
{
    ConfigManager& cfg = ConfigManager::Get();

    float yaw = cfg.GetFloat("camera_yaw");
    float pitch = cfg.GetFloat("camera_pitch");
    float dist = cfg.GetFloat("camera_distance");
    float tx = cfg.GetFloat("camera_target_x");
    float ty = cfg.GetFloat("camera_target_y");
    float tz = cfg.GetFloat("camera_target_z");

    m_camera.SetOrientation(yaw, pitch);
    m_camera.SetDistance(dist);
    m_camera.SetTarget(tx, ty, tz);

    if (ConsolePanel::Instance()) {
        wxString msg;
        msg.Printf("Camera loaded: yaw=%.2f pitch=%.2f dist=%.2f target=(%.2f, %.2f, %.2f)",
            yaw, pitch, dist, tx, ty, tz);
        ConsolePanel::Instance()->AppendMessage(msg);
    }
}
