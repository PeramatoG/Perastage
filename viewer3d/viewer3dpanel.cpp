/*
 * File: viewer3dpanel.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of the 3D viewer panel using OpenGL.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "viewer3dpanel.h"
#include "consolepanel.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "configmanager.h"
#include "fixturepatchdialog.h"
#include <wx/dcclient.h>
#include <wx/event.h>
#include <wx/log.h>
#include <chrono>

wxDEFINE_EVENT(wxEVT_VIEWER_REFRESH, wxThreadEvent);
wxBEGIN_EVENT_TABLE(Viewer3DPanel, wxGLCanvas)
EVT_PAINT(Viewer3DPanel::OnPaint)
EVT_SIZE(Viewer3DPanel::OnResize)
EVT_LEFT_DOWN(Viewer3DPanel::OnMouseDown)
EVT_LEFT_UP(Viewer3DPanel::OnMouseUp)
EVT_MOTION(Viewer3DPanel::OnMouseMove)
EVT_LEFT_DCLICK(Viewer3DPanel::OnMouseDClick)
EVT_MOUSEWHEEL(Viewer3DPanel::OnMouseWheel)
EVT_KEY_DOWN(Viewer3DPanel::OnKeyDown)
EVT_ENTER_WINDOW(Viewer3DPanel::OnMouseEnter)
EVT_LEAVE_WINDOW(Viewer3DPanel::OnMouseLeave)
EVT_THREAD(wxEVT_VIEWER_REFRESH, Viewer3DPanel::OnThreadRefresh)
wxEND_EVENT_TABLE()


Viewer3DPanel::Viewer3DPanel(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
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
    m_threadRunning = false;
    if (m_refreshThread.joinable())
        m_refreshThread.join();
    delete m_glContext;
}

// Initializes OpenGL basic settings
void Viewer3DPanel::InitGL()
{
    SetCurrent(*m_glContext);
    if (!m_glInitialized) {
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            wxLogError("GLEW initialization failed: %s",
                       reinterpret_cast<const char*>(glewGetErrorString(err)));
        }
        m_controller.InitializeGL();
        m_glInitialized = true;
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
}

// Paint event handler
void Viewer3DPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    InitGL();
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
        if (event.ShiftDown() || event.MiddleDown())
            m_mode = InteractionMode::Pan;
        else
            m_mode = InteractionMode::Orbit;

        m_dragging = true;
        m_draggedSincePress = false;
        m_lastMousePos = event.GetPosition();
        CaptureMouse();
    }
}

// Handles mouse button release
void Viewer3DPanel::OnMouseUp(wxMouseEvent& event)
{
    if (m_dragging && (event.LeftUp() || event.MiddleUp()))
    {
        m_dragging = false;
        m_mode = InteractionMode::None;
        ReleaseMouse();
    }

    if (event.LeftUp() && !m_draggedSincePress)
    {
        int w, h;
        GetClientSize(&w, &h);
        SetCurrent(*m_glContext);
        wxString label;
        wxPoint pos;
        if (!m_controller.GetFixtureLabelAt(event.GetX(), event.GetY(), w, h, label, pos))
        {
            SetSelectedFixtures({});
            if (FixtureTablePanel::Instance())
                FixtureTablePanel::Instance()->ClearSelection();
        }
    }
    m_draggedSincePress = false;
}

// Handles mouse movement (orbit or pan)
void Viewer3DPanel::OnMouseMove(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();

    if (m_dragging && event.Dragging())
    {
        int dx = pos.x - m_lastMousePos.x;
        int dy = pos.y - m_lastMousePos.y;

        m_draggedSincePress = true;

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
    int rotation = event.GetWheelRotation();
    float delta = (rotation > 0) ? -1.0f : 1.0f;
    m_camera.Zoom(delta);
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
    m_controller.Update();
}

void Viewer3DPanel::SetSelectedFixtures(const std::vector<std::string>& uuids)
{
    m_controller.SetSelectedUuids(uuids);
    Refresh();
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

