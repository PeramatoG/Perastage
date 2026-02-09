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
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "fixturepatchdialog.h"
#include "viewer2dpanel.h"
#include <wx/dcclient.h>
#include <wx/event.h>
#include <wx/log.h>
#include <chrono>
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
                 wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
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
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
}

// Paint event handler
void Viewer3DPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    if (!IsShownOnScreen()) {
        return;
    }
    InitGL();

    const bool wasInteracting = m_isInteracting;
    const bool pauseHeavyTasks = ShouldPauseHeavyTasks();
    if (wasInteracting && !pauseHeavyTasks)
        m_controller.Update();

    Render();

    // Ensure the OpenGL context is current before drawing overlays
    SetCurrent(*m_glContext);

    int w, h;
    GetClientSize(&w, &h);

    wxString newLabel;
    wxPoint newPos;
    std::string newUuid;
    bool found = false;

    if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage()) {
        found = m_controller.GetFixtureLabelAt(m_lastMousePos.x, m_lastMousePos.y,
            w, h, newLabel, newPos, &newUuid);
        if (found) {
            if (TrussTablePanel::Instance())
                TrussTablePanel::Instance()->HighlightTruss(std::string());
            if (SceneObjectTablePanel::Instance())
                SceneObjectTablePanel::Instance()->HighlightObject(std::string());
        }
    }
    else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage()) {
        found = m_controller.GetTrussLabelAt(m_lastMousePos.x, m_lastMousePos.y,
            w, h, newLabel, newPos, &newUuid);
        if (found) {
            if (FixtureTablePanel::Instance())
                FixtureTablePanel::Instance()->HighlightFixture(std::string());
            if (SceneObjectTablePanel::Instance())
                SceneObjectTablePanel::Instance()->HighlightObject(std::string());
        }
    }
    else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage()) {
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
    else if (!m_hasHover || m_mouseMoved) {
        m_hasHover = false;
        m_controller.SetHighlightUuid("");
        if (FixtureTablePanel::Instance())
            FixtureTablePanel::Instance()->HighlightFixture(std::string());
        if (TrussTablePanel::Instance())
            TrussTablePanel::Instance()->HighlightTruss(std::string());
        if (SceneObjectTablePanel::Instance())
            SceneObjectTablePanel::Instance()->HighlightObject(std::string());
    }
    m_mouseMoved = false;

    // Draw labels before swapping buffers to avoid losing them
    if (FixtureTablePanel::Instance() && FixtureTablePanel::Instance()->IsActivePage())
        m_controller.DrawFixtureLabels(w, h);
    else if (TrussTablePanel::Instance() && TrussTablePanel::Instance()->IsActivePage())
        m_controller.DrawTrussLabels(w, h);
    else if (SceneObjectTablePanel::Instance() && SceneObjectTablePanel::Instance()->IsActivePage())
        m_controller.DrawSceneObjectLabels(w, h);

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

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / height, 1.0, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    m_camera.Apply(); // Camera view

    m_controller.RenderScene();

    glFlush();
}

// Handles mouse button press
void Viewer3DPanel::OnMouseDown(wxMouseEvent& event)
{
    if (event.LeftDown() || event.MiddleDown())
    {
        if (event.LeftDown() && event.ControlDown()) {
            m_rectSelecting = true;
            m_controller.SetInteracting(true);
            m_isInteracting = true;
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
        m_lastInteractionTime = std::chrono::steady_clock::now();
        m_draggedSincePress = false;
        m_lastMousePos = event.GetPosition();
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
        m_controller.SetInteracting(false);
        m_mode = InteractionMode::None;
        m_draggedSincePress = false;
        Refresh();
        return;
    }

    if (m_dragging && (event.LeftUp() || event.MiddleUp()))
    {
        m_dragging = false;
        m_controller.SetInteracting(false);
        m_mode = InteractionMode::None;
        ReleaseMouse();
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
    if (m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label, pos, &hitUuid)) {
        event.Skip();
        return;
    }

    const auto& scene = ConfigManager::Get().GetScene();
    if (scene.fixtures.empty())
        return;

    std::set<std::string> typeNames;
    std::set<std::string> positionNames;
    bool hasNoPosition = false;
    for (const auto& [uuid, fixture] : scene.fixtures) {
        if (!fixture.typeName.empty())
            typeNames.insert(fixture.typeName);
        if (fixture.positionName.empty())
            hasNoPosition = true;
        else
            positionNames.insert(fixture.positionName);
    }

    wxMenu rootMenu;
    auto typeSubmenu = std::make_unique<wxMenu>();
    auto positionSubmenu = std::make_unique<wxMenu>();

    constexpr int kSelectTypeAllId = wxID_HIGHEST + 900;
    constexpr int kSelectTypeBaseId = wxID_HIGHEST + 901;
    constexpr int kSelectPositionNoneId = wxID_HIGHEST + 1100;
    constexpr int kSelectPositionBaseId = wxID_HIGHEST + 1101;

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

    rootMenu.AppendSubMenu(typeSubmenu.release(), "Select by fixture type");
    rootMenu.AppendSubMenu(positionSubmenu.release(), "Select by position");

    const int selectedId = GetPopupMenuSelectionFromUser(rootMenu, event.GetPosition());
    if (selectedId == wxID_NONE)
        return;

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
    m_mode = InteractionMode::None;
    m_rectSelecting = false;
    m_controller.SetInteracting(false);
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
        m_lastInteractionTime = std::chrono::steady_clock::now();

        if (m_mode == InteractionMode::Orbit && event.LeftIsDown())
        {
            m_camera.Orbit(dx * 0.5f, -dy * 0.5f);
        }
        else if (m_mode == InteractionMode::Pan && (event.MiddleIsDown() || event.ShiftDown()))
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
    m_lastInteractionTime = std::chrono::steady_clock::now();
    m_camera.Zoom(steps);
    m_controller.SetInteracting(false);
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
    if (!m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label, pos, &uuid))
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
    if (!m_mouseInside) { event.Skip(); return; }

    bool shift = event.ShiftDown();
    bool alt = event.AltDown();

    switch (event.GetKeyCode()) {
        case WXK_LEFT:
            if (shift)
                m_camera.Pan(-0.1f, 0.0f);
            else if (alt)
                m_camera.Zoom(-1.0f);
            else
                m_camera.Orbit(-5.0f, 0.0f);
            break;
        case WXK_RIGHT:
            if (shift)
                m_camera.Pan(0.1f, 0.0f);
            else if (alt)
                m_camera.Zoom(1.0f);
            else
                m_camera.Orbit(5.0f, 0.0f);
            break;
        case WXK_UP:
            if (shift)
                m_camera.Pan(0.0f, 0.1f);
            else if (alt)
                m_camera.Zoom(-1.0f);
            else
                m_camera.Orbit(0.0f, 5.0f);
            break;
        case WXK_DOWN:
            if (shift)
                m_camera.Pan(0.0f, -0.1f);
            else if (alt)
                m_camera.Zoom(1.0f);
            else
                m_camera.Orbit(0.0f, -5.0f);
            break;
        case WXK_NUMPAD1: // Front
            m_camera.SetOrientation(0.0f, 0.0f);
            break;
        case WXK_NUMPAD3: // Right
            m_camera.SetOrientation(90.0f, 0.0f);
            break;
        case WXK_NUMPAD7: // Top
            m_camera.SetOrientation(0.0f, 89.0f);
            break;
        case WXK_NUMPAD5: // Reset/isometric
            m_camera.Reset();
            break;
        default:
            event.Skip();
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
    if (ShouldPauseHeavyTasks())
        return;

    m_controller.Update();
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
