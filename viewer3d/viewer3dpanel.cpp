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
#include "glew_init_utils.h"
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
#include "editable_focus_utils.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "scene_object_primitive_editing.h"
#include "../gui/selection_origin_token.h"
#include "configmanager.h"
#include "fixturepatchdialog.h"
#include "viewer2dpanel.h"
#include "viewer3dviewfit.h"
#include "viewer3d_render_style.h"
#include "base_pass_framebuffer_cache.h"
#include "gl_state_guard.h"
#include "ui_render_size.h"
#include "../viewer_common/measure_overlay_style.h"
#include "units/units.h"
#include <wx/dcclient.h>
#include <wx/debug.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <chrono>
#include <array>
#include <algorithm>
#include <memory>
#include <cmath>
#include <set>
#include <cstdint>

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
wxEND_EVENT_TABLE()


namespace {
constexpr auto kPauseDelay = std::chrono::milliseconds(200);
constexpr int kZoomInteractionTimeoutMs = 260;
constexpr auto kHoverQueryInterval = std::chrono::milliseconds(40);
constexpr auto kResourceSyncInterval = std::chrono::milliseconds(250);
constexpr int kSelectionDragDelayMs = 120;
constexpr int kExportImageWidth = 1920;
constexpr int kExportImageHeight = 1080;
constexpr double kDefaultFovYDegrees = 45.0;

void ValidateGlStateAfterRender(const char* stage, int expectedWidth,
                                int expectedHeight) {
    GLint framebuffer = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);

    const bool validFramebuffer = framebuffer == 0;
    const bool validViewport =
        viewport[0] == 0 && viewport[1] == 0 &&
        viewport[2] == expectedWidth && viewport[3] == expectedHeight;
    if (!validFramebuffer || !validViewport) {
        wxLogTrace("viewer3d_gl_state",
                   "%s left unexpected GL state (fbo=%d viewport=%d,%d,%d,%d expected=0,0,%d,%d).",
                   stage, framebuffer, viewport[0], viewport[1], viewport[2],
                   viewport[3], expectedWidth, expectedHeight);
    }
    wxASSERT_MSG(validFramebuffer,
                 "Unexpected non-default framebuffer after 3D render.");
    wxASSERT_MSG(validViewport, "Unexpected viewport after 3D render.");
}

bool IsFastInteractionModeEnabled()
{
    return ConfigManager::Get().GetFloat("viewer3d_fast_interaction_mode") >= 0.5f;
}

bool IsOrbitInversionEnabled()
{
    const auto value = ConfigManager::Get().GetValue("viewer3d_invert_orbit");
    return value && *value == "1";
}

bool Is2DDarkModeEnabled() {
    return ConfigManager::Get().GetFloat("view2d_dark_mode") >= 0.5f;
}

template <typename T>
void HashCombine(size_t& seed, const T& value) {
    seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

size_t ComputeCameraFingerprint(const Viewer3DCamera& camera) {
    size_t cameraFingerprint = 0;
    HashCombine(cameraFingerprint, camera.GetYaw());
    HashCombine(cameraFingerprint, camera.GetPitch());
    HashCombine(cameraFingerprint, camera.GetDistance());
    HashCombine(cameraFingerprint, camera.GetTargetX());
    HashCombine(cameraFingerprint, camera.GetTargetY());
    HashCombine(cameraFingerprint, camera.GetTargetZ());
    HashCombine(cameraFingerprint, camera.targetYaw);
    HashCombine(cameraFingerprint, camera.targetPitch);
    HashCombine(cameraFingerprint, camera.targetDistance);
    HashCombine(cameraFingerprint, camera.targetTargetX);
    HashCombine(cameraFingerprint, camera.targetTargetY);
    HashCombine(cameraFingerprint, camera.targetTargetZ);
    return cameraFingerprint;
}

wxPoint ToFramebufferPoint(wxWindow* window, const wxPoint& logicalPoint) {
    if (window == nullptr)
        return logicalPoint;
    const double contentScale =
        static_cast<double>(window->GetContentScaleFactor());
    if (!std::isfinite(contentScale) || contentScale <= 0.0)
        return logicalPoint;
    return wxPoint(static_cast<int>(std::lround(
                       static_cast<double>(logicalPoint.x) * contentScale)),
                   static_cast<int>(std::lround(
                       static_cast<double>(logicalPoint.y) * contentScale)));
}

// Converts framebuffer pixel coordinates back to logical client-space pixels.
wxPoint FromFramebufferPoint(wxWindow* window, const wxPoint& framebufferPoint) {
    if (window == nullptr)
        return framebufferPoint;
    const double contentScale =
        static_cast<double>(window->GetContentScaleFactor());
    if (!std::isfinite(contentScale) || contentScale <= 0.0)
        return framebufferPoint;
    return wxPoint(static_cast<int>(std::lround(
                       static_cast<double>(framebufferPoint.x) / contentScale)),
                   static_cast<int>(std::lround(
                       static_cast<double>(framebufferPoint.y) / contentScale)));
}

// Normalizes label rotation so text remains readable while staying parallel to the measure line.
float NormalizeMeasureLabelAngleDegrees(float angleDegrees) {
    while (angleDegrees > 90.0f)
        angleDegrees -= 180.0f;
    while (angleDegrees < -90.0f)
        angleDegrees += 180.0f;
    return angleDegrees;
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
        case Viewer3DRenderStyle::White:
        case Viewer3DRenderStyle::WhiteModel:
        case Viewer3DRenderStyle::Textured:
        case Viewer3DRenderStyle::Standard:
        default:
            return Viewer2DRenderMode::White;
    }
}

double ComputeExpandedFovYDegrees(int sourceWidth, int sourceHeight,
                                  int targetWidth, int targetHeight) {
    constexpr double kPi = 3.14159265358979323846;
    if (sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0)
        return kDefaultFovYDegrees;

    const double sourceAspect =
        static_cast<double>(sourceWidth) / static_cast<double>(sourceHeight);
    const double targetAspect =
        static_cast<double>(targetWidth) / static_cast<double>(targetHeight);
    if (sourceAspect <= targetAspect)
        return kDefaultFovYDegrees;

    const double baseHalfFovRadians = (kDefaultFovYDegrees * 0.5) * kPi / 180.0;
    const double expandedHalfFovRadians =
        std::atan(std::tan(baseHalfFovRadians) * (sourceAspect / targetAspect));
    const double expandedFovDegrees = expandedHalfFovRadians * 2.0 * 180.0 / kPi;
    return std::clamp(expandedFovDegrees, kDefaultFovYDegrees, 170.0);
}

void ApplyViewer3DClearColorForStyle(Viewer3DRenderStyle style) {
    if (IsWhiteModelRenderStyle(style)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }
    if (style == Viewer3DRenderStyle::Wireframe) {
        if (Is2DDarkModeEnabled())
            glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        else
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
    GLint shadeModel = GL_SMOOTH;
    glGetIntegerv(GL_SHADE_MODEL, &shadeModel);

    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
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
    glShadeModel(shadeModel);
}

void DrawTexturedGroundPlaneBackdrop() {
    const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
    const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean texture2DWasEnabled = glIsEnabled(GL_TEXTURE_2D);
    GLint shadeModel = GL_SMOOTH;
    glGetIntegerv(GL_SHADE_MODEL, &shadeModel);

    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
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
    glShadeModel(shadeModel);
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
    selection::ScopedOrigin selectionOrigin(selection::Origin::Viewer3D);
    if (panel)
        panel->SetSelectedFixtures(selection);
    if (FixtureTablePanel::Instance()) {
        if (selection.empty())
            FixtureTablePanel::Instance()->ClearSelection();
        else
            FixtureTablePanel::Instance()->SelectByUuid(selection, false);
    }
}

Viewer3DPanel::HoverTargetTable ResolveActiveHoverTargetTable()
{
    if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
        return Viewer3DPanel::HoverTargetTable::Fixtures;
    if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
        return Viewer3DPanel::HoverTargetTable::Trusses;
    if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
        return Viewer3DPanel::HoverTargetTable::SceneObjects;
    return Viewer3DPanel::HoverTargetTable::None;
}

Viewer3DController::HoverPickTarget ToHoverPickTarget(Viewer3DPanel::HoverTargetTable target)
{
    switch (target) {
    case Viewer3DPanel::HoverTargetTable::Fixtures:
        return Viewer3DController::HoverPickTarget::Fixture;
    case Viewer3DPanel::HoverTargetTable::Trusses:
        return Viewer3DController::HoverPickTarget::Truss;
    case Viewer3DPanel::HoverTargetTable::SceneObjects:
        return Viewer3DController::HoverPickTarget::SceneObject;
    case Viewer3DPanel::HoverTargetTable::None:
    default:
        return Viewer3DController::HoverPickTarget::Fixture;
    }
}

bool QueryHoverUuid(Viewer3DController& controller,
                    Viewer3DPanel::HoverTargetTable target,
                    int mouseX, int mouseY, int width, int height,
                    std::string& outUuid,
                    bool confirmDepth = false)
{
    if (target == Viewer3DPanel::HoverTargetTable::None)
        return false;
    return controller.GetHoverUuidAt(mouseX, mouseY, width, height,
                                     ToHoverPickTarget(target), outUuid,
                                     confirmDepth);
}

// Projects a world-space point to framebuffer-space pixels using current GL matrices.
std::optional<std::array<float, 2>> ProjectWorldToFramebuffer(
    const std::array<float, 3>& worldMeters) {
    GLdouble model[16] = {};
    GLdouble projection[16] = {};
    GLint viewport[4] = {};
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLdouble winX = 0.0;
    GLdouble winY = 0.0;
    GLdouble winZ = 0.0;
    if (gluProject(static_cast<GLdouble>(worldMeters[0]),
                   static_cast<GLdouble>(worldMeters[1]),
                   static_cast<GLdouble>(worldMeters[2]), model, projection,
                   viewport, &winX, &winY, &winZ) != GL_TRUE) {
        return std::nullopt;
    }
    return std::array<float, 2>{static_cast<float>(winX),
                                static_cast<float>(winY)};
}

bool QueryDragLabelUuid(Viewer3DController& controller,
                        Viewer3DPanel::HoverTargetTable target, int mouseX,
                        int mouseY, int width, int height, std::string& outUuid) {
    wxString label;
    wxPoint labelPos;
    switch (target) {
    case Viewer3DPanel::HoverTargetTable::Fixtures:
        return controller.GetFixtureLabelAt(mouseX, mouseY, width, height, label,
                                            labelPos, &outUuid);
    case Viewer3DPanel::HoverTargetTable::Trusses:
        return controller.GetTrussLabelAt(mouseX, mouseY, width, height, label,
                                          labelPos, &outUuid);
    case Viewer3DPanel::HoverTargetTable::SceneObjects:
        return controller.GetSceneObjectLabelAt(mouseX, mouseY, width, height,
                                                label, labelPos, &outUuid);
    case Viewer3DPanel::HoverTargetTable::None:
    default:
        return false;
    }
}

std::array<float, 3> AxisVectorFromSelectionDragAxis(
    viewer3d::SelectionDragAxis axis) {
    switch (axis) {
    case viewer3d::SelectionDragAxis::X:
        return {1.0f, 0.0f, 0.0f};
    case viewer3d::SelectionDragAxis::Y:
        return {0.0f, 1.0f, 0.0f};
    case viewer3d::SelectionDragAxis::Z:
        return {0.0f, 0.0f, 1.0f};
    case viewer3d::SelectionDragAxis::None:
    default:
        return {0.0f, 0.0f, 0.0f};
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
    Bind(wxEVT_THREAD, &Viewer3DPanel::OnThreadRefresh, this, wxEVT_VIEWER_REFRESH);
    m_zoomInteractionTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &Viewer3DPanel::OnZoomInteractionTimeout, this,
         m_zoomInteractionTimer.GetId());
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        if (event.IsShown()) {
            m_controller.MarkResourceSyncPending();
            UpdateScene();
            Refresh();
        }
        event.Skip();
    });
    m_threadRunning = true;
    m_lastResourceSyncCheck = std::chrono::steady_clock::now();
    m_basePassCache = std::make_unique<BasePassFramebufferCache>();
    m_refreshThread = std::thread(&Viewer3DPanel::RefreshLoop, this);
}

Viewer3DPanel::~Viewer3DPanel()
{
    m_shuttingDown = true;
    Unbind(wxEVT_THREAD, &Viewer3DPanel::OnThreadRefresh, this, wxEVT_VIEWER_REFRESH);
    Unbind(wxEVT_TIMER, &Viewer3DPanel::OnZoomInteractionTimeout, this,
           m_zoomInteractionTimer.GetId());
    m_zoomInteractionTimer.Stop();
    DeletePendingEvents();
    if (HasCapture())
        ReleaseMouse();
    StopRefreshThread();
    if (m_basePassCache && m_glContext && IsShownOnScreen()) {
        SetCurrent(*m_glContext);
        m_basePassCache.reset();
    } else if (m_basePassCache) {
        m_basePassCache->AbandonResources();
        m_basePassCache.reset();
    }
    delete m_glContext;
    m_glContext = nullptr;
    SetInstance(nullptr);
}

void Viewer3DPanel::StopRefreshThread()
{
    m_threadRunning = false;
    if (m_refreshThread.joinable())
        m_refreshThread.join();
}

// Initializes 3D OpenGL state only after centralized GLEW/context validation.
void Viewer3DPanel::InitGL()
{
    if (!IsShownOnScreen()) {
        return;
    }
    if (!m_glInitialized) {
        const GLEWInitResult initResult =
            InitializeGlewForCurrentContext(*this, *m_glContext, "Viewer3DPanel");
        if (!initResult.success) {
            wxLogError("%s", initResult.message);
            return;
        }
        if (initResult.isWarningOnly) {
            wxLogDebug("%s", initResult.message);
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
    if (!IsShownOnScreen() || m_modalDialogActive) {
        return;
    }
    InitGL();

    const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
    const bool highlightOnlyRefresh =
        m_highlightRefreshPending &&
        !m_controller.IsResourceSyncPending() &&
        !m_selectionRefreshPending &&
        !m_cameraMoving &&
        !m_isInteracting &&
        !m_mouseMoved &&
        !m_forceHoverQuery;
    const size_t cameraFingerprint = ComputeCameraFingerprint(m_camera);
    const auto hiddenLayers = ConfigManager::Get().GetHiddenLayers();
    const size_t sceneVersion = m_controller.GetSceneVersionSnapshot();

    m_controller.ResetDebugPerFrameCounters();
    m_controller.UpdateFrameStateLightweight();

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

    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid()) {
        return;
    }

    bool reusedBasePass = false;
    if (highlightOnlyRefresh && m_basePassCache) {
        reusedBasePass = m_basePassCache->RestoreToDefaultFramebuffer(
            renderSize.width, renderSize.height, cameraFingerprint,
            hiddenLayers, sceneVersion);
    }

    if (!reusedBasePass) {
        const auto fullRenderStart = std::chrono::steady_clock::now();
        Render(renderSize);
        const auto fullRenderElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - fullRenderStart)
                .count();
        m_fullRenderMsAccumInCurrentWindow += fullRenderElapsedMs;
        ++m_fullRenderSamplesInCurrentWindow;
        if (m_basePassCache && !m_cameraMoving && !m_isInteracting) {
            m_basePassCache->CaptureFromDefaultFramebuffer(
                renderSize.width, renderSize.height, cameraFingerprint,
                hiddenLayers, sceneVersion);
        }
    }

    // Ensure the OpenGL context is current before drawing overlays
    SetCurrent(*m_glContext);

    const int w = renderSize.width;
    const int h = renderSize.height;

    wxString newLabel;
    wxPoint newPos;
    std::string newUuid;
    const std::string oldHoverUuid = m_hoverUuid;
    const bool oldHasHover = m_hasHover;
    bool found = false;
    bool hoverQueryRan = false;

    const bool skipLabelsWhenMoving =
        ConfigManager::Get().GetFloat("viewer3d_skip_labels_when_moving") >= 0.5f;
    const bool skipLabelWork = m_cameraMoving &&
        (IsFastInteractionModeEnabled() || skipLabelsWhenMoving);

    if (cameraFingerprint != m_lastCameraFingerprint) {
        ++m_cameraRevision;
        m_lastCameraFingerprint = cameraFingerprint;
    }

    size_t hiddenLayersFingerprint = 0;
    for (const std::string& layer : hiddenLayers)
        HashCombine(hiddenLayersFingerprint, layer);
    if (hiddenLayersFingerprint != m_lastHiddenLayersFingerprint) {
        ++m_hiddenLayersRevision;
        m_lastHiddenLayersFingerprint = hiddenLayersFingerprint;
    }

    const HoverTargetTable activeTable = ResolveActiveHoverTargetTable();

    if (activeTable != m_lastHoverTargetTable) {
        m_forceHoverQuery = true;
        m_lastHoverTargetTable = activeTable;
    }

    const wxPoint pickPos = ToFramebufferPoint(this, m_lastMousePos);
    const HoverQueryState currentHoverQueryState{
        pickPos,
        m_cameraRevision,
        m_hiddenLayersRevision,
        m_sceneRevision
    };
    const bool hoverStateChanged = !m_hasLastHoverQueryState ||
        currentHoverQueryState.mouseFramebufferPos != m_lastHoverQueryState.mouseFramebufferPos ||
        currentHoverQueryState.cameraRevision != m_lastHoverQueryState.cameraRevision ||
        currentHoverQueryState.hiddenLayersRevision != m_lastHoverQueryState.hiddenLayersRevision ||
        currentHoverQueryState.sceneRevision != m_lastHoverQueryState.sceneRevision;
    const bool shouldUpdateHoverQuery =
        m_forceHoverQuery || m_mouseMoved || hoverStateChanged || !m_hasHover;
    const auto nowForHover = std::chrono::steady_clock::now();
    const bool hoverCadenceDue = m_forceHoverQuery ||
        (nowForHover - m_lastHoverQueryTime) >= kHoverQueryInterval;
    const bool shouldRunHoverQuery =
        (!skipLabelWork || m_forceHoverQuery) &&
        shouldUpdateHoverQuery && hoverCadenceDue &&
        activeTable != HoverTargetTable::None &&
        (m_forceHoverQuery || hoverStateChanged);

    if (shouldRunHoverQuery) {
        const auto hoverQueryStart = std::chrono::steady_clock::now();
        found = QueryHoverUuid(m_controller, activeTable, pickPos.x, pickPos.y,
                               w, h, newUuid);
        const auto hoverQueryElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - hoverQueryStart)
                .count();
        m_hoverQueryMsAccumInCurrentWindow += hoverQueryElapsedMs;
        ++m_hoverQuerySamplesInCurrentWindow;
        hoverQueryRan = true;
        if (found) {
            if (!skipLabelWork) {
                wxString tooltipLabel;
                wxPoint tooltipPos;
                if (activeTable == HoverTargetTable::Fixtures) {
                    m_controller.GetFixtureLabelAt(pickPos.x, pickPos.y,
                        w, h, tooltipLabel, tooltipPos, nullptr);
                } else if (activeTable == HoverTargetTable::Trusses) {
                    m_controller.GetTrussLabelAt(pickPos.x, pickPos.y,
                        w, h, tooltipLabel, tooltipPos, nullptr);
                } else if (activeTable == HoverTargetTable::SceneObjects) {
                    m_controller.GetSceneObjectLabelAt(pickPos.x, pickPos.y,
                        w, h, tooltipLabel, tooltipPos, nullptr);
                }
                newLabel = tooltipLabel;
                newPos = tooltipPos;
            } else {
                newLabel.clear();
            }
        }
    }

    if (hoverQueryRan) {
        m_lastHoverQueryState = currentHoverQueryState;
        m_hasLastHoverQueryState = true;
        m_lastHoverQueryTime = nowForHover;
        m_forceHoverQuery = false;
    }

    if (hoverQueryRan && found) {
        m_hasHover = true;
        m_hoverUuid = newUuid;
        if (!skipLabelWork) {
            m_hoverText = newLabel;
            m_hoverPos = newPos;
        } else {
            m_hoverText.clear();
        }
    }
    else if (hoverQueryRan && !skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
    } else if (skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
    }

    if (oldHoverUuid != m_hoverUuid || oldHasHover != m_hasHover) {
        const auto highlightUpdateStart = std::chrono::steady_clock::now();
        ++m_highlightRevision;
        m_highlightRefreshPending = true;
        m_controller.SetHighlightUuid(m_hoverUuid);
        if (FixtureTablePanel::Instance()) {
            FixtureTablePanel::Instance()->HighlightFixture(
                FixtureTablePanel::Instance()->IsActivePage()
                    ? std::string(m_hoverUuid)
                    : std::string());
        }
        if (TrussTablePanel::Instance()) {
            TrussTablePanel::Instance()->HighlightTruss(
                TrussTablePanel::Instance()->IsActivePage()
                    ? std::string(m_hoverUuid)
                    : std::string());
        }
        if (SceneObjectTablePanel::Instance()) {
            SceneObjectTablePanel::Instance()->HighlightObject(
                SceneObjectTablePanel::Instance()->IsActivePage()
                    ? std::string(m_hoverUuid)
                    : std::string());
        }
        const auto highlightUpdateElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - highlightUpdateStart)
                .count();
        m_highlightUpdateMsAccumInCurrentWindow += highlightUpdateElapsedMs;
        ++m_highlightUpdateSamplesInCurrentWindow;
    }
    m_mouseMoved = false;

    // Draw labels before swapping buffers to avoid losing them.
    if (!highlightOnlyRefresh && !pauseHeavyTasks && !skipLabelWork) {
        if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
            m_controller.DrawFixtureLabels(w, h);
        else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
            m_controller.DrawTrussLabels(w, h);
        else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
            m_controller.DrawSceneObjectLabels(w, h);
    }

    if (m_rectSelecting)
        DrawSelectionRectangle(w, h);
    if (m_selectionDragArmed)
        DrawSelectionDragGizmo(renderSize);
    DrawMeasureOverlay(renderSize);

    if (reusedBasePass)
        ++m_highlightRefreshesInCurrentWindow;
    else
        ++m_fullRefreshesInCurrentWindow;

    const auto telemetryNow = std::chrono::steady_clock::now();
    if (m_refreshTelemetryWindowStart.time_since_epoch().count() == 0)
        m_refreshTelemetryWindowStart = telemetryNow;
    const auto telemetryElapsed = telemetryNow - m_refreshTelemetryWindowStart;
    if (telemetryElapsed >= std::chrono::seconds(1)) {
        const double avgFullRenderMs = m_fullRenderSamplesInCurrentWindow > 0
            ? (m_fullRenderMsAccumInCurrentWindow /
               static_cast<double>(m_fullRenderSamplesInCurrentWindow))
            : 0.0;
        const double avgHoverQueryMs = m_hoverQuerySamplesInCurrentWindow > 0
            ? (m_hoverQueryMsAccumInCurrentWindow /
               static_cast<double>(m_hoverQuerySamplesInCurrentWindow))
            : 0.0;
        const double avgHighlightUpdateMs =
            m_highlightUpdateSamplesInCurrentWindow > 0
                ? (m_highlightUpdateMsAccumInCurrentWindow /
                   static_cast<double>(m_highlightUpdateSamplesInCurrentWindow))
                : 0.0;
        wxLogDebug(
            "Viewer3DPanel refreshes/s full=%d highlight=%d full_render_ms=%.3f hover_query_ms=%.3f highlight_update_ms=%.3f hover_samples=%d highlight_samples=%d",
                   m_fullRefreshesInCurrentWindow,
                   m_highlightRefreshesInCurrentWindow, avgFullRenderMs,
                   avgHoverQueryMs, avgHighlightUpdateMs,
                   m_hoverQuerySamplesInCurrentWindow,
                   m_highlightUpdateSamplesInCurrentWindow);
        m_refreshTelemetryWindowStart = telemetryNow;
        m_fullRefreshesInCurrentWindow = 0;
        m_highlightRefreshesInCurrentWindow = 0;
        m_fullRenderMsAccumInCurrentWindow = 0.0;
        m_fullRenderSamplesInCurrentWindow = 0;
        m_hoverQueryMsAccumInCurrentWindow = 0.0;
        m_hoverQuerySamplesInCurrentWindow = 0;
        m_highlightUpdateMsAccumInCurrentWindow = 0.0;
        m_highlightUpdateSamplesInCurrentWindow = 0;
    }

    if (reusedBasePass)
        m_highlightRefreshPending = false;
    if (m_selectionRefreshPending)
        m_selectionRefreshPending = false;

    SwapBuffers(); // Swap after drawing labels to ensure they are visible
}

// Resize event handler
void Viewer3DPanel::OnResize(wxSizeEvent& event)
{
    Refresh();
}

// Renders the full 3D scene
void Viewer3DPanel::Render(const RenderSize& renderSize)
{
    if (!IsShownOnScreen()) {
        return;
    }
    SetCurrent(*m_glContext);

    static unsigned long long s_renderFrameId = 0;
    const int width = renderSize.width;
    const int height = renderSize.height;
    glstate::ApplyKnownBaseOnscreenState(width, height);
    const RenderSize viewportSize{width, height, "glstate::ApplyKnownBaseOnscreenState(framebuffer-px)"};

    const Viewer3DRenderStyle renderStyle = ResolveRenderStyleFromPreferences();
    ApplyViewer3DClearColorForStyle(renderStyle);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ApplyCameraMatrices(renderSize);
    const RenderSize projectionSize{width, height, "ApplyCameraMatrices::projection(framebuffer-px)"};

    ++s_renderFrameId;
    ValidateRenderSizeContract("Viewer3DPanel", s_renderFrameId, renderSize,
                               viewportSize, projectionSize, &renderSize);
    if (renderStyle == Viewer3DRenderStyle::Textured) {
        DrawTexturedStyleBackgroundGradient(ComputeGridPlaneHorizonNdcY());
        DrawTexturedGroundPlaneBackdrop();
    }

    m_controller.SetCameraMoving(m_cameraMoving);
    m_controller.RenderScene(IsWireframeRenderStyle(renderStyle),
                             ToSceneRenderMode(renderStyle));

    glFlush();
    ValidateGlStateAfterRender("Viewer3DPanel::Render", width, height);
}

bool Viewer3DPanel::ExportCurrentViewToPng() {
    if (!IsShownOnScreen()) {
        wxMessageBox("Cannot export while the 3D viewer is hidden.",
                     "Export image", wxOK | wxICON_WARNING, this);
        return false;
    }

    wxFileDialog saveDialog(this, "Export image",
                            wxEmptyString, "viewer3d_export.png",
                            "PNG files (*.png)|*.png",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDialog.ShowModal() != wxID_OK)
        return false;

    std::string path = saveDialog.GetPath().ToStdString();
    if (path.empty())
        return false;

    SetCurrent(*m_glContext);
    InitGL();
    if (!m_glInitialized) {
        wxMessageBox("OpenGL is not initialized yet.", "Export image",
                     wxOK | wxICON_ERROR, this);
        return false;
    }

    const RenderSize sourceRenderSize = ResolveRenderSize(this);
    int sourceWidth = sourceRenderSize.width;
    int sourceHeight = sourceRenderSize.height;
    const double exportFovYDegrees =
        ComputeExpandedFovYDegrees(sourceWidth, sourceHeight, kExportImageWidth,
                                   kExportImageHeight);

    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLuint depthRb = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &colorTex);
    glGenRenderbuffers(1, &depthRb);

    glstate::ScopedFramebufferViewportScissorState stateGuard;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, kExportImageWidth, kExportImageHeight);

    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kExportImageWidth, kExportImageHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorTex, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kExportImageWidth,
                          kExportImageHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depthRb);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteRenderbuffers(1, &depthRb);
        glDeleteTextures(1, &colorTex);
        glDeleteFramebuffers(1, &fbo);
        wxMessageBox("Failed to create an offscreen framebuffer for export.",
                     "Export image", wxOK | wxICON_ERROR, this);
        return false;
    }

    const Viewer3DRenderStyle renderStyle = ResolveRenderStyleFromPreferences();
    ApplyViewer3DClearColorForStyle(renderStyle);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const RenderSize exportRenderSize{kExportImageWidth, kExportImageHeight, "export-fbo"};
    ApplyCameraMatrices(exportRenderSize, exportFovYDegrees);
    if (renderStyle == Viewer3DRenderStyle::Textured) {
        DrawTexturedStyleBackgroundGradient(ComputeGridPlaneHorizonNdcY());
        DrawTexturedGroundPlaneBackdrop();
    }

    const bool previousCameraMoving = m_cameraMoving;
    m_controller.SetCameraMoving(false);
    m_controller.RenderScene(IsWireframeRenderStyle(renderStyle),
                             ToSceneRenderMode(renderStyle));
    glFlush();

    std::vector<unsigned char> rgba(static_cast<size_t>(kExportImageWidth) *
                                    static_cast<size_t>(kExportImageHeight) * 4u);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, kExportImageWidth, kExportImageHeight, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());

    glDeleteRenderbuffers(1, &depthRb);
    glDeleteTextures(1, &colorTex);
    glDeleteFramebuffers(1, &fbo);
    m_controller.SetCameraMoving(previousCameraMoving);

    wxImage image(kExportImageWidth, kExportImageHeight);
    unsigned char *rgb = image.GetData();
    for (int y = 0; y < kExportImageHeight; ++y) {
        const int srcY = kExportImageHeight - 1 - y;
        for (int x = 0; x < kExportImageWidth; ++x) {
            const size_t srcIndex =
                (static_cast<size_t>(srcY) * kExportImageWidth + x) * 4u;
            const size_t dstIndex =
                (static_cast<size_t>(y) * kExportImageWidth + x) * 3u;
            rgb[dstIndex + 0] = rgba[srcIndex + 0];
            rgb[dstIndex + 1] = rgba[srcIndex + 1];
            rgb[dstIndex + 2] = rgba[srcIndex + 2];
        }
    }

    if (!image.SaveFile(saveDialog.GetPath(), wxBITMAP_TYPE_PNG)) {
        wxMessageBox("Failed to save PNG image.", "Export image",
                     wxOK | wxICON_ERROR, this);
        return false;
    }

    return true;
}

void Viewer3DPanel::ApplyCameraMatrices(const RenderSize& renderSize, double fovYDegrees)
{
    const int width = renderSize.width;
    const int height = renderSize.height;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr double kNearPlane = 0.05;
    constexpr double kFarPlane = 2000.0;
    gluPerspective(fovYDegrees, static_cast<double>(width) / height,
                   kNearPlane, kFarPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    m_camera.Apply();
}


// Toggles the 3D measure tool mode and clears any pending measurement points.
void Viewer3DPanel::SetMeasureToolEnabled(bool enabled)
{
    m_measureToolEnabled = enabled;
    ResetMeasureState();
    if (MainWindow::Instance())
        MainWindow::Instance()->SyncViewportToolToggleState(enabled);
    SetCursor(enabled ? wxCursor(wxCURSOR_CROSS) : wxCursor(wxCURSOR_ARROW));
    Refresh();
}

// Resets the active and committed 3D measurement points while preserving enablement.
void Viewer3DPanel::ResetMeasureState()
{
    m_measureHasAnchor = false;
    m_measureAnchorUuid.clear();
    m_measureHasCommittedTarget = false;
    m_measureHasPreviewMousePos = false;
}

// Resolves the world-space center position of a selectable element.
std::optional<std::array<float, 3>> Viewer3DPanel::ResolveMeasureWorldFromUuid(
    HoverTargetTable target, const std::string& uuid) const
{
    const auto& scene = ConfigManager::Get().GetScene();
    auto extract = [&](const auto& map) -> std::optional<std::array<float, 3>> {
        auto it = map.find(uuid);
        if (it == map.end())
            return std::nullopt;
        return std::array<float, 3>{
            it->second.transform.o[0] / 1000.0f,
            it->second.transform.o[1] / 1000.0f,
            it->second.transform.o[2] / 1000.0f};
    };
    if (target == HoverTargetTable::Fixtures)
        return extract(scene.fixtures);
    if (target == HoverTargetTable::Trusses) {
        const auto& selectionContext =
            static_cast<const ISelectionContext&>(m_controller);
        if (const auto* trussBounds = selectionContext.FindTrussBounds(uuid)) {
            return std::array<float, 3>{
                0.5f * (trussBounds->min[0] + trussBounds->max[0]),
                0.5f * (trussBounds->min[1] + trussBounds->max[1]),
                0.5f * (trussBounds->min[2] + trussBounds->max[2])};
        }
        return extract(scene.trusses);
    }
    if (target == HoverTargetTable::SceneObjects)
        return extract(scene.sceneObjects);
    return std::nullopt;
}

// Draws the active 3D measurement line and distance text when two elements are fixed.
void Viewer3DPanel::DrawMeasureOverlay(const RenderSize& renderSize)
{
    if (!m_measureToolEnabled || !m_measureHasAnchor || !renderSize.IsValid())
        return;

    const auto anchorFramebuffer = ProjectWorldToFramebuffer(m_measureAnchorWorldMeters);
    if (!anchorFramebuffer)
        return;

    float endX = (*anchorFramebuffer)[0];
    float endY = (*anchorFramebuffer)[1];
    bool showDistanceLabel = false;
    std::array<float, 3> lineEndWorld = m_measureAnchorWorldMeters;
    if (m_measureHasCommittedTarget) {
        const auto targetFramebuffer =
            ProjectWorldToFramebuffer(m_measureCommittedTargetWorldMeters);
        if (!targetFramebuffer)
            return;
        endX = (*targetFramebuffer)[0];
        endY = (*targetFramebuffer)[1];
        lineEndWorld = m_measureCommittedTargetWorldMeters;
        showDistanceLabel = true;
    } else if (m_measureHasPreviewMousePos) {
        const wxPoint previewFramebuffer =
            ToFramebufferPoint(this, m_measurePreviewMousePos);
        endX = static_cast<float>(previewFramebuffer.x);
        endY = static_cast<float>(renderSize.height - previewFramebuffer.y);
    } else {
        return;
    }

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
        glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, static_cast<float>(renderSize.width), 0.0f,
            static_cast<float>(renderSize.height), -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    const float x0 = (*anchorFramebuffer)[0];
    const float y0 = (*anchorFramebuffer)[1];
    const float x1 = endX;
    const float y1 = endY;
    viewer_common::DrawMeasureOverlayStyle(x0, y0, x1, y1,
                                         Is2DDarkModeEnabled());

    float vx = x1 - x0;
    float vy = y1 - y0;
    const float len = std::sqrt(vx * vx + vy * vy);
    if (len < 1.0f) {
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        if (depthEnabled)
            glEnable(GL_DEPTH_TEST);
        return;
    }
    vx /= len;
    vy /= len;
    const float nx = -vy;
    const float ny = vx;
    const float offset = 14.0f;
    const float tx0 = x0 + nx * offset;
    const float ty0 = y0 + ny * offset;
    const float tx1 = x1 + nx * offset;
    const float ty1 = y1 + ny * offset;
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    if (depthEnabled)
        glEnable(GL_DEPTH_TEST);

    if (!showDistanceLabel)
        return;

    const float dx = lineEndWorld[0] - m_measureAnchorWorldMeters[0];
    const float dy = lineEndWorld[1] - m_measureAnchorWorldMeters[1];
    const float dz = lineEndWorld[2] - m_measureAnchorWorldMeters[2];
    const float distanceMeters = std::sqrt(dx * dx + dy * dy + dz * dz);
    const auto distanceUnitSystem =
        Units::ParseDistanceUnitSystem(ConfigManager::Get().GetValue("ui_distance_unit_system"));
    const std::string distanceText =
        Units::FormatDistanceFromMillimeters(static_cast<double>(distanceMeters) * 1000.0,
                                             distanceUnitSystem,
                                             Units::ValueFormatContext::Label) +
        " " + Units::DistanceUnitSuffix(distanceUnitSystem);

    const float labelX = ((tx0 + tx1) * 0.5f);
    const float labelY =
        static_cast<float>(renderSize.height) - ((ty0 + ty1) * 0.5f) - 10.0f;
    const float labelAngleDegrees = NormalizeMeasureLabelAngleDegrees(
        -std::atan2(ty1 - ty0, tx1 - tx0) * (180.0f / 3.14159265358979323846f));
    std::vector<OverlayTextLabel> labels{
        {labelX, labelY, distanceText, true, true, 20.0f, true, 0.95f, 0.1f,
         0.1f, labelAngleDegrees}};
    m_controller.DrawOverlayTextLabels(labels, Is2DDarkModeEnabled());
    if (MainWindow::Instance() && MainWindow::Instance()->GetStatusBar())
        MainWindow::Instance()->SetStatusText(wxString::FromUTF8("Measure: " + distanceText), 0);
}

// Handles mouse button press
void Viewer3DPanel::OnMouseDown(wxMouseEvent& event)
{
    ResetSelectionDragState();
    if (event.LeftDown() || event.MiddleDown() || event.RightDown())
    {
        if (event.LeftDown() && event.ControlDown()) {
            m_rectSelecting = true;
            m_rectSelectionAcrossAllTables = event.ShiftDown();
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

        if (event.LeftDown() && !event.ShiftDown() && !event.MiddleDown() &&
            !event.RightDown()) {
            if (PrepareSelectionDrag(event.GetPosition())) {
                m_lastMousePos = event.GetPosition();
                m_draggedSincePress = false;
                SetFocus();
                CaptureMouse();
                Refresh();
                return;
            }
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
        m_forceHoverQuery = true;
        Refresh();
        return;
    }

    if (event.LeftUp() && m_selectionDragArmed)
    {
        if (HasCapture())
            ReleaseMouse();
        if (m_selectionDragMoved) {
            FinalizeSelectionDrag();
            m_draggedSincePress = true;
        }
        ResetSelectionDragState();
        m_forceHoverQuery = true;
        Refresh();
        if (m_draggedSincePress) {
            m_draggedSincePress = false;
            return;
        }
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
        m_forceHoverQuery = true;
        Refresh();
    }

    if (event.LeftUp() && !m_draggedSincePress)
    {
        m_forceHoverQuery = true;
        const RenderSize renderSize = ResolveRenderSize(this);
        const int w = renderSize.width;
        const int h = renderSize.height;
        if (!IsShownOnScreen()) {
            return;
        }
        if (!renderSize.IsValid())
            return;
        SetCurrent(*m_glContext);
        std::string uuid;
        const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
        const HoverTargetTable activeTable = ResolveActiveHoverTargetTable();
        bool found = QueryHoverUuid(m_controller, activeTable, pickPos.x,
                                    pickPos.y, w, h, uuid, true);

        ConfigManager& cfg = ConfigManager::Get();
        if (m_measureToolEnabled) {
            if (!found) {
                found = QueryHoverUuid(m_controller, activeTable, pickPos.x,
                                       pickPos.y, w, h, uuid, false);
            }
            if (!found && !m_hoverUuid.empty()) {
                uuid = m_hoverUuid;
                found = true;
            }
            if (found) {
                const auto worldPos = ResolveMeasureWorldFromUuid(activeTable, uuid);
                if (worldPos) {
                    if (!m_measureHasAnchor || m_measureHasCommittedTarget) {
                        ResetMeasureState();
                        m_measureHasAnchor = true;
                        m_measureAnchorUuid = uuid;
                        m_measureAnchorWorldMeters = *worldPos;
                    } else {
                        m_measureHasCommittedTarget = true;
                        m_measureCommittedTargetWorldMeters = *worldPos;
                    }
                    Refresh();
                }
            } else {
                ResetMeasureState();
                Refresh();
            }
            return;
        }
        if (found)
        {
            bool additive = event.ShiftDown() || event.ControlDown();
            std::vector<std::string> selection;
            if (activeTable == HoverTargetTable::Fixtures &&
                FixtureTablePanel::Instance())
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
                FixtureTablePanel::Instance()->SelectByUuid(selection, false);
            }
            else if (activeTable == HoverTargetTable::Trusses &&
                     TrussTablePanel::Instance())
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
                TrussTablePanel::Instance()->SelectByUuid(selection, false);
            }
            else if (activeTable == HoverTargetTable::SceneObjects &&
                     SceneObjectTablePanel::Instance())
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
                SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
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

    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
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
        std::string hitUuid;
        const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
        if (!QueryHoverUuid(m_controller, HoverTargetTable::Fixtures, pickPos.x,
                            pickPos.y, w, h, hitUuid)) {
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
    constexpr int kRenderStyleWhiteId = wxID_HIGHEST + 1201;
    constexpr int kRenderStyleSketchId = wxID_HIGHEST + 1202;
    constexpr int kRenderStyleTexturedId = wxID_HIGHEST + 1203;
    constexpr int kRenderStyleWireframeId = wxID_HIGHEST + 1204;
    constexpr int kRenderStyleByDeviceTypeId = wxID_HIGHEST + 1205;
    constexpr int kRenderStyleByLayerId = wxID_HIGHEST + 1206;
    constexpr int kRenderStyleByUniverseId = wxID_HIGHEST + 1207;
    constexpr int kExportImagePngId = wxID_HIGHEST + 1208;

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
    renderStyleSubmenu->AppendRadioItem(kRenderStyleSketchId, "Sketch mode");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleTexturedId, "Textured");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWireframeId, "Wireframe");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWhiteId, "White");
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
        case Viewer3DRenderStyle::White:
            renderStyleSubmenu->Check(kRenderStyleWhiteId, true);
            break;
        case Viewer3DRenderStyle::WhiteModel:
            renderStyleSubmenu->Check(kRenderStyleSketchId, true);
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
    rootMenu.AppendSeparator();
    rootMenu.Append(kExportImagePngId, "Export image...");

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
    if (selectedId == kRenderStyleWhiteId) {
        applyRenderStyleSelection(Viewer3DRenderStyle::White);
        return;
    }
    if (selectedId == kRenderStyleSketchId) {
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
    if (selectedId == kExportImagePngId) {
        ExportCurrentViewToPng();
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
    m_rectSelectionAcrossAllTables = false;
    ResetSelectionDragState();
}

void Viewer3DPanel::ApplyRectangleSelection(const wxPoint& start,
                                            const wxPoint& end)
{
    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (w <= 0 || h <= 0 || !IsShownOnScreen()) {
        return;
    }

    SetCurrent(*m_glContext);

    ConfigManager& cfg = ConfigManager::Get();
    const wxPoint pickStart = ToFramebufferPoint(this, start);
    const wxPoint pickEnd = ToFramebufferPoint(this, end);
    if (m_rectSelectionAcrossAllTables)
    {
        const auto fixtures = m_controller.GetFixturesInScreenRect(
            pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
        const auto trusses = m_controller.GetTrussesInScreenRect(
            pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
        const auto sceneObjects = m_controller.GetSceneObjectsInScreenRect(
            pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);

        const bool selectionChanged =
            fixtures != cfg.GetSelectedFixtures() ||
            trusses != cfg.GetSelectedTrusses() ||
            sceneObjects != cfg.GetSelectedSceneObjects();
        if (selectionChanged)
            cfg.PushUndoState("global selection");

        cfg.SetSelectedFixtures(fixtures);
        cfg.SetSelectedTrusses(trusses);
        cfg.SetSelectedSceneObjects(sceneObjects);

        std::set<std::string> mergedSelection;
        mergedSelection.insert(fixtures.begin(), fixtures.end());
        mergedSelection.insert(trusses.begin(), trusses.end());
        mergedSelection.insert(sceneObjects.begin(), sceneObjects.end());
        SetSelectedFixtures(
            std::vector<std::string>(mergedSelection.begin(), mergedSelection.end()));

        if (FixtureTablePanel::Instance()) {
            if (fixtures.empty())
                FixtureTablePanel::Instance()->ClearSelection();
            else
                FixtureTablePanel::Instance()->SelectByUuid(fixtures, false);
        }
        if (TrussTablePanel::Instance()) {
            if (trusses.empty())
                TrussTablePanel::Instance()->ClearSelection();
            else
                TrussTablePanel::Instance()->SelectByUuid(trusses, false);
        }
        if (SceneObjectTablePanel::Instance()) {
            if (sceneObjects.empty())
                SceneObjectTablePanel::Instance()->ClearSelection();
            else
                SceneObjectTablePanel::Instance()->SelectByUuid(sceneObjects, false);
        }
        return;
    }

    if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
    {
        auto selection = m_controller.GetFixturesInScreenRect(
            pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
        if (selection != cfg.GetSelectedFixtures()) {
            cfg.PushUndoState("fixture selection");
            cfg.SetSelectedFixtures(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            FixtureTablePanel::Instance()->ClearSelection();
        else
            FixtureTablePanel::Instance()->SelectByUuid(selection, false);
    }
    else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
    {
        auto selection = m_controller.GetTrussesInScreenRect(
            pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
        if (selection != cfg.GetSelectedTrusses()) {
            cfg.PushUndoState("truss selection");
            cfg.SetSelectedTrusses(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            TrussTablePanel::Instance()->ClearSelection();
        else
            TrussTablePanel::Instance()->SelectByUuid(selection, false);
    }
    else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
    {
        auto selection =
            m_controller.GetSceneObjectsInScreenRect(
                pickStart.x, pickStart.y, pickEnd.x, pickEnd.y, w, h);
        if (selection != cfg.GetSelectedSceneObjects()) {
            cfg.PushUndoState("scene object selection");
            cfg.SetSelectedSceneObjects(selection);
        }
        SetSelectedFixtures(selection);
        if (selection.empty())
            SceneObjectTablePanel::Instance()->ClearSelection();
        else
            SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
    }
}

void Viewer3DPanel::DrawSelectionRectangle(int width, int height)
{
    int left = std::min(m_rectSelectStart.x, m_rectSelectEnd.x);
    int right = std::max(m_rectSelectStart.x, m_rectSelectEnd.x);
    int top = std::min(m_rectSelectStart.y, m_rectSelectEnd.y);
    int bottom = std::max(m_rectSelectStart.y, m_rectSelectEnd.y);
    const wxPoint physicalTopLeft = ToFramebufferPoint(this, wxPoint(left, top));
    const wxPoint physicalBottomRight =
        ToFramebufferPoint(this, wxPoint(right, bottom));

    float glLeft = static_cast<float>(physicalTopLeft.x);
    float glRight = static_cast<float>(physicalBottomRight.x);
    float glBottom = static_cast<float>(height - physicalBottomRight.y);
    float glTop = static_cast<float>(height - physicalTopLeft.y);

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

void Viewer3DPanel::ResetSelectionDragState()
{
    m_selectionDragArmed = false;
    m_selectionDragMoved = false;
    m_selectionDragUndoPushed = false;
    m_selectionDragPressTime = 0;
    m_selectionDragTarget = HoverTargetTable::None;
    m_dragSelectionUuids.clear();
    m_dragFixtureUuids.clear();
    m_dragTrussUuids.clear();
    m_dragSceneObjectUuids.clear();
    m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
}

std::array<float, 3> Viewer3DPanel::ComputeSelectionCenterMeters(
    const std::vector<std::string>& uuids, HoverTargetTable target) const
{
    std::array<float, 3> center{0.0f, 0.0f, 0.0f};
    if (uuids.empty())
        return center;

    const auto& scene = ConfigManager::Get().GetScene();
    size_t count = 0;
    auto accumulateCenter = [&](const auto& items) {
        for (const std::string& uuid : uuids) {
            auto it = items.find(uuid);
            if (it == items.end())
                continue;
            center[0] += it->second.transform.o[0] / 1000.0f;
            center[1] += it->second.transform.o[1] / 1000.0f;
            center[2] += it->second.transform.o[2] / 1000.0f;
            ++count;
        }
    };

    if (target == HoverTargetTable::Fixtures)
        accumulateCenter(scene.fixtures);
    else if (target == HoverTargetTable::Trusses)
        accumulateCenter(scene.trusses);
    else if (target == HoverTargetTable::SceneObjects)
        accumulateCenter(scene.sceneObjects);

    if (count == 0)
        return {0.0f, 0.0f, 0.0f};
    center[0] /= static_cast<float>(count);
    center[1] /= static_cast<float>(count);
    center[2] /= static_cast<float>(count);
    return center;
}

bool Viewer3DPanel::PrepareSelectionDrag(const wxPoint& mousePos)
{
    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid() || !IsShownOnScreen())
        return false;

    SetCurrent(*m_glContext);
    const wxPoint pickPos = ToFramebufferPoint(this, mousePos);
    const HoverTargetTable target = ResolveActiveHoverTargetTable();
    if (target == HoverTargetTable::None)
        return false;

    std::string uuid;
    if (!QueryDragLabelUuid(m_controller, target, pickPos.x, pickPos.y,
                            renderSize.width, renderSize.height, uuid)) {
        return false;
    }

    ConfigManager& cfg = ConfigManager::Get();
    std::vector<std::string> selection;
    if (target == HoverTargetTable::Fixtures)
        selection = cfg.GetSelectedFixtures();
    else if (target == HoverTargetTable::Trusses)
        selection = cfg.GetSelectedTrusses();
    else if (target == HoverTargetTable::SceneObjects)
        selection = cfg.GetSelectedSceneObjects();
    const auto hitSelectionIt = std::find(selection.begin(), selection.end(), uuid);
    const bool dragCurrentSelection = hitSelectionIt != selection.end();
    if (selection.size() > 1 || dragCurrentSelection)
        m_dragSelectionUuids = selection;
    else
        m_dragSelectionUuids = {uuid};

    m_selectionDragTarget = target;
    m_dragFixtureUuids.clear();
    m_dragTrussUuids.clear();
    m_dragSceneObjectUuids.clear();
    if (dragCurrentSelection) {
        m_dragFixtureUuids = cfg.GetSelectedFixtures();
        m_dragTrussUuids = cfg.GetSelectedTrusses();
        m_dragSceneObjectUuids = cfg.GetSelectedSceneObjects();
    } else {
        if (target == HoverTargetTable::Fixtures)
            m_dragFixtureUuids = {uuid};
        else if (target == HoverTargetTable::Trusses)
            m_dragTrussUuids = {uuid};
        else if (target == HoverTargetTable::SceneObjects)
            m_dragSceneObjectUuids = {uuid};
    }

    m_selectionDragAnchorMeters =
        ComputeSelectionCenterMeters(m_dragSelectionUuids, target);
    m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
    m_selectionDragArmed = true;
    m_selectionDragMoved = false;
    m_selectionDragUndoPushed = false;
    m_selectionDragPressTime = wxGetLocalTimeMillis();
    return true;
}

std::array<viewer3d::ProjectedAxis, 3>
Viewer3DPanel::BuildProjectedDragAxes(const RenderSize& renderSize) const
{
    std::array<viewer3d::ProjectedAxis, 3> axes{};
    if (!renderSize.IsValid())
        return axes;

    GLdouble modelview[16] = {};
    GLdouble projection[16] = {};
    GLint viewport[4] = {0, 0, 0, 0};
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLdouble originX = 0.0;
    GLdouble originY = 0.0;
    GLdouble originZ = 0.0;
    if (gluProject(m_selectionDragAnchorMeters[0], m_selectionDragAnchorMeters[1],
                   m_selectionDragAnchorMeters[2], modelview, projection, viewport,
                   &originX, &originY, &originZ) != GL_TRUE) {
        return axes;
    }

    const std::array<std::array<float, 3>, 3> axisWorldVectors{{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    }};

    for (size_t i = 0; i < axisWorldVectors.size(); ++i) {
        GLdouble tipX = 0.0;
        GLdouble tipY = 0.0;
        GLdouble tipZ = 0.0;
        const auto& worldAxis = axisWorldVectors[i];
        if (gluProject(m_selectionDragAnchorMeters[0] + worldAxis[0],
                       m_selectionDragAnchorMeters[1] + worldAxis[1],
                       m_selectionDragAnchorMeters[2] + worldAxis[2], modelview,
                       projection, viewport, &tipX, &tipY, &tipZ) != GL_TRUE) {
            continue;
        }

        auto& projectedAxis = axes[i];
        projectedAxis.screenDx = tipX - originX;
        projectedAxis.screenDy = tipY - originY;
        projectedAxis.pixelsPerMeter =
            std::sqrt(projectedAxis.screenDx * projectedAxis.screenDx +
                      projectedAxis.screenDy * projectedAxis.screenDy);
        projectedAxis.valid = projectedAxis.pixelsPerMeter > 1e-4;
    }

    return axes;
}

void Viewer3DPanel::ApplySelectionDragDelta(const std::array<float, 3>& deltaMeters)
{
    const float dxMm = deltaMeters[0] * 1000.0f;
    const float dyMm = deltaMeters[1] * 1000.0f;
    const float dzMm = deltaMeters[2] * 1000.0f;
    if (dxMm == 0.0f && dyMm == 0.0f && dzMm == 0.0f)
        return;

    ConfigManager& cfg = ConfigManager::Get();
    if (!m_selectionDragUndoPushed) {
        cfg.PushUndoState("move selection");
        m_selectionDragUndoPushed = true;
    }

    auto& scene = cfg.GetScene();
    auto applyDelta = [&](const auto& uuids, auto& items) {
        for (const std::string& uuid : uuids) {
            auto it = items.find(uuid);
            if (it == items.end())
                continue;
            it->second.transform.o[0] += dxMm;
            it->second.transform.o[1] += dyMm;
            it->second.transform.o[2] += dzMm;
        }
    };

    applyDelta(m_dragFixtureUuids, scene.fixtures);
    applyDelta(m_dragTrussUuids, scene.trusses);
    applyDelta(m_dragSceneObjectUuids, scene.sceneObjects);
    m_selectionDragAnchorMeters[0] += deltaMeters[0];
    m_selectionDragAnchorMeters[1] += deltaMeters[1];
    m_selectionDragAnchorMeters[2] += deltaMeters[2];
}

void Viewer3DPanel::FinalizeSelectionDrag()
{
    if (!m_selectionDragMoved)
        return;

    ConfigManager& cfg = ConfigManager::Get();
    if (!m_dragFixtureUuids.empty() && FixtureTablePanel::Instance()) {
        auto selection = cfg.GetSelectedFixtures();
        FixtureTablePanel::Instance()->ReloadData();
        FixtureTablePanel::Instance()->SelectByUuid(selection, false);
    }
    if (!m_dragTrussUuids.empty() && TrussTablePanel::Instance()) {
        auto selection = cfg.GetSelectedTrusses();
        TrussTablePanel::Instance()->ReloadData();
        TrussTablePanel::Instance()->SelectByUuid(selection, false);
    }
    if (!m_dragSceneObjectUuids.empty() && SceneObjectTablePanel::Instance()) {
        auto selection = cfg.GetSelectedSceneObjects();
        SceneObjectTablePanel::Instance()->ReloadData();
        SceneObjectTablePanel::Instance()->SelectByUuid(selection, false);
    }
    UpdateScene();
    if (Viewer2DPanel::Instance()) {
        Viewer2DPanel::Instance()->UpdateScene();
        Viewer2DPanel::Instance()->Refresh();
    }
}

void Viewer3DPanel::DrawSelectionDragGizmo(const RenderSize& renderSize)
{
    if (!m_selectionDragArmed || m_dragSelectionUuids.empty() ||
        !renderSize.IsValid()) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    constexpr double kNearPlane = 0.05;
    constexpr double kFarPlane = 2000.0;
    gluPerspective(kDefaultFovYDegrees,
                   static_cast<double>(renderSize.width) / renderSize.height,
                   kNearPlane, kFarPlane);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    m_camera.Apply();

    glLineWidth(2.0f);
    glDisable(GL_DEPTH_TEST);
    const float gizmoLength = std::max(0.4f, m_camera.GetDistance() * 0.08f);
    const bool xActive = m_selectionDragAxis == viewer3d::SelectionDragAxis::X;
    const bool yActive = m_selectionDragAxis == viewer3d::SelectionDragAxis::Y;
    const bool zActive = m_selectionDragAxis == viewer3d::SelectionDragAxis::Z;
    const std::array<std::array<float, 4>, 3> colors{{
        {xActive ? 1.0f : 0.95f, 0.25f, 0.25f, 1.0f},
        {0.25f, yActive ? 1.0f : 0.95f, 0.25f, 1.0f},
        {0.25f, 0.45f, zActive ? 1.0f : 0.95f, 1.0f},
    }};

    glBegin(GL_LINES);
    for (size_t i = 0; i < colors.size(); ++i) {
        const auto axis = AxisVectorFromSelectionDragAxis(
            i == 0 ? viewer3d::SelectionDragAxis::X
                   : i == 1 ? viewer3d::SelectionDragAxis::Y
                            : viewer3d::SelectionDragAxis::Z);
        glColor4f(colors[i][0], colors[i][1], colors[i][2], colors[i][3]);
        glVertex3f(m_selectionDragAnchorMeters[0], m_selectionDragAnchorMeters[1],
                   m_selectionDragAnchorMeters[2]);
        glVertex3f(m_selectionDragAnchorMeters[0] + axis[0] * gizmoLength,
                   m_selectionDragAnchorMeters[1] + axis[1] * gizmoLength,
                   m_selectionDragAnchorMeters[2] + axis[2] * gizmoLength);
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Handles mouse movement (orbit or pan)
void Viewer3DPanel::OnMouseMove(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();
    if (m_measureToolEnabled && m_measureHasAnchor && !m_measureHasCommittedTarget) {
        m_measurePreviewMousePos = pos;
        m_measureHasPreviewMousePos = true;
    }

    if (m_rectSelecting && event.Dragging())
    {
        m_rectSelectEnd = pos;
        m_draggedSincePress = true;
        Refresh();
        return;
    }

    if (m_selectionDragArmed && event.Dragging() && event.LeftIsDown())
    {
        if ((wxGetLocalTimeMillis() - m_selectionDragPressTime).ToLong() <
            kSelectionDragDelayMs) {
            m_lastMousePos = pos;
            return;
        }

        const int dx = pos.x - m_lastMousePos.x;
        const int dy = pos.y - m_lastMousePos.y;
        if (dx != 0 || dy != 0) {
            const RenderSize renderSize = ResolveRenderSize(this);
            if (renderSize.IsValid()) {
                SetCurrent(*m_glContext);
                ApplyCameraMatrices(renderSize);
                const auto projectedAxes = BuildProjectedDragAxes(renderSize);
                if (m_selectionDragAxis == viewer3d::SelectionDragAxis::None) {
                    m_selectionDragAxis = viewer3d::SelectDragAxisFromMouseDelta(
                        dx, -dy, projectedAxes);
                }

                const double axisDeltaMeters = viewer3d::ComputeDragMetersOnAxis(
                    dx, -dy, m_selectionDragAxis, projectedAxes);
                if (axisDeltaMeters != 0.0) {
                    const auto axisVector =
                        AxisVectorFromSelectionDragAxis(m_selectionDragAxis);
                    const std::array<float, 3> worldDelta{
                        axisVector[0] * static_cast<float>(axisDeltaMeters),
                        axisVector[1] * static_cast<float>(axisDeltaMeters),
                        axisVector[2] * static_cast<float>(axisDeltaMeters)};
                    ApplySelectionDragDelta(worldDelta);
                    m_selectionDragMoved = true;
                    m_draggedSincePress = true;
                }
            }
        }

        m_lastMousePos = pos;
        m_mouseMoved = true;
        m_forceHoverQuery = true;
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
            const float orbitPitchDirection = IsOrbitInversionEnabled() ? 1.0f : -1.0f;
            m_camera.targetYaw += dx * 0.5f;
            m_camera.targetPitch += orbitPitchDirection * static_cast<float>(dy) * 0.5f;
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
    m_forceHoverQuery = true;

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
    ArmZoomInteractionTimeout();

    Refresh();
}

void Viewer3DPanel::OnMouseDClick(wxMouseEvent& event)
{
    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (!renderSize.IsValid())
        return;
    SetCurrent(*m_glContext);
    std::string uuid;
    const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
    const bool cameraWasMoving = m_cameraMoving;
    if (cameraWasMoving)
        m_controller.SetCameraMoving(false);

    const bool sceneObjectsActive =
        SceneObjectTablePanel::Instance() &&
        SceneObjectTablePanel::Instance()->IsActivePage();

    const bool foundSceneObject = sceneObjectsActive &&
        QueryHoverUuid(m_controller, HoverTargetTable::SceneObjects,
                       pickPos.x, pickPos.y, w, h, uuid);

    bool found = foundSceneObject;
    if (!found)
        found = QueryHoverUuid(m_controller, HoverTargetTable::Fixtures,
                               pickPos.x, pickPos.y, w, h, uuid);

    if (cameraWasMoving)
        m_controller.SetCameraMoving(true);

    if (!found && !m_hoverUuid.empty())
        uuid = m_hoverUuid;
    if (uuid.empty())
        return;

    ConfigManager &cfg = ConfigManager::Get();
    if (foundSceneObject) {
        const bool edited = scene_object_primitives::EditPrimitiveObjectByUuid(
            this, cfg, uuid);
        if (!edited)
            return;

        if (SceneObjectTablePanel::Instance()) {
            SceneObjectTablePanel::Instance()->ReloadData();
            SceneObjectTablePanel::Instance()->SelectByUuid({uuid}, false);
        }

        UpdateScene();
        Refresh();
        if (Viewer2DPanel::Instance()) {
            Viewer2DPanel::Instance()->UpdateScene();
            Viewer2DPanel::Instance()->Refresh();
        }
        return;
    }

    auto& scene = cfg.GetScene();
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

// Handles keyboard shortcuts for viewport tools and camera actions.
void Viewer3DPanel::OnKeyDown(wxKeyEvent& event)
{
    if (!m_mouseInside && !HasFocus()) { event.Skip(); return; }
    if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
        event.Skip();
        return;
    }

    bool shift = event.ShiftDown();
    bool alt = event.AltDown();
    bool zoomTriggered = false;

    switch (event.GetKeyCode()) {
        case WXK_ESCAPE:
            if (m_measureToolEnabled) {
                SetMeasureToolEnabled(false);
                return;
            }
            event.Skip();
            return;
        case 'M':
        case 'm':
            SetMeasureToolEnabled(!m_measureToolEnabled);
            return;
        case WXK_LEFT:
            if (shift)
                m_camera.Pan(-0.1f, 0.0f);
            else if (alt) {
                m_camera.Zoom(-1.0f);
                zoomTriggered = true;
            } else
                m_camera.targetYaw += -5.0f;
            break;
        case WXK_RIGHT:
            if (shift)
                m_camera.Pan(0.1f, 0.0f);
            else if (alt) {
                m_camera.Zoom(1.0f);
                zoomTriggered = true;
            } else
                m_camera.targetYaw += 5.0f;
            break;
        case WXK_UP:
            if (shift)
                m_camera.Pan(0.0f, 0.1f);
            else if (alt) {
                m_camera.Zoom(-1.0f);
                zoomTriggered = true;
            } else
                m_camera.targetPitch = std::clamp(m_camera.targetPitch + 5.0f, -89.0f, 89.0f);
            break;
        case WXK_DOWN:
            if (shift)
                m_camera.Pan(0.0f, -0.1f);
            else if (alt) {
                m_camera.Zoom(1.0f);
                zoomTriggered = true;
            } else
                m_camera.targetPitch = std::clamp(m_camera.targetPitch - 5.0f, -89.0f, 89.0f);
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
        default:
            event.Skip();
            return;
    }

    m_controller.SetInteracting(true);
    m_isInteracting = true;
    m_cameraMoving = true;
    m_lastInteractionTime = std::chrono::steady_clock::now();
    if (zoomTriggered)
        ArmZoomInteractionTimeout();

    Refresh();
}

bool Viewer3DPanel::ResetCameraToIsometric() {
    m_camera.Reset();
    Refresh();
    return true;
}

bool Viewer3DPanel::FrameSceneToFit() {
    int width = 0;
    int height = 0;
    GetClientSize(&width, &height);
    if (!viewer3d::FrameSceneInCamera(m_controller, width, height, m_camera))
        return false;

    m_isInteracting = true;
    m_cameraMoving = true;
    m_lastInteractionTime = std::chrono::steady_clock::now();
    m_controller.SetInteracting(true);
    Refresh();
    return true;
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
            m_camera.SetOrientation(-90.0f, 0.0f);
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
    m_measureHasPreviewMousePos = false;
    m_hasHover = false;
    m_hoverUuid.clear();
    ++m_highlightRevision;
    m_highlightRefreshPending = true;
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

// Updates scene resources only when the 3D canvas is fully shown and its GL context can be safely activated.
void Viewer3DPanel::UpdateScene()
{
    ++m_sceneRevision;
    m_controller.MarkResourceSyncPending();

    if (ShouldPauseHeavyTasks() || m_cameraMoving)
        return;
    if (!IsShownOnScreen())
        return;

    if (!SetCurrent(*m_glContext))
        return;
    if (m_controller.ConsumeResourceSyncPending())
        m_controller.UpdateResourcesIfDirty();
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->UpdateScene();
}

void Viewer3DPanel::SetSelectedFixtures(const std::vector<std::string>& uuids)
{
    if (uuids == m_lastAppliedSelectionUuids)
        return;
    m_lastAppliedSelectionUuids = uuids;
    ++m_selectionRevision;
    m_selectionRefreshPending = true;
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
        if (m_shuttingDown)
            return;
        if (m_modalDialogActive) {
            std::this_thread::sleep_for(16ms);
            continue;
        }
        wxThreadEvent* evt = new wxThreadEvent(wxEVT_VIEWER_REFRESH);
        wxQueueEvent(this, evt);
        std::this_thread::sleep_for(16ms);
    }
}

void Viewer3DPanel::OnThreadRefresh(wxThreadEvent& event)
{
    if (m_shuttingDown || m_modalDialogActive || !m_glContext || IsBeingDeleted())
        return;

    const size_t cameraFingerprint = ComputeCameraFingerprint(m_camera);
    const bool cameraChanged =
        !m_hasLastThreadCameraFingerprint ||
        cameraFingerprint != m_lastThreadCameraFingerprint;
    if (cameraChanged) {
        m_lastThreadCameraFingerprint = cameraFingerprint;
        m_hasLastThreadCameraFingerprint = true;
    }

    const bool hasRelevantVisualChange =
        cameraChanged ||
        m_controller.IsResourceSyncPending() ||
        m_selectionRefreshPending ||
        m_highlightRefreshPending ||
        m_mouseMoved ||
        m_forceHoverQuery ||
        m_rectSelecting ||
        m_dragging ||
        m_isInteracting ||
        m_cameraMoving;
    if (!hasRelevantVisualChange)
        return;

    if (m_controller.IsResourceSyncPending() && !m_cameraMoving) {
        const auto now = std::chrono::steady_clock::now();
        const bool syncCadenceDue =
            (now - m_lastResourceSyncCheck) >= kResourceSyncInterval;
        if (syncCadenceDue) {
            if (!IsShown())
                return;
            SetCurrent(*m_glContext);
            m_lastResourceSyncCheck = now;
            if (m_controller.ConsumeResourceSyncPending())
                m_controller.UpdateResourcesIfDirty();
        }
    }

    Refresh();
}

void Viewer3DPanel::SetModalDialogActive(bool active)
{
    m_modalDialogActive = active;
}


bool Viewer3DPanel::ShouldPauseHeavyTasks()
{
    const auto now = std::chrono::steady_clock::now();
    const bool interactionGraceActive =
        (now - m_lastInteractionTime) < kPauseDelay;

    if (interactionGraceActive && (m_isInteracting || m_cameraMoving))
        return true;

    if (!m_isInteracting && !m_cameraMoving)
        return false;

    m_isInteracting = false;
    m_cameraMoving = false;
    m_controller.SetInteracting(false);
    m_controller.SetCameraMoving(false);

    const bool fastInteractionMode = IsFastInteractionModeEnabled();

    if (fastInteractionMode) {
        m_controller.MarkResourceSyncPending();
        m_mouseMoved = true;
    }

    return false;
}

void Viewer3DPanel::ArmZoomInteractionTimeout()
{
    m_zoomInteractionTimer.StartOnce(kZoomInteractionTimeoutMs);
}

void Viewer3DPanel::OnZoomInteractionTimeout(wxTimerEvent& event)
{
    (void)event;

    if (m_dragging || m_rectSelecting)
        return;

    m_isInteracting = false;
    m_cameraMoving = false;
    m_controller.SetInteracting(false);
    m_controller.SetCameraMoving(false);
    m_controller.MarkResourceSyncPending();
    m_mouseMoved = true;
    m_forceHoverQuery = true;
    Refresh();
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
