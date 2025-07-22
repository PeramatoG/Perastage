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
#include <GL/gl.h>
#include <GL/glu.h>

#include "viewer3dpanel.h"
#include "consolepanel.h"
#include <wx/dcclient.h>
#include <wx/event.h>

wxBEGIN_EVENT_TABLE(Viewer3DPanel, wxGLCanvas)
EVT_PAINT(Viewer3DPanel::OnPaint)
EVT_SIZE(Viewer3DPanel::OnResize)
EVT_LEFT_DOWN(Viewer3DPanel::OnMouseDown)
EVT_LEFT_UP(Viewer3DPanel::OnMouseUp)
EVT_MOTION(Viewer3DPanel::OnMouseMove)
EVT_MOUSEWHEEL(Viewer3DPanel::OnMouseWheel)
wxEND_EVENT_TABLE()


Viewer3DPanel::Viewer3DPanel(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
    m_glContext(new wxGLContext(this))
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
}

Viewer3DPanel::~Viewer3DPanel()
{
    delete m_glContext;
}

// Initializes OpenGL basic settings
void Viewer3DPanel::InitGL()
{
    SetCurrent(*m_glContext);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

// Paint event handler
void Viewer3DPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    InitGL();
    if (ConsolePanel::Instance())
        ConsolePanel::Instance()->AppendMessage("Viewer paint event");
    Render();

    int w, h;
    GetClientSize(&w, &h);
    dc.SetTextForeground(*wxWHITE);
    m_controller.DrawFixtureLabels(dc, w, h);
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
    SwapBuffers();
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
}

// Handles mouse movement (orbit or pan)
void Viewer3DPanel::OnMouseMove(wxMouseEvent& event)
{
    if (m_dragging && event.Dragging())
    {
        wxPoint pos = event.GetPosition();
        int dx = pos.x - m_lastMousePos.x;
        int dy = pos.y - m_lastMousePos.y;

        if (m_mode == InteractionMode::Orbit && event.LeftIsDown())
        {
            m_camera.Orbit(dx * 0.5f, -dy * 0.5f);
        }
        else if (m_mode == InteractionMode::Pan && (event.MiddleIsDown() || event.ShiftDown()))
        {
            m_camera.Pan(-dx * 0.01f, dy * 0.01f);
        }

        m_lastMousePos = pos;
        Refresh();
    }
}

// Handles mouse wheel (zoom)
void Viewer3DPanel::OnMouseWheel(wxMouseEvent& event)
{
    int rotation = event.GetWheelRotation();
    float delta = (rotation > 0) ? -1.0f : 1.0f;
    m_camera.Zoom(delta);
    Refresh();
}

// Updates the controller with current scene data
void Viewer3DPanel::UpdateScene()
{
    m_controller.Update();
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
