#pragma once

#include <wx/glcanvas.h>
#include <vector>
#include <string>
#include "viewer3dcamera.h"
#include "gdtfloader.h"

// Simple 3D preview panel for a single fixture
class FixturePreviewPanel : public wxGLCanvas {
public:
    explicit FixturePreviewPanel(wxWindow* parent);
    ~FixturePreviewPanel();

    // Loads fixture model from a GDTF file. When loading fails a
    // simple cube will be displayed instead.
    void LoadFixture(const std::string& gdtfPath);

private:
    void OnPaint(wxPaintEvent& evt);
    void OnResize(wxSizeEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);

    void InitGL();
    void Render();

    wxGLContext* m_glContext = nullptr;
    bool m_glInitialized = false;

    Viewer3DCamera m_camera;
    std::vector<GdtfObject> m_objects;
    bool m_hasModel = false;
    float m_bbMin[3];
    float m_bbMax[3];

    bool m_dragging = false;
    wxPoint m_lastMousePos;

    wxDECLARE_EVENT_TABLE();
};

