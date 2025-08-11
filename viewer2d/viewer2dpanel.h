/*
 * File: viewer2dpanel.h
 * Author: Luisma Peramato
 * License: MIT
 * Description: Simple 2D viewer panel using wxPanel.
 */

#pragma once

#include <wx/wx.h>
#include <wx/panel.h>

class Viewer2DPanel : public wxPanel
{
public:
    explicit Viewer2DPanel(wxWindow* parent);

    static Viewer2DPanel* Instance();
    static void SetInstance(Viewer2DPanel* panel);

private:
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

    wxDECLARE_EVENT_TABLE();
};
