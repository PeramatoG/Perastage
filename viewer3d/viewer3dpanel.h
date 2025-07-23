/*
 * File: viewer3dpanel.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: OpenGL-based 3D viewer panel using wxGLCanvas.
 */

#pragma once

#include <wx/glcanvas.h>
#include "viewer3dcamera.h"
#include "viewer3dcontroller.h"
#include <string>
#include <thread>
#include <atomic>
#include <wx/thread.h>

wxDECLARE_EVENT(wxEVT_VIEWER_REFRESH, wxThreadEvent);

class Viewer3DPanel : public wxGLCanvas
{
public:
    Viewer3DPanel(wxWindow* parent);
    ~Viewer3DPanel();

    // Toggles for rendering options
    bool showAxes = true;
    bool showGrid = true;

    void UpdateScene();
    void SetSelectedFixtures(const std::vector<std::string>& uuids);

    static Viewer3DPanel* Instance();
    static void SetInstance(Viewer3DPanel* panel);

private:
    wxGLContext* m_glContext;
    Viewer3DCamera m_camera;

    // Mouse interaction state
    bool m_dragging = false;
    bool m_mouseInside = false;
    wxPoint m_lastMousePos;

    // Type of interaction currently active (Orbit or Pan)
    enum class InteractionMode { None, Orbit, Pan };
    InteractionMode m_mode = InteractionMode::None;

    // Initializes OpenGL settings
    void InitGL();

    // Handles paint events
    void OnPaint(wxPaintEvent& event);

    // Handles resize events
    void OnResize(wxSizeEvent& event);

    // Handles mouse input for camera control
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

    // Renders the full scene
    void Render();

    // Hovered fixture label state
    bool m_hasHover = false;
    wxPoint m_hoverPos;
    wxString m_hoverText;
    std::string m_hoverUuid;

    Viewer3DController m_controller;

    std::atomic<bool> m_threadRunning{false};
    std::thread m_refreshThread;
    void RefreshLoop();
    void OnThreadRefresh(wxThreadEvent& event);

    wxDECLARE_EVENT_TABLE();
};

