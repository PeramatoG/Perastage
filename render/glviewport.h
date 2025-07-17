#pragma once

#include <wx/glcanvas.h>
#include <wx/timer.h>
#include "irenderviewport.h"
#include "camera.h"

// OpenGL-based viewport
class GLViewport : public wxGLCanvas, public IRenderViewport
{
public:
    explicit GLViewport(wxWindow* parent);
    ~GLViewport();

    wxWindow* GetWindow() override { return this; }
    void InitRenderer() override;

    void OnKeyDown(wxKeyEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnResize(wxSizeEvent& event);

private:
    void Render();
    void OnRenderTimer(wxTimerEvent&);
    void OnEraseBackground(wxEraseEvent&);

    wxGLContext context;
    SimpleCamera camera;
    bool mouseDragging = false;
    wxPoint lastMousePos;
    wxTimer renderTimer;

    wxDECLARE_EVENT_TABLE();
};
