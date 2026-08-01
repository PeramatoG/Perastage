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
#include "gl_context_utils.h"
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
#include "../core/pixel_coordinate_math.h"
#include "mainwindow.h"
#include "../gui/mainwindow/ids/tools_ids.h"
#include "editable_focus_utils.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "hoisttablepanel.h"
#include "sceneobjecttablepanel.h"
#include "scene_object_primitive_editing.h"
#include "../gui/selection_origin_token.h"
#include "configmanager.h"
#include "continuous_placement_scene.h"
#include "selection_movement_settings.h"
#include "interaction/context_menu_model.h"
#include "../viewport_interaction_scope.h"
#include "magnet_snap.h"
#include "scene_grouping.h"
#include "fixturepatchdialog.h"
#include "viewer2dpanel.h"
#include "viewer3dviewfit.h"
#include "viewer3d_render_style.h"
#include "gl_state_guard.h"
#include "navigation_diagnostics.h"
#include "ui_render_size.h"
#include "../viewer_common/gl_canvas_config.h"
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
#include <utility>
#include <limits>

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
constexpr int kSelectionDragStartThresholdPx = 3;
constexpr int kExportImageWidth = 1920;
constexpr int kExportImageHeight = 1080;
constexpr double kDefaultFovYDegrees = 45.0;

struct ScopedBoolFlag {
    // Sets a boolean flag for a scope and restores it when the scope exits.
    explicit ScopedBoolFlag(bool& flagRef) : flag(flagRef) { flag = true; }
    // Clears the scoped boolean flag during unwinding or normal return.
    ~ScopedBoolFlag() { flag = false; }

    bool& flag;
};

void ValidateGlStateAfterRender(const char* stage, int expectedWidth,
                                int expectedHeight) {
    GLint framebuffer = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);

    const bool validFramebuffer = framebuffer == 0;
    const bool validViewport = viewport[0] == 0 && viewport[1] == 0 &&
                               viewport[2] == expectedWidth &&
                               viewport[3] == expectedHeight;
    if (!validFramebuffer || !validViewport) {
        wxLogTrace("viewer3d_gl_state",
                 "%s left unexpected GL state (fbo=%d viewport=%d,%d,%d,%d "
                 "expected=0,0,%d,%d).",
                   stage, framebuffer, viewport[0], viewport[1], viewport[2],
                   viewport[3], expectedWidth, expectedHeight);
    }
    wxASSERT_MSG(validFramebuffer,
                 "Unexpected non-default framebuffer after 3D render.");
    wxASSERT_MSG(validViewport, "Unexpected viewport after 3D render.");
}

// Resets transient OpenGL state before drawing a new 3D frame.
void ApplyKnownViewer3DFrameState(int width, int height) {
    glstate::ApplyKnownBaseOnscreenState(width, height);

    if (GLEW_VERSION_2_0)
        glUseProgram(0);
    if (GLEW_VERSION_3_0 || GLEW_ARB_vertex_array_object)
        glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_LINE_SMOOTH);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);
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

struct HoverTableHighlights {
    std::vector<std::string> fixtures;
    std::vector<std::string> trusses;
    std::vector<std::string> supports;
    std::vector<std::string> sceneObjects;
};

// Appends a UUID to a sequence when it is not already present.
void AppendUniqueUuid(std::vector<std::string>& uuids, const std::string& uuid) {
    if (uuid.empty())
        return;
    if (std::find(uuids.begin(), uuids.end(), uuid) == uuids.end())
        uuids.push_back(uuid);
}

// Splits one UUID into the table bucket that owns it.
void AppendUuidToTableHighlights(const MvrScene& scene, const std::string& uuid,
                                 HoverTableHighlights& highlights) {
    if (scene.fixtures.find(uuid) != scene.fixtures.end())
        AppendUniqueUuid(highlights.fixtures, uuid);
    else if (scene.trusses.find(uuid) != scene.trusses.end())
        AppendUniqueUuid(highlights.trusses, uuid);
    else if (scene.supports.find(uuid) != scene.supports.end())
        AppendUniqueUuid(highlights.supports, uuid);
    else if (scene.sceneObjects.find(uuid) != scene.sceneObjects.end())
        AppendUniqueUuid(highlights.sceneObjects, uuid);
}

// Splits related hover UUIDs by table ownership for synchronized table highlights.
HoverTableHighlights BuildHoverTableHighlights(const MvrScene& scene,
                                               const std::string& hoverUuid) {
    HoverTableHighlights highlights;
    for (const auto& uuid :
         scene_grouping::ExpandHoverForGroupHighlights(scene, hoverUuid)) {
        AppendUuidToTableHighlights(scene, uuid, highlights);
    }
    return highlights;
}

// Builds the cross-table selection represented by a clicked scene item.
HoverTableHighlights BuildClickSelectionHighlights(
    const MvrScene& scene, const std::string& uuid) {
    HoverTableHighlights highlights;
    AppendUuidToTableHighlights(scene, uuid, highlights);
    for (const auto& relatedUuid :
         scene_grouping::ExpandHoverForGroupHighlights(scene, uuid)) {
        AppendUuidToTableHighlights(scene, relatedUuid, highlights);
    }
    return highlights;
}

// Returns true when every grouped click UUID is already selected.
bool ContainsAllUuids(const std::vector<std::string>& selection,
                      const std::vector<std::string>& clickedUuids) {
    return std::all_of(clickedUuids.begin(), clickedUuids.end(),
                       [&](const std::string& uuid) {
                           return std::find(selection.begin(), selection.end(),
                                            uuid) != selection.end();
                       });
}

// Adds missing clicked UUIDs to an existing selection while preserving order.
std::vector<std::string> AddClickedUuids(std::vector<std::string> selection,
                                         const std::vector<std::string>& clickedUuids) {
    for (const auto& uuid : clickedUuids) {
        if (std::find(selection.begin(), selection.end(), uuid) == selection.end())
            selection.push_back(uuid);
    }
    return selection;
}

// Adds or removes grouped click UUIDs from an existing additive selection.
std::vector<std::string> ToggleClickedUuids(std::vector<std::string> selection,
                                            const std::vector<std::string>& clickedUuids) {
    if (clickedUuids.empty())
        return selection;
    const bool removeClickedUuids = ContainsAllUuids(selection, clickedUuids);
    for (const auto& uuid : clickedUuids) {
        auto it = std::find(selection.begin(), selection.end(), uuid);
        if (removeClickedUuids) {
            if (it != selection.end())
                selection.erase(it);
        } else if (it == selection.end()) {
            selection.push_back(uuid);
        }
    }
    return selection;
}

// Updates one typed selection according to the additive click mode.
std::vector<std::string> ResolveClickedSelection(
    const std::vector<std::string>& currentSelection,
    const std::vector<std::string>& clickedUuids, bool additive, bool addOnly) {
    if (!additive)
        return clickedUuids;
    if (addOnly)
        return AddClickedUuids(currentSelection, clickedUuids);
    return ToggleClickedUuids(currentSelection, clickedUuids);
}

// Flattens typed table selections into one viewer UUID list.
std::vector<std::string> FlattenSelectionHighlights(
    const HoverTableHighlights& selection) {
    std::set<std::string> mergedSelection;
    mergedSelection.insert(selection.fixtures.begin(), selection.fixtures.end());
    mergedSelection.insert(selection.trusses.begin(), selection.trusses.end());
    mergedSelection.insert(selection.supports.begin(), selection.supports.end());
    mergedSelection.insert(selection.sceneObjects.begin(),
                           selection.sceneObjects.end());
    return std::vector<std::string>(mergedSelection.begin(), mergedSelection.end());
}

// Builds a typed selection snapshot from the current configuration state.
HoverTableHighlights BuildCurrentSelectionHighlights(const ConfigManager& cfg) {
    HoverTableHighlights selection;
    selection.fixtures = cfg.GetSelectedFixtures();
    selection.trusses = cfg.GetSelectedTrusses();
    selection.supports = cfg.GetSelectedSupports();
    selection.sceneObjects = cfg.GetSelectedSceneObjects();
    return selection;
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
    const auto converted = pixel_coordinates::LogicalToFramebuffer(
        {static_cast<double>(logicalPoint.x),
         static_cast<double>(logicalPoint.y)}, contentScale);
    if (!converted)
        return logicalPoint;
    return wxPoint((*converted)[0], (*converted)[1]);
}

// Converts a logical point while preserving conversion failure for placement.
std::optional<wxPoint> TryToFramebufferPoint(wxWindow* window,
                                             const wxPoint& logicalPoint) {
    if (window == nullptr)
        return std::nullopt;
    const auto converted = pixel_coordinates::LogicalToFramebuffer(
        {static_cast<double>(logicalPoint.x),
         static_cast<double>(logicalPoint.y)},
        static_cast<double>(window->GetContentScaleFactor()));
    if (!converted)
        return std::nullopt;
    return wxPoint((*converted)[0], (*converted)[1]);
}

// Converts framebuffer pixel coordinates back to logical client-space pixels.
wxPoint FromFramebufferPoint(wxWindow* window, const wxPoint& framebufferPoint) {
    if (window == nullptr)
        return framebufferPoint;
    const double contentScale =
        static_cast<double>(window->GetContentScaleFactor());
    const auto converted = pixel_coordinates::FramebufferToLogical(
        {static_cast<double>(framebufferPoint.x),
         static_cast<double>(framebufferPoint.y)}, contentScale);
    if (!converted)
        return framebufferPoint;
    return wxPoint((*converted)[0], (*converted)[1]);
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
    if (sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 ||
        targetHeight <= 0)
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
    const double expandedFovDegrees =
        expandedHalfFovRadians * 2.0 * 180.0 / kPi;
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
      const double angle =
          (static_cast<double>(i) / static_cast<double>(kSamples)) * 2.0 *
          3.14159265358979323846;
        const double x = std::cos(angle) * kProbeRadius;
        const double y = std::sin(angle) * kProbeRadius;
        GLdouble sx = 0.0, sy = 0.0, sz = 0.0;
      if (gluProject(x, y, 0.0, model, projection, viewport, &sx, &sy, &sz) ==
              GL_TRUE &&
            sz >= 0.0 && sz <= 1.0) {
            topMostY = std::max(topMostY, sy);
            found = true;
        }
    }

    if (!found)
        return -0.05f;

    const double ndcY =
        (topMostY / static_cast<double>(viewport[3])) * 2.0 - 1.0;
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
      glColor3f(bands[i + 1].second[0], bands[i + 1].second[1],
                bands[i + 1].second[2]);
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
      const float angle =
          (static_cast<float>(i) / static_cast<float>(kSegments)) * 2.0f * kPi;
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

// Applies cross-table scene selections to config, viewport, and table panels.
void ApplyObjectSelectionToUi(const HoverTableHighlights& selection,
                              Viewer3DPanel* panel)
{
    ConfigManager& cfg = ConfigManager::Get();
    const bool selectionChanged =
        selection.fixtures != cfg.GetSelectedFixtures() ||
        selection.trusses != cfg.GetSelectedTrusses() ||
        selection.supports != cfg.GetSelectedSupports() ||
        selection.sceneObjects != cfg.GetSelectedSceneObjects();
    if (selectionChanged) {
        cfg.PushUndoState("object selection");
        cfg.SetSelectedFixtures(selection.fixtures);
        cfg.SetSelectedTrusses(selection.trusses);
        cfg.SetSelectedSupports(selection.supports);
        cfg.SetSelectedSceneObjects(selection.sceneObjects);
    }

    if (panel)
        panel->SetSelectedFixtures(FlattenSelectionHighlights(selection));

    selection::ScopedOrigin selectionOrigin(selection::Origin::Viewer3D);
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->SelectByUuid(selection.fixtures, false);
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->SelectByUuid(selection.trusses, false);
    if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->SelectByUuid(selection.supports, false);
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->SelectByUuid(selection.sceneObjects,
                                                       false);
}

// Resolves which table should receive hover and pick queries.
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

// Converts panel hover target types into controller picking target types.
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

// Reports whether viewport interactions should ignore the active table scope.
bool IsCrossTableViewportActionsEnabled()
{
    return ConfigManager::Get().GetValue(
               viewport_interaction_scope::kCrossTableActionsConfigKey) == "1";
}

// Runs a guarded hover pick query for the requested target table.
bool QueryHoverUuid(Viewer3DController& controller,
                    Viewer3DPanel::HoverTargetTable target,
                    int mouseX, int mouseY, int width, int height,
                    std::string& outUuid,
                    bool confirmDepth = false)
{
    if (target == Viewer3DPanel::HoverTargetTable::None)
        return false;
    if (width <= 0 || height <= 0) {
        viewer3d::diagnostics::Log("Hover picking skipped because viewport dimensions are invalid.");
        wxASSERT_MSG(false, "Invalid viewport size passed to hover picking.");
        outUuid.clear();
        return false;
    }
    viewer3d::diagnostics::Logf(
        "Hover picking start target=%d mouse=(%d,%d) size=(%d,%d)",
        static_cast<int>(target), mouseX, mouseY, width, height);
    const bool found = controller.GetHoverUuidAt(mouseX, mouseY, width, height,
                                                 ToHoverPickTarget(target),
                                                 outUuid, confirmDepth);
    viewer3d::diagnostics::Logf("Hover picking end found=%d uuid=%s",
                                found ? 1 : 0, outUuid.c_str());
    if (!found)
        outUuid.clear();
    return found;
}

  // Runs hover pick queries across all selectable object tables in priority
  // order.
  bool QueryHoverUuidAcrossTables(
      Viewer3DController & controller, int mouseX, int mouseY, int width,
      int height, std::string &outUuid,
      Viewer3DPanel::HoverTargetTable &outTarget, bool confirmDepth = false) {
    const Viewer3DPanel::HoverTargetTable targets[] = {
        Viewer3DPanel::HoverTargetTable::Fixtures,
        Viewer3DPanel::HoverTargetTable::Trusses,
        Viewer3DPanel::HoverTargetTable::SceneObjects};
    for (const auto target : targets) {
        if (QueryHoverUuid(controller, target, mouseX, mouseY, width, height,
                           outUuid, confirmDepth)) {
            outTarget = target;
            return true;
        }
    }
    outTarget = Viewer3DPanel::HoverTargetTable::None;
    outUuid.clear();
    return false;
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

// Runs a label pick query used to arm selection dragging.
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

// Returns the world-space unit vector for a selection drag axis.
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

// Computes two perpendicular unit vectors around an axis for cone rendering.
std::pair<std::array<float, 3>, std::array<float, 3>> BuildAxisConeBasis(
    const std::array<float, 3>& axis) {
    const std::array<float, 3> reference =
        std::abs(axis[2]) < 0.9f ? std::array<float, 3>{0.0f, 0.0f, 1.0f}
                                : std::array<float, 3>{0.0f, 1.0f, 0.0f};
    std::array<float, 3> u{
        axis[1] * reference[2] - axis[2] * reference[1],
        axis[2] * reference[0] - axis[0] * reference[2],
        axis[0] * reference[1] - axis[1] * reference[0],
    };
    const float uLength = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (uLength > 1e-5f) {
        u[0] /= uLength;
        u[1] /= uLength;
        u[2] /= uLength;
    }
    const std::array<float, 3> v{
        axis[1] * u[2] - axis[2] * u[1],
        axis[2] * u[0] - axis[0] * u[2],
        axis[0] * u[1] - axis[1] * u[0],
    };
    return {u, v};
}

// Draws a filled cone arrowhead aligned to a selection drag axis.
void DrawSelectionDragArrowhead(const std::array<float, 3>& origin,
                                const std::array<float, 3>& axis,
                                float length, float radius) {
    constexpr int kSegments = 24;
    constexpr float kPi = 3.14159265358979323846f;
    const auto [u, v] = BuildAxisConeBasis(axis);
    const std::array<float, 3> tip{
        origin[0] + axis[0] * length,
        origin[1] + axis[1] * length,
        origin[2] + axis[2] * length,
    };
    const std::array<float, 3> base{
        tip[0] - axis[0] * length,
        tip[1] - axis[1] * length,
        tip[2] - axis[2] * length,
    };

    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(tip[0], tip[1], tip[2]);
    for (int i = 0; i <= kSegments; ++i) {
      const float angle =
          (static_cast<float>(i) / static_cast<float>(kSegments)) * 2.0f * kPi;
        const float ca = std::cos(angle);
        const float sa = std::sin(angle);
        glVertex3f(base[0] + (u[0] * ca + v[0] * sa) * radius,
                   base[1] + (u[1] * ca + v[1] * sa) * radius,
                   base[2] + (u[2] * ca + v[2] * sa) * radius);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(base[0], base[1], base[2]);
    for (int i = kSegments; i >= 0; --i) {
      const float angle =
          (static_cast<float>(i) / static_cast<float>(kSegments)) * 2.0f * kPi;
        const float ca = std::cos(angle);
        const float sa = std::sin(angle);
        glVertex3f(base[0] + (u[0] * ca + v[0] * sa) * radius,
                   base[1] + (u[1] * ca + v[1] * sa) * radius,
                   base[2] + (u[2] * ca + v[2] * sa) * radius);
    }
    glEnd();
}

  int GetRequestedViewerAASamples() {
    const int quality =
        std::clamp(static_cast<int>(std::lround(
                       ConfigManager::Get().GetFloat("viewer3d_aa_quality"))),
                   0, 2);
    switch (quality) {
    case 2:
        return 4;
    case 1:
        return 2;
    default:
        return 0;
    }
}

}

// Creates the 3D panel, OpenGL context, and refresh worker.
Viewer3DPanel::Viewer3DPanel(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, gl_lifecycle::GetMsaaCanvasAttributes(GetRequestedViewerAASamples()),
                 wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
    m_glContext(new wxGLContext(this))
{
    m_magnetEnabled = ConfigManager::Get().GetValue(
                          magnet_snap::kMagnetEnabledConfigKey) == "1";
    m_leftDragSelectionMovementEnabled =
        selection_movement_settings::IsLeftDragSelectionMovementEnabled(
            ConfigManager::Get());
    m_axisConstrainedMovementEnabled =
        selection_movement_settings::IsAxisConstrainedMovementEnabled(
            ConfigManager::Get());
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  Bind(wxEVT_THREAD, &Viewer3DPanel::OnThreadRefresh, this,
       wxEVT_VIEWER_REFRESH);
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
    m_refreshThread = std::thread(&Viewer3DPanel::RefreshLoop, this);
}

// Stops refresh work and releases OpenGL-owned panel resources.
Viewer3DPanel::~Viewer3DPanel() {
    m_shuttingDown = true;
  Unbind(wxEVT_THREAD, &Viewer3DPanel::OnThreadRefresh, this,
         wxEVT_VIEWER_REFRESH);
    Unbind(wxEVT_TIMER, &Viewer3DPanel::OnZoomInteractionTimeout, this,
           m_zoomInteractionTimer.GetId());
    m_zoomInteractionTimer.Stop();
    DeletePendingEvents();
    if (HasCapture())
        ReleaseMouse();
    StopRefreshThread();
    delete m_glContext;
    m_glContext = nullptr;
    SetInstance(nullptr);
}

// Binds the OpenGL context for interaction work when the panel can safely pick.
bool Viewer3DPanel::TryBindGlContextForInteraction(const char* caller)
{
    const char* callerName = caller != nullptr ? caller : "unknown";
    const char* reason = nullptr;
    if (!m_glContext)
        reason = "missing context";
    else if (!IsShownOnScreen())
        reason = "panel hidden";
    else if (m_modalDialogActive)
        reason = "modal dialog active";
    else if (IsBeingDeleted())
        reason = "panel being deleted";

    if (reason != nullptr) {
        viewer3d::diagnostics::Logf("%s skipped GL interaction binding: %s.",
                                    callerName, reason);
        return false;
    }

    if (!gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer3DPanel",
                                      callerName)) {
    viewer3d::diagnostics::Logf(
        "%s skipped GL interaction binding: SetCurrent failed.", callerName);
        return false;
    }

    return true;
}

// Prepares initialized OpenGL state before scene resource synchronization.
bool Viewer3DPanel::PrepareGlResourceSync(const char* caller)
{
    if (!TryBindGlContextForInteraction(caller))
        return false;
    if (!InitGL()) {
        const char* callerName = caller != nullptr ? caller : "unknown";
        viewer3d::diagnostics::Logf("%s skipped resource sync because GL initialization is not complete.",
                                    callerName);
        return false;
    }
    return true;
}

// Stops and joins the background refresh signal thread.
void Viewer3DPanel::StopRefreshThread() {
    m_threadRunning = false;
    if (m_refreshThread.joinable())
        m_refreshThread.join();
}

// Initializes 3D OpenGL state only after centralized GLEW/context validation.
bool Viewer3DPanel::InitGL() {
    if (!IsShownOnScreen()) {
        return false;
    }
    if (!m_glInitialized) {
        const GLEWInitResult initResult =
            gl_lifecycle::InitializeGlew(*this, *m_glContext, "Viewer3DPanel");
        if (!initResult.success) {
            wxLogError("%s", initResult.message);
            return false;
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
    return m_glInitialized;
}

// Paint event handler
void Viewer3DPanel::OnPaint(wxPaintEvent &event) {
    (void)event;
    wxPaintDC dc(this);
    if (m_paintInProgress) {
    viewer3d::diagnostics::Log(
        "Viewer3DPanel::OnPaint skipped re-entrant paint.");
        wxASSERT_MSG(false, "Re-entrant 3D paint event detected.");
        return;
    }
    ScopedBoolFlag paintScope(m_paintInProgress);
    viewer3d::diagnostics::Log("OpenGL render frame start.");
    if (!IsShownOnScreen() || m_modalDialogActive) {
    viewer3d::diagnostics::Log("OpenGL render frame skipped because the panel "
                               "is hidden or modal-blocked.");
        return;
    }
    if (!m_glContext) {
    viewer3d::diagnostics::Log(
        "OpenGL render frame skipped because the GL context is null.");
        wxASSERT_MSG(false, "Viewer3DPanel has no GL context during paint.");
        return;
    }
    if (!InitGL() || !m_glInitialized) {
    viewer3d::diagnostics::Log("OpenGL render frame skipped because GL "
                               "initialization is not complete.");
        return;
    }

    m_controller.ResetDebugPerFrameCounters();

    const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
    if (m_controller.IsResourceSyncPending() && !pauseHeavyTasks &&
        !m_cameraMoving && m_controller.ConsumeResourceSyncPending()) {
        m_controller.UpdateResourcesIfDirty();
    }

    const bool highlightRefreshPendingAtFrameStart = m_highlightRefreshPending;
    const bool highlightOnlyRefresh = false;
    const size_t cameraFingerprint = ComputeCameraFingerprint(m_camera);
    const auto hiddenLayers = ConfigManager::Get().GetHiddenLayers();

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
  const float dt =
      std::chrono::duration<float>(now - s_lastCameraUpdate).count();
    s_lastCameraUpdate = now;
    const auto cameraStateBeforeUpdate = m_camera.GetStateForDiagnostics();
    m_camera.Update(dt);
    if (cameraStateBeforeUpdate != m_camera.GetStateForDiagnostics()) {
        m_placementViewRevision.Invalidate();
    }
  wxASSERT_MSG(m_camera.IsValid(),
               "3D camera state became invalid during paint update.");

    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid()) {
        return;
    }
    if (m_continuousPlacementActive && m_hasLastMousePos &&
        m_placementViewRevision.NeedsAlignment())
        AlignContinuousElementToPointer(m_lastMousePos);

    wxLogTrace("viewer3d_perf", "Viewer3DPanel frame render mode=full");
    const auto fullRenderStart = std::chrono::steady_clock::now();
    Render(renderSize);
    const auto fullRenderElapsedMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - fullRenderStart)
            .count();
    m_fullRenderMsAccumInCurrentWindow += fullRenderElapsedMs;
    ++m_fullRenderSamplesInCurrentWindow;

    // Ensure the OpenGL context is current before drawing overlays.
    if (!gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer3DPanel",
                                      "OnPaint overlays")) {
        return;
    }

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
  const bool skipLabelWork =
      m_cameraMoving &&
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

  const HoverTargetTable activeTable = IsCrossTableViewportActionsEnabled()
                                           ? HoverTargetTable::Fixtures
                                           : ResolveActiveHoverTargetTable();

    if (activeTable != m_lastHoverTargetTable) {
        m_forceHoverQuery = true;
        m_lastHoverTargetTable = activeTable;
    }

    const wxPoint pickPos = ToFramebufferPoint(this, m_lastMousePos);
    const HoverQueryState currentHoverQueryState{
      pickPos, m_cameraRevision, m_hiddenLayersRevision, m_sceneRevision};
  const bool hoverStateChanged =
      !m_hasLastHoverQueryState ||
      currentHoverQueryState.mouseFramebufferPos !=
          m_lastHoverQueryState.mouseFramebufferPos ||
      currentHoverQueryState.cameraRevision !=
          m_lastHoverQueryState.cameraRevision ||
      currentHoverQueryState.hiddenLayersRevision !=
          m_lastHoverQueryState.hiddenLayersRevision ||
      currentHoverQueryState.sceneRevision !=
          m_lastHoverQueryState.sceneRevision;
    const bool shouldUpdateHoverQuery =
        m_forceHoverQuery || m_mouseMoved || hoverStateChanged || !m_hasHover;
    const auto nowForHover = std::chrono::steady_clock::now();
  const bool hoverCadenceDue =
      m_forceHoverQuery ||
        (nowForHover - m_lastHoverQueryTime) >= kHoverQueryInterval;
    const bool hoverQueriesPausedForInteraction =
        m_cameraMoving || m_isInteracting || m_dragging || m_selectionDragArmed;
    if (hoverQueriesPausedForInteraction && shouldUpdateHoverQuery)
    viewer3d::diagnostics::Log(
        "Hover picking paused during active interaction.");
  const bool shouldRunHoverQuery = !hoverQueriesPausedForInteraction &&
        (!skipLabelWork || m_forceHoverQuery) &&
        shouldUpdateHoverQuery && hoverCadenceDue &&
        activeTable != HoverTargetTable::None &&
        (m_forceHoverQuery || hoverStateChanged);

    if (shouldRunHoverQuery) {
        const auto hoverQueryStart = std::chrono::steady_clock::now();
        HoverTargetTable pickedTable = activeTable;
        if (IsCrossTableViewportActionsEnabled())
      found = QueryHoverUuidAcrossTables(m_controller, pickPos.x, pickPos.y, w,
                                         h, newUuid, pickedTable);
        else
      found = QueryHoverUuid(m_controller, activeTable, pickPos.x, pickPos.y, w,
                             h, newUuid);
        const auto hoverQueryElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - hoverQueryStart)
                .count();
        m_hoverQueryMsAccumInCurrentWindow += hoverQueryElapsedMs;
        ++m_hoverQuerySamplesInCurrentWindow;
        hoverQueryRan = true;
        if (found) {
            if (!skipLabelWork && activeTable != HoverTargetTable::Fixtures) {
                wxString tooltipLabel;
                wxPoint tooltipPos;
                if (activeTable == HoverTargetTable::Trusses) {
          m_controller.GetTrussLabelAt(pickPos.x, pickPos.y, w, h, tooltipLabel,
                                       tooltipPos, nullptr);
                } else if (activeTable == HoverTargetTable::SceneObjects) {
          m_controller.GetSceneObjectLabelAt(pickPos.x, pickPos.y, w, h,
                                             tooltipLabel, tooltipPos, nullptr);
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
  } else if (hoverQueryRan && !skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
    } else if (skipLabelWork) {
        m_hasHover = false;
        m_hoverUuid.clear();
        m_hoverText.clear();
    }

    bool highlightChanged = false;
    if (oldHoverUuid != m_hoverUuid || oldHasHover != m_hasHover) {
        const auto highlightUpdateStart = std::chrono::steady_clock::now();
        highlightChanged = true;
        SynchronizeHoverHighlight();
        const auto highlightUpdateElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - highlightUpdateStart)
                .count();
        m_highlightUpdateMsAccumInCurrentWindow += highlightUpdateElapsedMs;
        ++m_highlightUpdateSamplesInCurrentWindow;
    }
    m_mouseMoved = false;

    // Draw labels before swapping buffers to avoid losing them.
    if (!pauseHeavyTasks && !skipLabelWork) {
    if (FixtureTablePanel::Instance() &&
        FixtureTablePanel::Instance()->IsActivePage())
            m_controller.DrawFixtureLabels(w, h);
    else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage())
            m_controller.DrawTrussLabels(w, h);
    else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage())
            m_controller.DrawSceneObjectLabels(w, h);
    }

    if (m_rectSelecting)
        DrawSelectionRectangle(w, h);
    if (m_selectionDragArmed)
        DrawSelectionDragGizmo(renderSize);
    DrawMeasureOverlay(renderSize);

    ++m_fullRefreshesInCurrentWindow;

    const auto telemetryNow = std::chrono::steady_clock::now();
    if (m_refreshTelemetryWindowStart.time_since_epoch().count() == 0)
        m_refreshTelemetryWindowStart = telemetryNow;
    const auto telemetryElapsed = telemetryNow - m_refreshTelemetryWindowStart;
    if (telemetryElapsed >= std::chrono::seconds(1)) {
    const double avgFullRenderMs =
        m_fullRenderSamplesInCurrentWindow > 0
            ? (m_fullRenderMsAccumInCurrentWindow /
               static_cast<double>(m_fullRenderSamplesInCurrentWindow))
            : 0.0;
    const double avgHoverQueryMs =
        m_hoverQuerySamplesInCurrentWindow > 0
            ? (m_hoverQueryMsAccumInCurrentWindow /
               static_cast<double>(m_hoverQuerySamplesInCurrentWindow))
            : 0.0;
        const double avgHighlightUpdateMs =
            m_highlightUpdateSamplesInCurrentWindow > 0
                ? (m_highlightUpdateMsAccumInCurrentWindow /
                   static_cast<double>(m_highlightUpdateSamplesInCurrentWindow))
                : 0.0;
    wxLogDebug("Viewer3DPanel refreshes/s full=%d highlight=%d "
               "full_render_ms=%.3f hover_query_ms=%.3f "
               "highlight_update_ms=%.3f hover_samples=%d highlight_samples=%d",
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

    if (!highlightChanged && highlightRefreshPendingAtFrameStart)
        m_highlightRefreshPending = false;
    if (highlightChanged)
        Refresh(false);
    if (m_selectionRefreshPending)
        m_selectionRefreshPending = false;

    SwapBuffers(); // Swap after drawing labels to ensure they are visible
    viewer3d::diagnostics::Log("OpenGL render frame end.");
}

// Resize event handler
// Schedules a redraw when the 3D panel is resized.
void Viewer3DPanel::OnResize(wxSizeEvent &event) {
    (void)event;
    m_placementViewRevision.Invalidate();
    Refresh();
}

// Renders the full 3D scene
void Viewer3DPanel::Render(const RenderSize &renderSize) {
    viewer3d::diagnostics::Log("Viewer3DPanel::Render start.");
    if (!IsShownOnScreen()) {
    viewer3d::diagnostics::Log(
        "Viewer3DPanel::Render skipped because the panel is hidden.");
        return;
    }
  if (!m_glContext || !gl_lifecycle::TrySetCurrent(*this, m_glContext,
                                                   "Viewer3DPanel", "Render")) {
    viewer3d::diagnostics::Log(
        "Viewer3DPanel::Render skipped because SetCurrent failed.");
        wxASSERT_MSG(false, "Unable to make 3D GL context current for render.");
        return;
    }

    static unsigned long long s_renderFrameId = 0;
    const int width = renderSize.width;
    const int height = renderSize.height;
    ApplyKnownViewer3DFrameState(width, height);
  const RenderSize viewportSize{width, height,
                                "ApplyKnownViewer3DFrameState(framebuffer-px)"};

    const Viewer3DRenderStyle renderStyle = ResolveRenderStyleFromPreferences();
    ApplyViewer3DClearColorForStyle(renderStyle);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ApplyCameraMatrices(renderSize);
  const RenderSize projectionSize{
      width, height, "ApplyCameraMatrices::projection(framebuffer-px)"};

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
    viewer3d::diagnostics::Log("Viewer3DPanel::Render end.");
}

// Exports the current 3D view to a PNG image chosen by the user.
bool Viewer3DPanel::ExportCurrentViewToPng() {
    if (!IsShownOnScreen()) {
    wxMessageBox("Cannot export while the 3D viewer is hidden.", "Export image",
                 wxOK | wxICON_WARNING, this);
        return false;
    }

  wxFileDialog saveDialog(this, "Export image", wxEmptyString,
                          "viewer3d_export.png", "PNG files (*.png)|*.png",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDialog.ShowModal() != wxID_OK)
        return false;

    std::string path = saveDialog.GetPath().ToStdString();
    if (path.empty())
        return false;

    if (!gl_lifecycle::TrySetCurrent(*this, m_glContext, "Viewer3DPanel",
                                      "ExportImage")) {
        wxMessageBox("OpenGL is not initialized yet.", "Export image",
                     wxOK | wxICON_ERROR, this);
        return false;
    }
    if (!InitGL() || !m_glInitialized) {
        wxMessageBox("OpenGL is not initialized yet.", "Export image",
                     wxOK | wxICON_ERROR, this);
        return false;
    }

    const RenderSize sourceRenderSize = ResolveRenderSize(this);
    int sourceWidth = sourceRenderSize.width;
    int sourceHeight = sourceRenderSize.height;
  const double exportFovYDegrees = ComputeExpandedFovYDegrees(
      sourceWidth, sourceHeight, kExportImageWidth, kExportImageHeight);

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kExportImageWidth,
               kExportImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
  const RenderSize exportRenderSize{kExportImageWidth, kExportImageHeight,
                                    "export-fbo"};
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

// Applies viewport, projection, and camera matrices for the current 3D view.
void Viewer3DPanel::ApplyCameraMatrices(const RenderSize &renderSize,
                                        double fovYDegrees) {
    if (!renderSize.IsValid()) {
    viewer3d::diagnostics::Log(
        "ApplyCameraMatrices rejected invalid render size.");
        wxASSERT_MSG(false, "Invalid render size passed to 3D camera matrices.");
        return;
    }
  if (!std::isfinite(fovYDegrees) || fovYDegrees <= 0.0 ||
      fovYDegrees >= 179.0) {
    viewer3d::diagnostics::Log(
        "ApplyCameraMatrices replaced invalid field of view.");
        wxASSERT_MSG(false, "Invalid field of view passed to 3D camera matrices.");
        fovYDegrees = kDefaultFovYDegrees;
    }
  wxASSERT_MSG(m_camera.IsValid(),
               "Invalid 3D camera state before matrix generation.");
    const int width = renderSize.width;
    const int height = renderSize.height;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr double kNearPlane = 0.05;
    constexpr double kFarPlane = 2000.0;
  const double aspectRatio =
      static_cast<double>(width) / static_cast<double>(height);
    gluPerspective(fovYDegrees, aspectRatio, kNearPlane, kFarPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    m_camera.Apply();
}

// Toggles the 3D measure tool mode and clears any pending measurement points.
// Toggles the 3D measurement tool and refreshes tool state.
void Viewer3DPanel::SetMeasureToolEnabled(bool enabled) {
    SetMeasureToolEnabled(enabled, m_measureMode);
}

// Toggles the 3D measurement tool and applies the requested measuring mode.
void Viewer3DPanel::SetMeasureToolEnabled(bool enabled,
                                          Viewer2DMeasureMode mode) {
    m_measureToolEnabled = enabled;
    m_measureMode = mode;
    ResetMeasureState();
    if (MainWindow::Instance())
        MainWindow::Instance()->SyncViewportToolToggleState(enabled, m_measureMode);
    SetCursor(enabled ? wxCursor(wxCURSOR_CROSS) : wxCursor(wxCURSOR_ARROW));
    Refresh();
}

// Resets the active and committed 3D measurement points while preserving
// enablement. Clears the active and committed 3D measurement points.
void Viewer3DPanel::ResetMeasureState() {
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

// Resolves cached world bounds for a 3D measurement target.
std::optional<ISelectionContext::BoundingBox> Viewer3DPanel::ResolveMeasureBoundsFromUuid(
    HoverTargetTable target, const std::string& uuid) const
{
    const auto& selectionContext = static_cast<const ISelectionContext&>(m_controller);
    const ISelectionContext::BoundingBox* bounds = nullptr;
    if (target == HoverTargetTable::Fixtures)
        bounds = selectionContext.FindFixtureBounds(uuid);
    else if (target == HoverTargetTable::Trusses)
        bounds = selectionContext.FindTrussBounds(uuid);
    else if (target == HoverTargetTable::SceneObjects)
        bounds = selectionContext.FindObjectBounds(uuid);
    if (!bounds) {
        if (const auto* fixtureBounds = selectionContext.FindFixtureBounds(uuid))
            bounds = fixtureBounds;
        else if (const auto* trussBounds = selectionContext.FindTrussBounds(uuid))
            bounds = trussBounds;
        else if (const auto* objectBounds = selectionContext.FindObjectBounds(uuid))
            bounds = objectBounds;
    }
    if (bounds)
        return *bounds;
    if (const auto center = ResolveMeasureWorldFromUuid(target, uuid))
        return ISelectionContext::BoundingBox{*center, *center};
    return std::nullopt;
}

// Computes the nearest points between two 3D world-space bounding boxes.
std::pair<std::array<float, 3>, std::array<float, 3>>
Viewer3DPanel::ComputeNearestBoundsPoints(
    const ISelectionContext::BoundingBox &a,
    const ISelectionContext::BoundingBox &b) const {
    std::array<float, 3> start{0.0f, 0.0f, 0.0f};
    std::array<float, 3> end{0.0f, 0.0f, 0.0f};
    for (int axis = 0; axis < 3; ++axis) {
        if (a.max[axis] < b.min[axis]) {
            start[axis] = a.max[axis];
            end[axis] = b.min[axis];
        } else if (b.max[axis] < a.min[axis]) {
            start[axis] = a.min[axis];
            end[axis] = b.max[axis];
        } else {
            const float overlap = std::max(a.min[axis], b.min[axis]);
            start[axis] = overlap;
            end[axis] = overlap;
        }
    }
    return {start, end};
}

// Draws the active 3D measurement line and distance text when two elements are
// fixed. Draws the active 3D measurement line and distance label.
void Viewer3DPanel::DrawMeasureOverlay(const RenderSize &renderSize) {
    if (!m_measureToolEnabled || !m_measureHasAnchor || !renderSize.IsValid())
        return;

  const auto anchorFramebuffer =
      ProjectWorldToFramebuffer(m_measureAnchorDrawWorldMeters);
    if (!anchorFramebuffer)
        return;

    float endX = (*anchorFramebuffer)[0];
    float endY = (*anchorFramebuffer)[1];
    bool showDistanceLabel = false;
    std::array<float, 3> lineEndWorld = m_measureAnchorDrawWorldMeters;
    if (m_measureHasCommittedTarget) {
        const auto targetFramebuffer =
            ProjectWorldToFramebuffer(m_measureCommittedTargetDrawWorldMeters);
        if (!targetFramebuffer)
            return;
        endX = (*targetFramebuffer)[0];
        endY = (*targetFramebuffer)[1];
        lineEndWorld = m_measureCommittedTargetDrawWorldMeters;
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
  viewer_common::DrawMeasureOverlayStyle(x0, y0, x1, y1, Is2DDarkModeEnabled());

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

    const float dx = lineEndWorld[0] - m_measureAnchorDrawWorldMeters[0];
    const float dy = lineEndWorld[1] - m_measureAnchorDrawWorldMeters[1];
    const float dz = lineEndWorld[2] - m_measureAnchorDrawWorldMeters[2];
    const float distanceMeters = std::sqrt(dx * dx + dy * dy + dz * dz);
  const auto distanceUnitSystem = Units::ParseDistanceUnitSystem(
      ConfigManager::Get().GetValue("ui_distance_unit_system"));
    const std::string distanceText =
      Units::FormatDistanceFromMillimeters(
          static_cast<double>(distanceMeters) * 1000.0, distanceUnitSystem,
                                             Units::ValueFormatContext::Label) +
        " " + Units::DistanceUnitSuffix(distanceUnitSystem);

    const float labelX = ((tx0 + tx1) * 0.5f);
    const float labelY =
        static_cast<float>(renderSize.height) - ((ty0 + ty1) * 0.5f) - 10.0f;
    const float labelAngleDegrees = NormalizeMeasureLabelAngleDegrees(
        -std::atan2(ty1 - ty0, tx1 - tx0) * (180.0f / 3.14159265358979323846f));
  std::vector<OverlayTextLabel> labels{{labelX, labelY, distanceText, true,
                                        true, 20.0f, true, 0.95f, 0.1f, 0.1f,
                                        labelAngleDegrees}};
    m_controller.DrawOverlayTextLabels(labels, Is2DDarkModeEnabled());
    if (MainWindow::Instance() && MainWindow::Instance()->GetStatusBar())
    MainWindow::Instance()->SetStatusText(
        wxString::FromUTF8("Measure: " + distanceText), 0);
}

// Handles mouse button press
void Viewer3DPanel::OnMouseDown(wxMouseEvent &event) {
    m_hasLastMousePos = true;
    if (m_continuousPlacementActive && event.LeftDown()) {
    m_mode = event.ShiftDown() ? InteractionMode::Pan : InteractionMode::Orbit;
        m_dragging = true;
        m_controller.SetInteracting(true);
        m_isInteracting = true;
        m_cameraMoving = true;
        m_lastInteractionTime = std::chrono::steady_clock::now();
        m_draggedSincePress = false;
        m_lastMousePos = event.GetPosition();
        SetFocus();
        CaptureMouse();
        return;
    }
  viewer3d::diagnostics::Logf(
      "Mouse interaction start pos=(%d,%d) left=%d middle=%d right=%d shift=%d "
      "ctrl=%d",
                                event.GetX(), event.GetY(), event.LeftDown() ? 1 : 0,
                                event.MiddleDown() ? 1 : 0, event.RightDown() ? 1 : 0,
                                event.ShiftDown() ? 1 : 0, event.ControlDown() ? 1 : 0);
    ResetSelectionDragState();
  if (event.LeftDown() || event.MiddleDown() || event.RightDown()) {
        if (event.LeftDown() && event.ControlDown()) {
            m_rectSelecting = true;
            m_rectSelectionAcrossAllTables =
                event.ShiftDown() || IsCrossTableViewportActionsEnabled();
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
        !event.RightDown() && m_leftDragSelectionMovementEnabled) {
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
void Viewer3DPanel::OnMouseUp(wxMouseEvent &event) {
  viewer3d::diagnostics::Logf("Mouse interaction end pos=(%d,%d) leftUp=%d "
                              "middleUp=%d rightUp=%d dragged=%d",
                              event.GetX(), event.GetY(),
                              event.LeftUp() ? 1 : 0, event.MiddleUp() ? 1 : 0,
                              event.RightUp() ? 1 : 0,
                                m_draggedSincePress ? 1 : 0);
    if (m_continuousPlacementActive && event.LeftUp()) {
        const bool navigated = m_draggedSincePress;
        m_dragging = false;
        m_isInteracting = false;
        m_cameraMoving = false;
        m_controller.SetInteracting(false);
        m_controller.SetCameraMoving(false);
        m_mode = InteractionMode::None;
        if (HasCapture())
            ReleaseMouse();
        m_draggedSincePress = false;
        if (navigated) {
            AlignContinuousElementToPointer(event.GetPosition());
        } else {
            ConfirmContinuousPlacement();
        }
        m_forceHoverQuery = true;
        Refresh();
        return;
    }
  if (event.LeftUp() && m_rectSelecting) {
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

  if (event.LeftUp() && m_selectionDragArmed) {
        if (HasCapture())
            ReleaseMouse();
        if (m_selectionDragMoved) {
            CommitActiveMagnetSnap();
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

  if (m_dragging && (event.LeftUp() || event.MiddleUp() || event.RightUp())) {
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

  if (event.LeftUp() && !m_draggedSincePress) {
        m_forceHoverQuery = true;
        const RenderSize renderSize = ResolveRenderSize(this);
        const int w = renderSize.width;
        const int h = renderSize.height;
        if (!renderSize.IsValid())
            return;
        if (!TryBindGlContextForInteraction("OnMouseUp"))
            return;
        std::string uuid;
        const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
        HoverTargetTable activeTable = ResolveActiveHoverTargetTable();
        if (IsCrossTableViewportActionsEnabled())
            activeTable = HoverTargetTable::Fixtures;
        HoverTargetTable pickedTable = activeTable;
    bool found =
        IsCrossTableViewportActionsEnabled()
            ? QueryHoverUuidAcrossTables(m_controller, pickPos.x, pickPos.y, w,
                                         h, uuid, pickedTable, true)
            : QueryHoverUuid(m_controller, activeTable, pickPos.x, pickPos.y, w,
                             h, uuid, true);
        if (!found && !m_hoverUuid.empty()) {
            uuid = m_hoverUuid;
            found = true;
        }

        ConfigManager& cfg = ConfigManager::Get();
        if (m_measureToolEnabled) {
            if (!found) {
        found =
            IsCrossTableViewportActionsEnabled()
                ? QueryHoverUuidAcrossTables(m_controller, pickPos.x, pickPos.y,
                                             w, h, uuid, pickedTable, false)
                            : QueryHoverUuid(m_controller, activeTable, pickPos.x,
                                             pickPos.y, w, h, uuid, false);
            }
            if (!found && !m_hoverUuid.empty()) {
                uuid = m_hoverUuid;
                found = true;
            }
            if (found) {
                const auto worldPos = ResolveMeasureWorldFromUuid(pickedTable, uuid);
                if (worldPos) {
                    if (!m_measureHasAnchor || m_measureHasCommittedTarget) {
                        ResetMeasureState();
                        m_measureHasAnchor = true;
                        m_measureAnchorUuid = uuid;
                        m_measureAnchorWorldMeters = *worldPos;
                        m_measureAnchorDrawWorldMeters = *worldPos;
                    } else {
                        m_measureHasCommittedTarget = true;
                        m_measureCommittedTargetWorldMeters = *worldPos;
                        m_measureCommittedTargetDrawWorldMeters = *worldPos;
                        if (m_measureMode == Viewer2DMeasureMode::EdgeToEdge) {
                            const auto anchorBounds = ResolveMeasureBoundsFromUuid(
                                pickedTable, m_measureAnchorUuid);
              const auto targetBounds =
                  ResolveMeasureBoundsFromUuid(pickedTable, uuid);
                            if (anchorBounds && targetBounds) {
                const auto nearest =
                    ComputeNearestBoundsPoints(*anchorBounds, *targetBounds);
                                m_measureAnchorDrawWorldMeters = nearest.first;
                                m_measureCommittedTargetDrawWorldMeters = nearest.second;
                            }
                        }
                    }
                    Refresh();
                }
            } else {
                ResetMeasureState();
                Refresh();
            }
            return;
        }
    if (found) {
            const bool additive = event.ShiftDown() || event.ControlDown();
            const bool addOnly = event.ControlDown();
            const HoverTableHighlights clickedSelection =
                BuildClickSelectionHighlights(cfg.GetScene(), uuid);
            HoverTableHighlights resolvedSelection;
      resolvedSelection.fixtures =
          ResolveClickedSelection(cfg.GetSelectedFixtures(),
                                  clickedSelection.fixtures, additive, addOnly);
      resolvedSelection.trusses =
          ResolveClickedSelection(cfg.GetSelectedTrusses(),
                                  clickedSelection.trusses, additive, addOnly);
      resolvedSelection.supports =
          ResolveClickedSelection(cfg.GetSelectedSupports(),
                                  clickedSelection.supports, additive, addOnly);
            resolvedSelection.sceneObjects = ResolveClickedSelection(
                cfg.GetSelectedSceneObjects(), clickedSelection.sceneObjects,
                additive, addOnly);
            ApplyObjectSelectionToUi(resolvedSelection, this);
    } else {
            ClearAllObjectSelections("clear selection");
        }
    }
    m_draggedSincePress = false;
}

// Synchronizes the current hover highlight with the 3D controller and tables.
void Viewer3DPanel::SynchronizeHoverHighlight() {
    ++m_highlightRevision;
    m_highlightRefreshPending = true;
    wxLogDebug("Viewer3D hover highlight changed: uuid=%s revision=%llu",
               m_hoverUuid.c_str(),
               static_cast<unsigned long long>(m_highlightRevision));
    m_controller.SetHighlightUuid(m_hoverUuid);

    const MvrScene& scene = ConfigManager::Get().GetScene();
    const HoverTableHighlights relatedHighlights =
        BuildHoverTableHighlights(scene, m_hoverUuid);
    if (FixtureTablePanel::Instance()) {
        const std::string primaryUuid =
            scene.fixtures.find(m_hoverUuid) != scene.fixtures.end()
                ? std::string(m_hoverUuid)
                : std::string();
    FixtureTablePanel::Instance()->HighlightFixture(primaryUuid,
                                                    relatedHighlights.fixtures);
    }
    if (TrussTablePanel::Instance()) {
        const std::string primaryUuid =
            scene.trusses.find(m_hoverUuid) != scene.trusses.end()
                ? std::string(m_hoverUuid)
                : std::string();
    TrussTablePanel::Instance()->HighlightTruss(primaryUuid,
                                                relatedHighlights.trusses);
    }
    if (HoistTablePanel::Instance()) {
        const std::string primaryUuid =
            scene.supports.find(m_hoverUuid) != scene.supports.end()
                ? std::string(m_hoverUuid)
                : std::string();
    HoistTablePanel::Instance()->HighlightHoist(primaryUuid,
                                                relatedHighlights.supports);
    }
    if (SceneObjectTablePanel::Instance()) {
        const std::string primaryUuid =
            scene.sceneObjects.find(m_hoverUuid) != scene.sceneObjects.end()
                ? std::string(m_hoverUuid)
                : std::string();
        SceneObjectTablePanel::Instance()->HighlightObject(
            primaryUuid, relatedHighlights.sceneObjects);
    }
}


// Clears every object-selection store and synchronizes table and viewport highlights.
void Viewer3DPanel::ClearAllObjectSelections(const char* undoLabel)
{
    ConfigManager& cfg = ConfigManager::Get();
    const bool hasAnySelection =
        !cfg.GetSelectedFixtures().empty() || !cfg.GetSelectedTrusses().empty() ||
        !cfg.GetSelectedSupports().empty() || !cfg.GetSelectedSceneObjects().empty();

    if (hasAnySelection) {
        cfg.PushUndoState(undoLabel);
        cfg.SetSelectedFixtures({});
        cfg.SetSelectedTrusses({});
        cfg.SetSelectedSupports({});
        cfg.SetSelectedSceneObjects({});
    }

    SetSelectedFixtures({});
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->SetSelectedUuids({});
    if (FixtureTablePanel::Instance())
        FixtureTablePanel::Instance()->ClearSelection();
    if (TrussTablePanel::Instance())
        TrussTablePanel::Instance()->ClearSelection();
    if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->ClearSelection();
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->ClearSelection();
}

// Returns the display key used to group trusses by model or source file.
static std::string BuildTrussModelSelectionKey(const Truss &truss) {
    if (!truss.model.empty())
        return truss.model;
  std::string fileName =
      !truss.modelFile.empty() ? truss.modelFile : truss.symbolFile;
    const size_t slash = fileName.find_last_of("/\\");
    if (slash != std::string::npos)
        fileName = fileName.substr(slash + 1);
    return fileName;
}

// Builds truss UUIDs filtered by model-or-source-file key for selection
// workflows.
static std::vector<std::string>
BuildTrussSelectionByModelKey(const MvrScene &scene,
                              const std::string &modelKey) {
    std::vector<std::string> uuids;
    uuids.reserve(scene.trusses.size());
    for (const auto& [uuid, truss] : scene.trusses) {
        if (modelKey.empty() || BuildTrussModelSelectionKey(truss) == modelKey)
            uuids.push_back(uuid);
    }
    return uuids;
}

// Builds truss UUIDs filtered by position-name mapping criteria.
static std::vector<std::string>
BuildTrussSelectionByPosition(const MvrScene &scene,
                              const std::string &positionName,
                              bool selectNoPosition) {
    std::vector<std::string> uuids;
    uuids.reserve(scene.trusses.size());
    for (const auto& [uuid, truss] : scene.trusses) {
        const bool hasPosition = !truss.positionName.empty();
        if (selectNoPosition) {
            if (!hasPosition)
                uuids.push_back(uuid);
            continue;
        }
        if (positionName.empty() || truss.positionName == positionName)
            uuids.push_back(uuid);
    }
    return uuids;
}

// Applies a truss selection to scene state, controller highlights, and table UI.
static void ApplyTrussSelectionToUi(const std::vector<std::string>& selection,
                             Viewer3DPanel* panel,
                             Viewer3DController& controller)
{
    ConfigManager& cfg = ConfigManager::Get();
    if (selection != cfg.GetSelectedTrusses()) {
        cfg.PushUndoState("truss selection");
        cfg.SetSelectedTrusses(selection);
    }
    controller.SetSelectedUuids(selection);
    selection::ScopedOrigin selectionOrigin(selection::Origin::Viewer3D);
    if (panel)
        panel->SetSelectedFixtures(selection);
    if (TrussTablePanel::Instance()) {
        if (selection.empty())
            TrussTablePanel::Instance()->ClearSelection();
        else
            TrussTablePanel::Instance()->SelectByUuid(selection, false);
    }
}

// Opens the right-click selection and render-style context menu.
void Viewer3DPanel::OnRightUp(wxMouseEvent &event) {
    if (m_continuousPlacementActive) {
        CancelContinuousPlacement();
        return;
    }
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
    const bool trussPageActive =
        TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage();
    const auto& scene = ConfigManager::Get().GetScene();
    viewer3d::context_menu::Input menuInput;
    if (fixturePageActive)
        menuInput.page = viewer3d::context_menu::SelectionPage::Fixtures;
    else if (trussPageActive)
        menuInput.page = viewer3d::context_menu::SelectionPage::Trusses;
    std::string hitSceneObjectUuid;
    menuInput.pickingAvailable = TryBindGlContextForInteraction("OnRightUp");
    if (menuInput.pickingAvailable) {
        const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
        QueryHoverUuid(m_controller, HoverTargetTable::SceneObjects, pickPos.x,
                       pickPos.y, w, h, hitSceneObjectUuid);
        std::string hitUuid;
        if (fixturePageActive && !scene.fixtures.empty()) {
            menuInput.selectionObjectHit = QueryHoverUuid(
                m_controller, HoverTargetTable::Fixtures, pickPos.x, pickPos.y,
                w, h, hitUuid);
            for (const auto& [uuid, fixture] : scene.fixtures) {
                menuInput.objects.push_back(
                    {fixture.typeName, fixture.positionName});
            }
        } else if (trussPageActive && !scene.trusses.empty()) {
            menuInput.selectionObjectHit = QueryHoverUuid(
                m_controller, HoverTargetTable::Trusses, pickPos.x, pickPos.y,
                w, h, hitUuid);
            for (const auto& [uuid, truss] : scene.trusses) {
                menuInput.objects.push_back(
                    {BuildTrussModelSelectionKey(truss), truss.positionName});
            }
        }
    }
    menuInput.convertibleSceneObjectHit = !hitSceneObjectUuid.empty();
    const viewer3d::context_menu::Model menuModel =
        viewer3d::context_menu::Build(menuInput);

    wxMenu rootMenu;
    auto renderStyleSubmenu = std::make_unique<wxMenu>();

    constexpr int kSelectTypeAllId = wxID_HIGHEST + 900;
    constexpr int kSelectPositionNoneId = wxID_HIGHEST + 1100;
    constexpr int kRenderStyleStandardId = wxID_HIGHEST + 1200;
    constexpr int kRenderStyleWhiteId = wxID_HIGHEST + 1201;
    constexpr int kRenderStyleSketchId = wxID_HIGHEST + 1202;
    constexpr int kRenderStyleTexturedId = wxID_HIGHEST + 1203;
    constexpr int kRenderStyleWireframeId = wxID_HIGHEST + 1204;
    constexpr int kRenderStyleByDeviceTypeId = wxID_HIGHEST + 1205;
    constexpr int kRenderStyleByLayerId = wxID_HIGHEST + 1206;
    constexpr int kRenderStyleByUniverseId = wxID_HIGHEST + 1207;
    constexpr int kExportImagePngId = wxID_HIGHEST + 1208;
    constexpr int kConvertSceneObjectToTrussId = wxID_HIGHEST + 1209;

    const auto& orderedTypes = menuModel.typesOrModels;
    const auto& orderedPositions = menuModel.positions;
    std::vector<int> orderedTypeIds;
    std::vector<int> orderedPositionIds;

    renderStyleSubmenu->AppendRadioItem(kRenderStyleStandardId, "Standard");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleSketchId, "Sketch mode");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleTexturedId, "Textured");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWireframeId, "Wireframe");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleWhiteId, "White");
  renderStyleSubmenu->AppendRadioItem(kRenderStyleByDeviceTypeId,
                                      "By device type");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleByLayerId, "By layer");
    renderStyleSubmenu->AppendRadioItem(kRenderStyleByUniverseId, "By universe");
  const Viewer3DRenderStyle activeRenderStyle =
      ResolveRenderStyleFromPreferences();
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

    if (menuModel.convertToTrussAvailable) {
        rootMenu.Append(kConvertSceneObjectToTrussId, "Convert to Truss");
        rootMenu.AppendSeparator();
    }
    if (menuModel.selectionAvailable) {
        wxASSERT_MSG(!menuModel.selectAllLabel.empty() &&
                         !menuModel.typeSubmenuLabel.empty(),
                     "Selection menu labels must not be empty.");
        auto typeSubmenu = std::make_unique<wxMenu>();
        typeSubmenu->Append(kSelectTypeAllId,
                            wxString::FromUTF8(menuModel.selectAllLabel));
        for (const auto& typeName : orderedTypes) {
            wxASSERT_MSG(!typeName.empty(), "Selection type label must not be empty.");
            const int itemId = wxWindow::NewControlId();
            orderedTypeIds.push_back(itemId);
            typeSubmenu->Append(itemId, wxString::FromUTF8(typeName));
        }
        auto positionSubmenu = std::make_unique<wxMenu>();
        positionSubmenu->Append(kSelectPositionNoneId, "No position");
        for (const auto& positionName : orderedPositions) {
            wxASSERT_MSG(!positionName.empty(),
                         "Selection position label must not be empty.");
            const int itemId = wxWindow::NewControlId();
            orderedPositionIds.push_back(itemId);
            positionSubmenu->Append(itemId,
                                    wxString::FromUTF8(positionName));
        }
        rootMenu.AppendSubMenu(typeSubmenu.release(),
                               wxString::FromUTF8(menuModel.typeSubmenuLabel));
        rootMenu.AppendSubMenu(positionSubmenu.release(), "Select by position");
        rootMenu.AppendSeparator();
    }
    rootMenu.AppendSubMenu(renderStyleSubmenu.release(), "Render style");
    rootMenu.AppendSeparator();
    rootMenu.Append(kExportImagePngId, "Export image...");

  const int selectedId =
      GetPopupMenuSelectionFromUser(rootMenu, event.GetPosition());
    if (selectedId == wxID_NONE)
        return;

    if (selectedId == kConvertSceneObjectToTrussId) {
        ConfigManager::Get().SetSelectedSceneObjects({hitSceneObjectUuid});
        wxCommandEvent command(wxEVT_MENU, ID_Tools_ConvertSceneObjectsToTruss);
        if (MainWindow::Instance())
            MainWindow::Instance()->ProcessWindowEvent(command);
        return;
    }

    auto applyRenderStyleSelection = [this](Viewer3DRenderStyle style) {
    ConfigManager::Get().SetValue("viewer3d_render_style",
                                  ToConfigValue(style));
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
        if (fixturePageActive)
            ApplyFixtureSelectionToUi(BuildFixtureSelectionByType(scene, ""), this,
                                      m_controller);
        else
            ApplyTrussSelectionToUi(BuildTrussSelectionByModelKey(scene, ""), this,
                                    m_controller);
        Refresh();
        return;
    }

    const auto selectedType =
        std::find(orderedTypeIds.begin(), orderedTypeIds.end(), selectedId);
    if (selectedType != orderedTypeIds.end()) {
        const size_t idx =
            static_cast<size_t>(selectedType - orderedTypeIds.begin());
        if (fixturePageActive)
      ApplyFixtureSelectionToUi(
          BuildFixtureSelectionByType(scene, orderedTypes[idx]), this,
                                      m_controller);
        else
      ApplyTrussSelectionToUi(
          BuildTrussSelectionByModelKey(scene, orderedTypes[idx]), this,
                                    m_controller);
        Refresh();
        return;
    }

    if (selectedId == kSelectPositionNoneId) {
        if (!menuModel.hasNoPosition) {
            if (fixturePageActive)
                ApplyFixtureSelectionToUi({}, this, m_controller);
            else
                ApplyTrussSelectionToUi({}, this, m_controller);
        } else {
            if (fixturePageActive)
        ApplyFixtureSelectionToUi(
            BuildFixtureSelectionByPosition(scene, "", true), this,
            m_controller);
            else
                ApplyTrussSelectionToUi(BuildTrussSelectionByPosition(scene, "", true),
                                        this, m_controller);
        }
        Refresh();
        return;
    }

    const auto selectedPosition = std::find(
        orderedPositionIds.begin(), orderedPositionIds.end(), selectedId);
    if (selectedPosition != orderedPositionIds.end()) {
        const size_t idx =
            static_cast<size_t>(selectedPosition - orderedPositionIds.begin());
        if (fixturePageActive)
            ApplyFixtureSelectionToUi(
          BuildFixtureSelectionByPosition(scene, orderedPositions[idx], false),
          this, m_controller);
        else
            ApplyTrussSelectionToUi(
          BuildTrussSelectionByPosition(scene, orderedPositions[idx], false),
          this, m_controller);
        Refresh();
        return;
    }
}

// Enables or disables Magnet snapping for 3D selection dragging.
void Viewer3DPanel::SetMagnetEnabled(bool enabled, bool persist) {
    m_magnetEnabled = enabled;
    m_pendingMagnetSnap.reset();
    if (!persist)
        return;
    ConfigManager::Get().SetValue(magnet_snap::kMagnetEnabledConfigKey,
                                  enabled ? "1" : "0");
    ConfigManager::Get().SaveUserConfig();
}

// Enables or disables left-click selection dragging in the 3D viewport.
void Viewer3DPanel::SetLeftDragSelectionMovementEnabled(bool enabled) {
    m_leftDragSelectionMovementEnabled = enabled;
}

// Enables or disables axis-constrained selection movement in the 3D viewport.
void Viewer3DPanel::SetAxisConstrainedMovementEnabled(bool enabled) {
    m_axisConstrainedMovementEnabled = enabled;
    if (!enabled)
        m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
}

// Sets whether axis-constrained viewport transforms use world or local axes.
void Viewer3DPanel::SetTransformSpace(transform_space::TransformSpace space) {
    m_transformSpace = space;
}

// Resets interaction state after wxWidgets reports lost mouse capture.
void Viewer3DPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(event)) {
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

// Applies rectangle selection to the active or combined object tables.
void Viewer3DPanel::ApplyRectangleSelection(const wxPoint& start,
                                            const wxPoint& end)
{
    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (w <= 0 || h <= 0) {
        return;
    }
    if (!TryBindGlContextForInteraction("ApplyRectangleSelection"))
        return;

    ConfigManager& cfg = ConfigManager::Get();
    const wxPoint pickStart = ToFramebufferPoint(this, start);
    const wxPoint pickEnd = ToFramebufferPoint(this, end);
  if (m_rectSelectionAcrossAllTables) {
        HoverTableHighlights selection = BuildCurrentSelectionHighlights(cfg);
        selection.fixtures = AddClickedUuids(
        selection.fixtures,
        m_controller.GetFixturesInScreenRect(pickStart.x, pickStart.y,
                                             pickEnd.x, pickEnd.y, w, h));
        selection.trusses = AddClickedUuids(
        selection.trusses,
        m_controller.GetTrussesInScreenRect(pickStart.x, pickStart.y, pickEnd.x,
                                   pickEnd.y, w, h));
        selection.sceneObjects = AddClickedUuids(
        selection.sceneObjects,
        m_controller.GetSceneObjectsInScreenRect(pickStart.x, pickStart.y,
                                                 pickEnd.x, pickEnd.y, w, h));
        ApplyObjectSelectionToUi(selection, this);
        return;
    }

  if (FixtureTablePanel::Instance() &&
      FixtureTablePanel::Instance()->IsActivePage()) {
        HoverTableHighlights selection = BuildCurrentSelectionHighlights(cfg);
        selection.fixtures = AddClickedUuids(
        selection.fixtures,
        m_controller.GetFixturesInScreenRect(pickStart.x, pickStart.y,
                                             pickEnd.x, pickEnd.y, w, h));
        ApplyObjectSelectionToUi(selection, this);
  } else if (TrussTablePanel::Instance() &&
             TrussTablePanel::Instance()->IsActivePage()) {
        HoverTableHighlights selection = BuildCurrentSelectionHighlights(cfg);
        selection.trusses = AddClickedUuids(
        selection.trusses,
        m_controller.GetTrussesInScreenRect(pickStart.x, pickStart.y, pickEnd.x,
                                   pickEnd.y, w, h));
        ApplyObjectSelectionToUi(selection, this);
  } else if (SceneObjectTablePanel::Instance() &&
             SceneObjectTablePanel::Instance()->IsActivePage()) {
        HoverTableHighlights selection = BuildCurrentSelectionHighlights(cfg);
        selection.sceneObjects = AddClickedUuids(
        selection.sceneObjects,
        m_controller.GetSceneObjectsInScreenRect(pickStart.x, pickStart.y,
                                                 pickEnd.x, pickEnd.y, w, h));
        ApplyObjectSelectionToUi(selection, this);
    }
}

// Draws the current screen-space selection rectangle overlay.
void Viewer3DPanel::DrawSelectionRectangle(int width, int height) {
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
  glOrtho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height),
          -1.0f, 1.0f);
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

// Clears all transient selection-drag state.
void Viewer3DPanel::ResetSelectionDragState() {
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
    m_pendingMagnetSnap.reset();
    if (MainWindow::Instance())
        MainWindow::Instance()->ClearHighlightedWorldPositionInStatusBar();
}

// Computes the world-space center for the active selection drag target.
std::array<float, 3> Viewer3DPanel::ComputeSelectionCenterMeters(
    const std::vector<std::string> &uuids, HoverTargetTable target) const {
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

// Prepares a selection drag when the cursor starts on a draggable label.
bool Viewer3DPanel::PrepareSelectionDrag(const wxPoint &mousePos) {
  viewer3d::diagnostics::Logf("Selection dragging prepare pos=(%d,%d)",
                              mousePos.x, mousePos.y);
    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid())
        return false;
    if (!TryBindGlContextForInteraction("PrepareSelectionDrag"))
        return false;
    const wxPoint pickPos = ToFramebufferPoint(this, mousePos);
    const HoverTargetTable target = ResolveActiveHoverTargetTable();
    if (target == HoverTargetTable::None)
        return false;

    std::string uuid;
    if (!QueryDragLabelUuid(m_controller, target, pickPos.x, pickPos.y,
                            renderSize.width, renderSize.height, uuid)) {
    viewer3d::diagnostics::Log(
        "Selection dragging prepare found no draggable label.");
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
  const auto hitSelectionIt =
      std::find(selection.begin(), selection.end(), uuid);
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

    const scene_grouping::ObjectSelection dragSelection{
        .fixtures = m_dragFixtureUuids,
        .trusses = m_dragTrussUuids,
        .supports = {},
        .sceneObjects = m_dragSceneObjectUuids};
    const auto dragTargets = scene_grouping::BuildInteractiveTransformTargets(
        cfg.GetScene(), dragSelection,
        selection_movement_settings::LoadInteractiveTransformPolicy(cfg));
    m_selectionDragAnchorMeters = {0.0f, 0.0f, 0.0f};
    if (!dragTargets.empty()) {
        for (const auto& dragTarget : dragTargets) {
            const Matrix transform = scene_grouping::GetTargetWorldTransform(cfg.GetScene(), dragTarget);
            m_selectionDragAnchorMeters[0] += transform.o[0] / 1000.0f;
            m_selectionDragAnchorMeters[1] += transform.o[1] / 1000.0f;
            m_selectionDragAnchorMeters[2] += transform.o[2] / 1000.0f;
        }
        for (float& component : m_selectionDragAnchorMeters)
            component /= static_cast<float>(dragTargets.size());
    }
    m_hasHover = true;
    m_hoverUuid = uuid;
    m_hoverText.clear();
    SynchronizeHoverHighlight();

    m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
    m_pendingMagnetSnap.reset();
    m_selectionDragArmed = true;
    m_selectionDragMoved = false;
    m_selectionDragUndoPushed = false;
    m_selectionDragPressTime = wxGetLocalTimeMillis();
    viewer3d::diagnostics::Logf("Selection dragging prepared target=%d count=%zu",
                              static_cast<int>(target),
                              m_dragSelectionUuids.size());
    return true;
}

// Returns the active world-space vector for a selection drag axis.
std::array<float, 3> Viewer3DPanel::GetSelectionDragAxisVector(
    viewer3d::SelectionDragAxis axis) const {
    const auto worldAxis = AxisVectorFromSelectionDragAxis(axis);
    if (m_transformSpace != transform_space::TransformSpace::Local ||
      axis == viewer3d::SelectionDragAxis::None ||
      m_dragSelectionUuids.empty()) {
        return worldAxis;
    }
    const scene_grouping::ObjectSelection selection{
        .fixtures = m_dragFixtureUuids,
        .trusses = m_dragTrussUuids,
        .supports = {},
        .sceneObjects = m_dragSceneObjectUuids};
    const auto targets = scene_grouping::BuildInteractiveTransformTargets(
      ConfigManager::Get().GetScene(), selection,
      selection_movement_settings::LoadInteractiveTransformPolicy(
          ConfigManager::Get()));
    if (targets.empty())
        return worldAxis;
    const Matrix transform = scene_grouping::GetTargetWorldTransform(
        ConfigManager::Get().GetScene(), targets.front());
    return transform_space::TransformDirection(
        transform_space::ExtractOrientation(transform), worldAxis);
}

// Projects the active drag axes into screen space for axis-constrained
// dragging.
std::array<viewer3d::ProjectedAxis, 3>
Viewer3DPanel::BuildProjectedDragAxes(
    const RenderSize &renderSize,
    const std::array<float, 3> &anchorMeters) const {
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
    if (gluProject(anchorMeters[0], anchorMeters[1], anchorMeters[2], modelview, projection,
                 viewport, &originX, &originY, &originZ) != GL_TRUE) {
        return axes;
    }

    const std::array<std::array<float, 3>, 3> axisWorldVectors{{
        GetSelectionDragAxisVector(viewer3d::SelectionDragAxis::X),
        GetSelectionDragAxisVector(viewer3d::SelectionDragAxis::Y),
        GetSelectionDragAxisVector(viewer3d::SelectionDragAxis::Z),
    }};

    for (size_t i = 0; i < axisWorldVectors.size(); ++i) {
        GLdouble tipX = 0.0;
        GLdouble tipY = 0.0;
        GLdouble tipZ = 0.0;
        const auto& worldAxis = axisWorldVectors[i];
        if (gluProject(anchorMeters[0] + worldAxis[0],
                       anchorMeters[1] + worldAxis[1],
                       anchorMeters[2] + worldAxis[2], modelview,
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

// Returns the unsnapped anchor used by all pointer-movement geometry.
std::array<float, 3> Viewer3DPanel::CurrentRawSelectionDragAnchor() const {
    if (!m_pendingMagnetSnap)
        return m_selectionDragAnchorMeters;
    return continuous_placement::RawAnchorFromPreview(
        m_selectionDragAnchorMeters, m_pendingMagnetSnap->translationDeltaMm);
}

// Projects the mouse position onto the active selection drag view plane.
std::optional<std::array<float, 3>>
Viewer3DPanel::ProjectMouseToSelectionDragViewPlane(
    const wxPoint &mousePos, const RenderSize &renderSize,
    const std::array<float, 3> &planePointMeters) const {
    if (!renderSize.IsValid())
        return std::nullopt;

    GLdouble modelview[16] = {};
    GLdouble projection[16] = {};
    GLint viewport[4] = {0, 0, 0, 0};
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

  const auto framebufferPos =
      TryToFramebufferPoint(const_cast<Viewer3DPanel *>(this), mousePos);
    if (!framebufferPos)
        return std::nullopt;
    const double winX = static_cast<double>(framebufferPos->x);
    const double winY = static_cast<double>(renderSize.height - framebufferPos->y);

    auto unproject = [&](double x, double y, double z) {
        std::array<double, 3> point{0.0, 0.0, 0.0};
        if (gluUnProject(x, y, z, modelview, projection, viewport, &point[0],
                         &point[1], &point[2]) != GL_TRUE) {
            point = {std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::quiet_NaN()};
        }
        return point;
    };

    const auto nearPoint = unproject(winX, winY, 0.0);
    const auto farPoint = unproject(winX, winY, 1.0);
    const auto centerNear = unproject(viewport[0] + viewport[2] * 0.5,
                                      viewport[1] + viewport[3] * 0.5, 0.0);
    const auto centerFar = unproject(viewport[0] + viewport[2] * 0.5,
                                     viewport[1] + viewport[3] * 0.5, 1.0);
  for (double value :
       {nearPoint[0], nearPoint[1], nearPoint[2], farPoint[0], farPoint[1],
        farPoint[2], centerNear[0], centerNear[1], centerNear[2], centerFar[0],
        centerFar[1], centerFar[2]}) {
        if (!std::isfinite(value))
            return std::nullopt;
    }

    const std::array<double, 3> rayDir{farPoint[0] - nearPoint[0],
                                      farPoint[1] - nearPoint[1],
                                      farPoint[2] - nearPoint[2]};
    const std::array<double, 3> planeNormal{centerFar[0] - centerNear[0],
                                           centerFar[1] - centerNear[1],
                                           centerFar[2] - centerNear[2]};
  const std::array<double, 3> planePoint{planePointMeters[0],
                                         planePointMeters[1],
                                         planePointMeters[2]};
    const auto intersection = viewer3d::IntersectRayWithPlane(
        nearPoint, rayDir, planePoint, planeNormal);
    if (!intersection)
        return std::nullopt;
  return std::array<float, 3>{static_cast<float>((*intersection)[0]),
                              static_cast<float>((*intersection)[1]),
                              static_cast<float>((*intersection)[2])};
}

// Builds a Magnet snap source for the active single-object 3D drag.
std::optional<magnet_snap::SnapSource>
Viewer3DPanel::BuildActiveMagnetSource() const {
    if (!m_magnetEnabled || m_measureToolEnabled)
        return std::nullopt;
    if (!m_dragTrussUuids.empty() && m_dragSceneObjectUuids.empty()) {
        scene_grouping::ObjectSelection selection;
        selection.fixtures = m_dragFixtureUuids;
        selection.trusses = m_dragTrussUuids;
        const auto targets = scene_grouping::BuildInteractiveTransformTargets(
        ConfigManager::Get().GetScene(), selection,
        selection_movement_settings::LoadInteractiveTransformPolicy(
            ConfigManager::Get()));
    if (targets.size() == 1 && targets.front().type == MvrNodeType::GroupObject)
            return magnet_snap::SnapSource{magnet_snap::ObjectType::TrussGroup,
                                           targets.front().uuid};
        if (m_dragTrussUuids.size() == 1 && m_dragFixtureUuids.empty())
            return magnet_snap::SnapSource{magnet_snap::ObjectType::Truss,
                                           m_dragTrussUuids.front()};
    }
    if (m_dragFixtureUuids.size() == 1 && m_dragTrussUuids.empty() &&
        m_dragSceneObjectUuids.empty())
        return magnet_snap::SnapSource{magnet_snap::ObjectType::Fixture,
                                       m_dragFixtureUuids.front()};
    if (m_dragSceneObjectUuids.size() == 1 && m_dragFixtureUuids.empty() &&
        m_dragTrussUuids.empty())
        return magnet_snap::SnapSource{magnet_snap::ObjectType::SceneObject,
                                       m_dragSceneObjectUuids.front()};
    return std::nullopt;
}

// Builds camera-weighted Magnet settings for the active 3D view.
magnet_snap::SnapSettings Viewer3DPanel::BuildActiveMagnetSettings(
    const magnet_snap::SnapSource &source) const {
    magnet_snap::SnapSettings settings;
    settings.candidateResolver = &m_trussCandidateResolver;
    settings.thresholdMm = source.type == magnet_snap::ObjectType::Fixture
                               ? magnet_snap::kDefaultSnapDistanceMm * 2.0f
                               : magnet_snap::kDefaultSnapDistanceMm;

    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kMinimumViewAxisWeight = 0.2f;
    const float yawRad = m_camera.GetYaw() * kPi / 180.0f;
    const float pitchRad = m_camera.GetPitch() * kPi / 180.0f;
    const std::array<float, 3> forward = {
        -std::cos(pitchRad) * std::sin(yawRad),
        std::cos(pitchRad) * std::cos(yawRad),
        -std::sin(pitchRad),
    };
    for (int axis = 0; axis < 3; ++axis)
    settings.axisWeights[axis] =
        std::max(kMinimumViewAxisWeight, 1.0f - std::fabs(forward[axis]));
    if (source.type == magnet_snap::ObjectType::Truss ||
        source.type == magnet_snap::ObjectType::TrussGroup) {
        truss_screen_snap::ProjectionSnapshot snapshot;
        GLdouble modelView[16] = {};
        GLdouble projection[16] = {};
        GLint viewport[4] = {};
        glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
        glGetDoublev(GL_PROJECTION_MATRIX, projection);
        glGetIntegerv(GL_VIEWPORT, viewport);
        std::copy(std::begin(modelView), std::end(modelView),
                  snapshot.modelView.begin());
        std::copy(std::begin(projection), std::end(projection),
                  snapshot.projection.begin());
        std::copy(std::begin(viewport), std::end(viewport),
                  snapshot.viewport.begin());
        snapshot.contentScale =
            std::max(1.0, static_cast<double>(GetContentScaleFactor()));
        settings.trussProjection = snapshot;
    }
    return settings;
}

// Finds the current Magnet snap candidate for the active 3D drag.
std::optional<magnet_snap::SnapResult>
Viewer3DPanel::FindActiveMagnetSnap() const {
    auto source = BuildActiveMagnetSource();
    if (!source)
        return std::nullopt;
    return magnet_snap::FindSnap(ConfigManager::Get().GetScene(), *source,
                                 BuildActiveMagnetSettings(*source));
}

// Restores the raw mouse-following transform before applying the next 3D drag delta.
std::optional<magnet_snap::SnapResult> Viewer3DPanel::RestorePendingMagnetSnapPreview()
{
    if (!m_pendingMagnetSnap)
        return std::nullopt;
    magnet_snap::SnapResult previous = *m_pendingMagnetSnap;
    magnet_snap::SnapResult inverse = previous;
    for (float& component : inverse.translationDeltaMm)
        component = -component;
    ConfigManager& cfg = ConfigManager::Get();
    magnet_snap::ApplySnapTransform(
        cfg.GetScene(), inverse,
        selection_movement_settings::LoadInteractiveTransformPolicy(cfg));
    m_selectionDragAnchorMeters[0] += inverse.translationDeltaMm[0] / 1000.0f;
    m_selectionDragAnchorMeters[1] += inverse.translationDeltaMm[1] / 1000.0f;
    m_selectionDragAnchorMeters[2] += inverse.translationDeltaMm[2] / 1000.0f;
    m_pendingMagnetSnap.reset();
    return previous;
}

// Commits deferred Magnet grouping after a successful 3D snap.
void Viewer3DPanel::CommitActiveMagnetSnap() {
    if (!m_pendingMagnetSnap || !m_pendingMagnetSnap->needsGrouping)
        return;
    ConfigManager& cfg = ConfigManager::Get();
    magnet_snap::ApplyCommittedSnapGrouping(cfg.GetScene(), *m_pendingMagnetSnap);
}

// Applies a world-space delta to every object in the active selection drag.
void Viewer3DPanel::ApplySelectionDragDelta(
    const std::array<float, 3> &deltaMeters) {
    if (!std::isfinite(deltaMeters[0]) || !std::isfinite(deltaMeters[1]) ||
        !std::isfinite(deltaMeters[2])) {
    viewer3d::diagnostics::Log(
        "Selection dragging ignored non-finite world delta.");
        wxASSERT_MSG(false, "Non-finite selection drag delta.");
        return;
    }
    viewer3d::diagnostics::Logf("Selection dragging delta=(%.5f, %.5f, %.5f)",
                                deltaMeters[0], deltaMeters[1], deltaMeters[2]);
    const float dxMm = deltaMeters[0] * 1000.0f;
    const float dyMm = deltaMeters[1] * 1000.0f;
    const float dzMm = deltaMeters[2] * 1000.0f;
    ConfigManager& cfg = ConfigManager::Get();
    const bool hasTranslation =
        dxMm != 0.0f || dyMm != 0.0f || dzMm != 0.0f;
    if (hasTranslation && !m_selectionDragUndoPushed) {
        cfg.PushUndoState("move selection");
        m_selectionDragUndoPushed = true;
    }

    scene_grouping::ObjectSelection selection;
    selection.fixtures = m_dragFixtureUuids;
    selection.trusses = m_dragTrussUuids;
    selection.sceneObjects = m_dragSceneObjectUuids;
    const auto previousSnap = RestorePendingMagnetSnapPreview();
  const auto policy =
      selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
  if (hasTranslation) {
    scene_grouping::TranslateSelection(
        cfg.GetScene(), selection, {dxMm, dyMm, dzMm},
        transform_space::TransformSpace::World, policy);
  }
    if (auto snap = FindActiveMagnetSnap()) {
    magnet_snap::ApplySnapTransform(cfg.GetScene(), *snap, policy);
        m_pendingMagnetSnap = snap;
        m_selectionDragAnchorMeters[0] += snap->translationDeltaMm[0] / 1000.0f;
        m_selectionDragAnchorMeters[1] += snap->translationDeltaMm[1] / 1000.0f;
        m_selectionDragAnchorMeters[2] += snap->translationDeltaMm[2] / 1000.0f;
    } else if (previousSnap) {
        magnet_snap::DetachSnapSourceFromGroup(cfg.GetScene(), *previousSnap);
    }
    m_selectionDragAnchorMeters[0] += deltaMeters[0];
    m_selectionDragAnchorMeters[1] += deltaMeters[1];
    m_selectionDragAnchorMeters[2] += deltaMeters[2];
    UpdateSelectionDragStatusPosition();
}

// Updates the status bar with the active selection drag insertion point.
void Viewer3DPanel::UpdateSelectionDragStatusPosition() {
    if (MainWindow::Instance())
        MainWindow::Instance()->UpdateHighlightedWorldPositionInStatusBar(
            std::optional<std::array<float, 3>>(m_selectionDragAnchorMeters));
}

// Finalizes a completed selection drag and synchronizes dependent views.
void Viewer3DPanel::FinalizeSelectionDrag() {
  viewer3d::diagnostics::Logf(
      "Selection dragging finalize moved=%d fixtures=%zu trusses=%zu "
      "objects=%zu",
                                m_selectionDragMoved ? 1 : 0, m_dragFixtureUuids.size(),
                                m_dragTrussUuids.size(), m_dragSceneObjectUuids.size());
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

// Starts moving a newly created scene element with the pointer until it is placed.
void Viewer3DPanel::BeginContinuousPlacement(
    ContinuousPlacementType type, const std::string& elementUuid)
{
    ConfigManager& cfg = ConfigManager::Get();
    if (!continuous_placement::Contains(cfg.GetScene(), type, elementUuid))
        return;

    ResetSelectionDragState();
    m_continuousPlacementActive = true;
    m_continuousPlacementType = type;
    m_placementViewRevision.Invalidate();
    m_continuousPlacementUuid = elementUuid;
    m_continuousPlacedUuids.clear();
    m_selectionDragArmed = true;
    m_selectionDragUndoPushed = true;
  m_selectionDragTarget =
      type == ContinuousPlacementType::Fixture ? HoverTargetTable::Fixtures
      : type == ContinuousPlacementType::Truss ? HoverTargetTable::Trusses
                                : HoverTargetTable::SceneObjects;
    m_dragSelectionUuids = {elementUuid};
    m_dragFixtureUuids = type == ContinuousPlacementType::Fixture
                             ? std::vector<std::string>{elementUuid}
                             : std::vector<std::string>{};
    m_dragTrussUuids = type == ContinuousPlacementType::Truss
                           ? std::vector<std::string>{elementUuid}
                           : std::vector<std::string>{};
    m_dragSceneObjectUuids = type == ContinuousPlacementType::SceneObject
                                 ? std::vector<std::string>{elementUuid}
                                 : std::vector<std::string>{};
    m_selectionDragAnchorMeters = continuous_placement::PositionMeters(
        cfg.GetScene(), type, elementUuid);
    SetFocus();
    UpdateSelectionDragStatusPosition();
    Refresh();
}

// Commits the current element and creates the next pointer-driven copy.
void Viewer3DPanel::ConfirmContinuousPlacement()
{
    ConfigManager& cfg = ConfigManager::Get();
    if (!continuous_placement::Contains(cfg.GetScene(),
                                        m_continuousPlacementType,
                                        m_continuousPlacementUuid)) {
        CancelContinuousPlacement();
        return;
    }

    auto nextRawPosition = continuous_placement::PositionMeters(
        cfg.GetScene(), m_continuousPlacementType, m_continuousPlacementUuid);
    if (m_pendingMagnetSnap) {
        nextRawPosition = continuous_placement::RawAnchorFromPreview(
            nextRawPosition, m_pendingMagnetSnap->translationDeltaMm);
    }
    cfg.PushUndoState(std::string("place ") +
                      continuous_placement::ElementName(
                          m_continuousPlacementType));
    m_continuousPlacedUuids.push_back(m_continuousPlacementUuid);
    const std::string nextUuid =
        wxString::Format("uuid_%lld", static_cast<long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()))
            .ToStdString();
    if (!continuous_placement::CloneElement(
            cfg.GetScene(), m_continuousPlacementType,
            m_continuousPlacementUuid, nextUuid)) {
        CancelContinuousPlacement();
        return;
    }
    continuous_placement::SetPositionMeters(
        cfg.GetScene(), m_continuousPlacementType, nextUuid, nextRawPosition);
    CommitActiveMagnetSnap();
    const auto placedUuids = m_continuousPlacedUuids;
    BeginContinuousPlacement(m_continuousPlacementType, nextUuid);
    m_continuousPlacedUuids = placedUuids;
    if (m_hasLastMousePos)
        AlignContinuousElementToPointer(m_lastMousePos);
    RefreshContinuousPlacementViews();
}

// Removes the uncommitted element and ends continuous placement.
void Viewer3DPanel::CancelContinuousPlacement()
{
    RestorePendingMagnetSnapPreview();
    ConfigManager& cfg = ConfigManager::Get();
    continuous_placement::EraseElement(cfg.GetScene(),
                                       m_continuousPlacementType,
                                       m_continuousPlacementUuid);
    if (m_continuousPlacedUuids.empty()) {
        if (cfg.CanUndo())
            cfg.Undo();
    } else {
        const MvrScene finalScene = cfg.GetScene();
        for (size_t i = 0; i <= m_continuousPlacedUuids.size() &&
                           cfg.CanUndo();
             ++i) {
            cfg.Undo();
        }
        cfg.PushUndoState(std::string("continuous ") +
                          continuous_placement::ElementName(
                              m_continuousPlacementType) +
                          " placement");
        cfg.GetScene() = finalScene;
    }
    EndContinuousPlacementState();
    RefreshContinuousPlacementViews();
}

// Undoes one confirmed element while keeping the placement session active.
bool Viewer3DPanel::UndoContinuousPlacement() {
    if (!m_continuousPlacementActive)
        return false;

    ConfigManager& cfg = ConfigManager::Get();
    RestorePendingMagnetSnapPreview();
    if (m_continuousPlacedUuids.empty()) {
        if (cfg.CanUndo())
            cfg.Undo();
        EndContinuousPlacementState();
        RefreshContinuousPlacementViews();
        return true;
    }
    if (m_continuousPlacedUuids.size() == 1) {
        if (cfg.CanUndo())
            cfg.Undo();
        if (cfg.CanUndo())
            cfg.Undo();
        EndContinuousPlacementState();
        RefreshContinuousPlacementViews();
        return true;
    }

    const std::string restoredUuid = m_continuousPlacedUuids.back();
    m_continuousPlacedUuids.pop_back();
    if (cfg.CanUndo())
        cfg.Undo();
  if (!continuous_placement::Contains(cfg.GetScene(), m_continuousPlacementType,
                                        restoredUuid)) {
        EndContinuousPlacementState();
        RefreshContinuousPlacementViews();
        return true;
    }
    const auto placedUuids = m_continuousPlacedUuids;
    BeginContinuousPlacement(m_continuousPlacementType, restoredUuid);
    m_continuousPlacedUuids = placedUuids;
    RefreshContinuousPlacementViews();
    return true;
}

// Clears placement-only interaction state without changing the scene.
void Viewer3DPanel::EndContinuousPlacementState() {
    m_continuousPlacementActive = false;
    m_continuousPlacementType = ContinuousPlacementType::None;
    m_continuousPlacementUuid.clear();
    m_continuousPlacedUuids.clear();
    ResetSelectionDragState();
}

// Synchronizes tables and both viewers after a placement history change.
void Viewer3DPanel::RefreshContinuousPlacementViews() {
    if (MainWindow::Instance()) {
        MainWindow::Instance()->RefreshAfterToolSceneUpdate();
        return;
    }
    UpdateScene();
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->UpdateScene();
    Refresh();
}

// Draws the axis gizmo for an armed or active selection drag.
void Viewer3DPanel::DrawSelectionDragGizmo(const RenderSize &renderSize) {
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

    glLineWidth(2.5f);
    glDisable(GL_DEPTH_TEST);
    const float gizmoLength = std::max(0.4f, m_camera.GetDistance() * 0.08f);
    const float arrowheadLength = gizmoLength * 0.22f;
    const float arrowheadRadius = gizmoLength * 0.055f;
    const float shaftLength = gizmoLength - arrowheadLength;
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
    const auto axis =
        GetSelectionDragAxisVector(i == 0   ? viewer3d::SelectionDragAxis::X
                   : i == 1 ? viewer3d::SelectionDragAxis::Y
                            : viewer3d::SelectionDragAxis::Z);
        glColor4f(colors[i][0], colors[i][1], colors[i][2], colors[i][3]);
        glVertex3f(m_selectionDragAnchorMeters[0], m_selectionDragAnchorMeters[1],
                   m_selectionDragAnchorMeters[2]);
        glVertex3f(m_selectionDragAnchorMeters[0] + axis[0] * shaftLength,
                   m_selectionDragAnchorMeters[1] + axis[1] * shaftLength,
                   m_selectionDragAnchorMeters[2] + axis[2] * shaftLength);
    }
    glEnd();

    for (size_t i = 0; i < colors.size(); ++i) {
    const auto axis =
        GetSelectionDragAxisVector(i == 0   ? viewer3d::SelectionDragAxis::X
                   : i == 1 ? viewer3d::SelectionDragAxis::Y
                            : viewer3d::SelectionDragAxis::Z);
        const std::array<float, 3> arrowheadBase{
            m_selectionDragAnchorMeters[0] + axis[0] * shaftLength,
            m_selectionDragAnchorMeters[1] + axis[1] * shaftLength,
            m_selectionDragAnchorMeters[2] + axis[2] * shaftLength,
        };
        glColor4f(colors[i][0], colors[i][1], colors[i][2], colors[i][3]);
        DrawSelectionDragArrowhead(arrowheadBase, axis, arrowheadLength,
                                   arrowheadRadius);
    }
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Applies the standard orbit or pan response for an active camera drag.
void Viewer3DPanel::ApplyCameraDrag(const wxMouseEvent& event,
                                    const wxPoint& mousePos)
{
    const int dx = mousePos.x - m_lastMousePos.x;
    const int dy = mousePos.y - m_lastMousePos.y;
    if (dx == 0 && dy == 0)
        return;

    m_draggedSincePress = true;
    m_isInteracting = true;
    m_cameraMoving = true;
    m_lastInteractionTime = std::chrono::steady_clock::now();

    if (m_mode == InteractionMode::Orbit &&
        (event.LeftIsDown() || event.RightIsDown())) {
    const float orbitPitchDirection = IsOrbitInversionEnabled() ? 1.0f : -1.0f;
        m_camera.Orbit(static_cast<float>(dx) * 0.5f,
                       orbitPitchDirection * static_cast<float>(dy) * 0.5f);
    } else if (m_mode == InteractionMode::Pan &&
               (event.MiddleIsDown() || event.RightIsDown() ||
                event.ShiftDown())) {
        m_camera.Pan(-dx * 0.01f, dy * 0.01f);
    }
    m_lastMousePos = mousePos;
    m_placementViewRevision.Invalidate();
}

// Aligns the provisional fixture with the raw view-plane position under the
// pointer.
bool Viewer3DPanel::AlignContinuousElementToPointer(const wxPoint &mousePos) {
    const RenderSize renderSize = ResolveRenderSize(this);
    if (!renderSize.IsValid() ||
        !TryBindGlContextForInteraction("continuous element alignment")) {
        return false;
    }

    ApplyCameraMatrices(renderSize);
    const auto rawAnchor = CurrentRawSelectionDragAnchor();
    const auto pointer =
        ProjectMouseToSelectionDragViewPlane(mousePos, renderSize, rawAnchor);
    if (!pointer)
        return false;

    RestorePendingMagnetSnapPreview();
    ApplySelectionDragDelta(continuous_placement::AbsoluteAlignmentDelta(
        *pointer, m_selectionDragAnchorMeters));
    m_placementViewRevision.CompleteAlignmentAttempt(true);
    m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
    m_selectionDragMoved = true;
    m_lastMousePos = mousePos;
    return true;
}

// Handles mouse movement for placement, selection, orbit, and pan.
void Viewer3DPanel::OnMouseMove(wxMouseEvent &event) {
    m_hasLastMousePos = true;
    wxPoint pos = event.GetPosition();
    if (m_continuousPlacementActive) {
        if (m_dragging && event.Dragging()) {
            ApplyCameraDrag(event, pos);
            Refresh();
            return;
        }
        if (m_placementViewRevision.NeedsAlignment()) {
            m_lastMousePos = pos;
            AlignContinuousElementToPointer(pos);
            Refresh();
            return;
        }
        const RenderSize renderSize = ResolveRenderSize(this);
        if (renderSize.IsValid() &&
            TryBindGlContextForInteraction("continuous element placement")) {
            ApplyCameraMatrices(renderSize);
            const auto rawAnchor = CurrentRawSelectionDragAnchor();
            const int dx = pos.x - m_lastMousePos.x;
            const int dy = pos.y - m_lastMousePos.y;
            if (m_axisConstrainedMovementEnabled) {
                const auto projectedAxes =
                    BuildProjectedDragAxes(renderSize, rawAnchor);
                const bool hasValidAxis = std::any_of(
                    projectedAxes.begin(), projectedAxes.end(),
                    [](const viewer3d::ProjectedAxis &axis) {
                        return axis.valid;
                    });
                if (!hasValidAxis) {
                    m_lastMousePos = pos;
                    m_placementViewRevision.Invalidate();
                    Refresh();
                    return;
                }
        if (m_selectionDragAxis == viewer3d::SelectionDragAxis::None &&
                    (std::abs(dx) >= kSelectionDragStartThresholdPx ||
                     std::abs(dy) >= kSelectionDragStartThresholdPx)) {
                    m_selectionDragAxis =
              viewer3d::SelectDragAxisFromMouseDelta(dx, -dy, projectedAxes);
                }
        const double axisDeltaMeters = viewer3d::ComputeDragMetersOnAxis(
                        dx, -dy, m_selectionDragAxis, projectedAxes);
                if (axisDeltaMeters != 0.0) {
          const auto axis = GetSelectionDragAxisVector(m_selectionDragAxis);
                    ApplySelectionDragDelta(
                        {axis[0] * static_cast<float>(axisDeltaMeters),
                         axis[1] * static_cast<float>(axisDeltaMeters),
                         axis[2] * static_cast<float>(axisDeltaMeters)});
                    m_selectionDragMoved = true;
                }
            } else {
        m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
                const auto lastPoint =
            ProjectMouseToSelectionDragViewPlane(
                m_lastMousePos, renderSize, rawAnchor);
                const auto currentPoint =
                    ProjectMouseToSelectionDragViewPlane(
                        pos, renderSize, rawAnchor);
                if (lastPoint && currentPoint) {
          ApplySelectionDragDelta({(*currentPoint)[0] - (*lastPoint)[0],
                         (*currentPoint)[1] - (*lastPoint)[1],
                         (*currentPoint)[2] - (*lastPoint)[2]});
                    m_selectionDragMoved = true;
                } else {
                    m_lastMousePos = pos;
                    m_placementViewRevision.Invalidate();
                    Refresh();
                    return;
                }
            }
            Refresh();
        } else {
            m_placementViewRevision.Invalidate();
        }
        m_lastMousePos = pos;
        return;
    }
  viewer3d::diagnostics::Logf(
      "Mouse move pos=(%d,%d) dragging=%d selectionDrag=%d rect=%d", pos.x,
      pos.y, event.Dragging() ? 1 : 0, m_selectionDragArmed ? 1 : 0,
      m_rectSelecting ? 1 : 0);
  if (m_measureToolEnabled && m_measureHasAnchor &&
      !m_measureHasCommittedTarget) {
        m_measurePreviewMousePos = pos;
        m_measureHasPreviewMousePos = true;
    }

  if (m_rectSelecting && event.Dragging()) {
        m_rectSelectEnd = pos;
        m_draggedSincePress = true;
        Refresh();
        return;
    }

  if (m_selectionDragArmed && event.Dragging() && event.LeftIsDown()) {
        if ((wxGetLocalTimeMillis() - m_selectionDragPressTime).ToLong() <
            kSelectionDragDelayMs) {
            m_lastMousePos = pos;
            return;
        }

        const int dx = pos.x - m_lastMousePos.x;
        const int dy = pos.y - m_lastMousePos.y;
    if (!m_selectionDragMoved &&
        std::abs(dx) < kSelectionDragStartThresholdPx &&
            std::abs(dy) < kSelectionDragStartThresholdPx)
            return;
        if (dx != 0 || dy != 0) {
            const RenderSize renderSize = ResolveRenderSize(this);
            if (renderSize.IsValid() &&
                TryBindGlContextForInteraction("OnMouseMove")) {
                ApplyCameraMatrices(renderSize);
                if (m_axisConstrainedMovementEnabled) {
                    const auto projectedAxes = BuildProjectedDragAxes(renderSize, CurrentRawSelectionDragAnchor());
                    if (m_selectionDragAxis == viewer3d::SelectionDragAxis::None) {
            m_selectionDragAxis =
                viewer3d::SelectDragAxisFromMouseDelta(dx, -dy, projectedAxes);
                    }

                    const double axisDeltaMeters = viewer3d::ComputeDragMetersOnAxis(
                        dx, -dy, m_selectionDragAxis, projectedAxes);
                    if (axisDeltaMeters != 0.0) {
                        const auto axisVector =
                            GetSelectionDragAxisVector(m_selectionDragAxis);
                        const std::array<float, 3> worldDelta{
                            axisVector[0] * static_cast<float>(axisDeltaMeters),
                            axisVector[1] * static_cast<float>(axisDeltaMeters),
                            axisVector[2] * static_cast<float>(axisDeltaMeters)};
                        ApplySelectionDragDelta(worldDelta);
                        m_selectionDragMoved = true;
                        m_draggedSincePress = true;
                    }
                } else {
                    const auto rawAnchor = CurrentRawSelectionDragAnchor();
                    const auto lastPoint = ProjectMouseToSelectionDragViewPlane(
                        m_lastMousePos, renderSize, rawAnchor);
                    const auto currentPoint = ProjectMouseToSelectionDragViewPlane(
                        pos, renderSize, rawAnchor);
                    if (lastPoint && currentPoint) {
                        const std::array<float, 3> worldDelta{
                            (*currentPoint)[0] - (*lastPoint)[0],
                            (*currentPoint)[1] - (*lastPoint)[1],
                            (*currentPoint)[2] - (*lastPoint)[2]};
                        ApplySelectionDragDelta(worldDelta);
                        m_selectionDragAxis = viewer3d::SelectionDragAxis::None;
                        m_selectionDragMoved = true;
                        m_draggedSincePress = true;
                    }
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
        ApplyCameraDrag(event, pos);
    else
        m_lastMousePos = pos;

    // Mark that the mouse has moved so OnPaint can update hover info
    m_mouseMoved = true;
    m_forceHoverQuery = true;

    Refresh();
}

// Handles mouse wheel (zoom)
void Viewer3DPanel::OnMouseWheel(wxMouseEvent &event) {
    m_hasLastMousePos = true;
    viewer3d::diagnostics::Logf("Mouse wheel start rotation=%d delta=%d",
                                event.GetWheelRotation(), event.GetWheelDelta());
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
    if (steps == 0.0f || !std::isfinite(steps)) {
    viewer3d::diagnostics::Log(
        "Mouse wheel ignored because zoom steps are invalid or zero.");
        return;
    }
    m_controller.SetInteracting(true);
    m_isInteracting = true;
    m_cameraMoving = true;
    m_lastInteractionTime = std::chrono::steady_clock::now();

    m_camera.Zoom(steps);
    m_placementViewRevision.Invalidate();
    if (m_continuousPlacementActive)
        AlignContinuousElementToPointer(event.GetPosition());
    ArmZoomInteractionTimeout();

    Refresh();
}

// Opens an editor for the item under the double-click position.
void Viewer3DPanel::OnMouseDClick(wxMouseEvent &event) {
    const RenderSize renderSize = ResolveRenderSize(this);
    const int w = renderSize.width;
    const int h = renderSize.height;
    if (!renderSize.IsValid())
        return;
    if (!TryBindGlContextForInteraction("OnMouseDClick"))
        return;
    std::string uuid;
    const wxPoint pickPos = ToFramebufferPoint(this, event.GetPosition());
    const bool cameraWasMoving = m_cameraMoving;
    if (cameraWasMoving)
        m_controller.SetCameraMoving(false);

    const bool sceneObjectsActive =
        SceneObjectTablePanel::Instance() &&
        SceneObjectTablePanel::Instance()->IsActivePage();

  const bool foundSceneObject =
      sceneObjectsActive &&
      QueryHoverUuid(m_controller, HoverTargetTable::SceneObjects, pickPos.x,
                     pickPos.y, w, h, uuid);

    bool found = foundSceneObject;
    if (!found)
    found = QueryHoverUuid(m_controller, HoverTargetTable::Fixtures, pickPos.x,
                           pickPos.y, w, h, uuid);

    if (cameraWasMoving)
        m_controller.SetCameraMoving(true);

    if (!found && !m_hoverUuid.empty())
        uuid = m_hoverUuid;
    if (uuid.empty())
        return;

    ConfigManager &cfg = ConfigManager::Get();
    if (foundSceneObject) {
    const bool edited =
        scene_object_primitives::EditPrimitiveObjectByUuid(this, cfg, uuid);
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
void Viewer3DPanel::OnKeyDown(wxKeyEvent &event) {
    viewer3d::diagnostics::Logf("Key interaction start key=%d shift=%d alt=%d",
                                event.GetKeyCode(), event.ShiftDown() ? 1 : 0,
                                event.AltDown() ? 1 : 0);
  if (!m_mouseInside && !HasFocus()) {
    event.Skip();
    return;
  }
    if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
        event.Skip();
        return;
    }

    bool shift = event.ShiftDown();
    bool alt = event.AltDown();
    bool zoomTriggered = false;

    switch (event.GetKeyCode()) {
        case WXK_ESCAPE:
            if (m_continuousPlacementActive) {
                CancelContinuousPlacement();
                return;
            }
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
                m_camera.Orbit(-5.0f, 0.0f);
            break;
        case WXK_RIGHT:
            if (shift)
                m_camera.Pan(0.1f, 0.0f);
            else if (alt) {
                m_camera.Zoom(1.0f);
                zoomTriggered = true;
            } else
                m_camera.Orbit(5.0f, 0.0f);
            break;
        case WXK_UP:
            if (shift)
                m_camera.Pan(0.0f, 0.1f);
            else if (alt) {
                m_camera.Zoom(-1.0f);
                zoomTriggered = true;
            } else
                m_camera.Orbit(0.0f, 5.0f);
            break;
        case WXK_DOWN:
            if (shift)
                m_camera.Pan(0.0f, -0.1f);
            else if (alt) {
                m_camera.Zoom(1.0f);
                zoomTriggered = true;
            } else
                m_camera.Orbit(0.0f, -5.0f);
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
    m_placementViewRevision.Invalidate();
    if (m_continuousPlacementActive && m_hasLastMousePos)
        AlignContinuousElementToPointer(m_lastMousePos);

    viewer3d::diagnostics::Log("Key interaction end.");
    Refresh();
}

// Resets the camera to its default isometric view.
bool Viewer3DPanel::ResetCameraToIsometric() {
    m_camera.Reset();
    m_placementViewRevision.Invalidate();
    Refresh();
    return true;
}

// Frames all visible scene content inside the current camera view.
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
    m_placementViewRevision.Invalidate();
    Refresh();
    return true;
}

// Applies a standard orthographic-style orientation to the 3D camera.
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
    m_placementViewRevision.Invalidate();
    Refresh();
}

// Marks the mouse as inside the 3D panel and focuses keyboard input.
void Viewer3DPanel::OnMouseEnter(wxMouseEvent &event) {
    m_mouseInside = true;
    SetFocus();
    event.Skip();
}

// Clears hover and highlight state when the mouse leaves the 3D panel.
void Viewer3DPanel::OnMouseLeave(wxMouseEvent &event) {
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
    if (HoistTablePanel::Instance())
        HoistTablePanel::Instance()->HighlightHoist(std::string());
    if (SceneObjectTablePanel::Instance())
        SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    Refresh();
    event.Skip();
}

// Updates scene resources only when the 3D canvas is fully shown and its GL
// context can be safely activated.
void Viewer3DPanel::UpdateScene() {
    ++m_sceneRevision;
    m_controller.MarkResourceSyncPending();

    if (ShouldPauseHeavyTasks() || m_cameraMoving)
        return;

    if (!PrepareGlResourceSync("UpdateScene"))
        return;
    if (m_controller.ConsumeResourceSyncPending())
        m_controller.UpdateResourcesIfDirty();
    if (Viewer2DPanel::Instance())
        Viewer2DPanel::Instance()->UpdateScene();
}

// Releases controller references to scene entries before a project is replaced.
void Viewer3DPanel::PrepareForSceneReplacement() {
    m_controller.PrepareForSceneReplacement();
}

// Resumes controller synchronization after a replacement scene is installed.
void Viewer3DPanel::CompleteSceneReplacement() {
    m_controller.CompleteSceneReplacement();
}

// Applies a new selected UUID set to the 3D controller and schedules a refresh.
void Viewer3DPanel::SetSelectedFixtures(const std::vector<std::string> &uuids) {
  const scene_grouping::ObjectSelection typedSelection{.fixtures = uuids,
                                                       .trusses = uuids,
                                                       .supports = uuids,
        .sceneObjects = uuids};
    const std::vector<std::string> expandedUuids =
        scene_grouping::ExpandSelectionForGroupHighlights(
          ConfigManager::Get().GetScene(), typedSelection,
          selection_movement_settings::LoadInteractiveTransformPolicy(
              ConfigManager::Get()));
    if (expandedUuids == m_lastAppliedSelectionUuids &&
        uuids == m_lastAppliedPrimarySelectionUuids)
        return;
    m_lastAppliedSelectionUuids = expandedUuids;
    m_lastAppliedPrimarySelectionUuids = uuids;
    ++m_selectionRevision;
    m_selectionRefreshPending = true;
    m_controller.SetSelectedUuids(expandedUuids, uuids);
    Refresh();
}

// Updates the cached layer color used by 3D rendering.
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

// Returns the singleton 3D panel instance used by legacy UI integrations.
Viewer3DPanel* Viewer3DPanel::Instance()
{
    return s_instance;
}

// Updates the singleton 3D panel instance used by legacy UI integrations.
void Viewer3DPanel::SetInstance(Viewer3DPanel* panel)
{
    s_instance = panel;
}

// Posts coalesced refresh notifications while the viewer needs runtime updates.
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
        if (!m_refreshEventPending.exchange(true, std::memory_order_acq_rel)) {
            wxThreadEvent* evt = new wxThreadEvent(wxEVT_VIEWER_REFRESH);
            wxQueueEvent(this, evt);
        }
        std::this_thread::sleep_for(16ms);
    }
}

// Handles a coalesced refresh notification from the refresh worker.
void Viewer3DPanel::OnThreadRefresh(wxThreadEvent &event) {
    (void)event;
    m_refreshEventPending.store(false, std::memory_order_release);
    if (m_shuttingDown || m_modalDialogActive || !m_glContext || IsBeingDeleted())
        return;

    const size_t cameraFingerprint = ComputeCameraFingerprint(m_camera);
  const bool cameraChanged = !m_hasLastThreadCameraFingerprint ||
        cameraFingerprint != m_lastThreadCameraFingerprint;
    if (cameraChanged) {
        m_lastThreadCameraFingerprint = cameraFingerprint;
        m_hasLastThreadCameraFingerprint = true;
    }

    const bool hasRelevantVisualChange =
      cameraChanged || m_controller.IsResourceSyncPending() ||
      m_selectionRefreshPending || m_highlightRefreshPending || m_mouseMoved ||
      m_forceHoverQuery || m_rectSelecting || m_dragging || m_isInteracting ||
        m_cameraMoving;
    if (!hasRelevantVisualChange)
        return;

    if (m_controller.IsResourceSyncPending() && !m_cameraMoving) {
        const auto now = std::chrono::steady_clock::now();
        const bool syncCadenceDue =
            (now - m_lastResourceSyncCheck) >= kResourceSyncInterval;
        if (syncCadenceDue) {
            if (!PrepareGlResourceSync("OnThreadRefresh"))
                return;
            m_lastResourceSyncCheck = now;
            if (m_controller.ConsumeResourceSyncPending())
                m_controller.UpdateResourcesIfDirty();
        }
    }

    viewer3d::diagnostics::Log("Thread refresh requested panel repaint.");
    Refresh();
}

// Records whether a modal dialog should pause 3D refresh work.
void Viewer3DPanel::SetModalDialogActive(bool active) {
    m_modalDialogActive = active;
}

// Returns whether expensive scene work should pause during active navigation.
bool Viewer3DPanel::ShouldPauseHeavyTasks() {
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

// Arms the timeout that ends wheel-driven zoom interaction.
void Viewer3DPanel::ArmZoomInteractionTimeout() {
    m_zoomInteractionTimer.StartOnce(kZoomInteractionTimeoutMs);
}

// Ends wheel-driven zoom interaction after input has gone idle.
void Viewer3DPanel::OnZoomInteractionTimeout(wxTimerEvent &event) {
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

// Loads persisted camera state from configuration into the 3D camera.
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
    msg.Printf("Camera loaded: yaw=%.2f pitch=%.2f dist=%.2f target=(%.2f, "
               "%.2f, %.2f)",
            yaw, pitch, dist, tx, ty, tz);
        ConsolePanel::Instance()->AppendMessage(msg);
    }
}
