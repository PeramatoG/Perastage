/*
 * File: viewer2dpanel.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: OpenGL-based top-down viewer sharing models with the 3D view.
 */

#pragma once

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include "viewer3dcontroller.h"

class Viewer2DPanel : public wxGLCanvas
{
public:
    explicit Viewer2DPanel(wxWindow* parent);
    ~Viewer2DPanel();

    static Viewer2DPanel* Instance();
    static void SetInstance(Viewer2DPanel* panel);

    void UpdateScene();

    void SetRenderMode(Viewer2DRenderMode mode);
    Viewer2DRenderMode GetRenderMode() const { return m_renderMode; }

private:
    void InitGL();
    void Render();
    void OnPaint(wxPaintEvent& event);

    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

    bool m_dragging = false;
    wxPoint m_lastMousePos;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
    float m_zoom = 1.0f;
    bool m_mouseInside = false;

    wxGLContext* m_glContext = nullptr;
    bool m_glInitialized = false;
    Viewer3DController m_controller;
    Viewer2DRenderMode m_renderMode = Viewer2DRenderMode::White;

    wxDECLARE_EVENT_TABLE();
};
