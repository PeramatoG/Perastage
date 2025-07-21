#include "glviewport.h"
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include "gridoverlay.h"
#include <cmath>
#include <algorithm>
struct Vec3 { float x; float y; float z; };
#include <GL/gl.h>


wxBEGIN_EVENT_TABLE(GLViewport, wxGLCanvas)
    EVT_PAINT(GLViewport::OnPaint)
    EVT_SIZE(GLViewport::OnResize)
    EVT_KEY_DOWN(GLViewport::OnKeyDown)
    EVT_LEFT_DOWN(GLViewport::OnMouseDown)
    EVT_LEFT_UP(GLViewport::OnMouseUp)
    EVT_MOTION(GLViewport::OnMouseMove)
    EVT_MOUSEWHEEL(GLViewport::OnMouseWheel)
    EVT_TIMER(wxID_ANY, GLViewport::OnRenderTimer)
    EVT_ERASE_BACKGROUND(GLViewport::OnEraseBackground)
wxEND_EVENT_TABLE()

GLViewport::GLViewport(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      context(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetFocus();
    renderTimer.SetOwner(this);
    renderTimer.Start(16);
}

GLViewport::~GLViewport()
{
    renderTimer.Stop();
}

void GLViewport::InitRenderer()
{
    // No special initialization needed for now
}

void GLViewport::Render()
{
    wxSize size = GetClientSize();
    glViewport(0, 0, size.GetWidth(), size.GetHeight());
    glClearColor(0.3f, 0.3f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLViewport::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGCDC gdc(dc);

    SetCurrent(context);
    Render();
    SwapBuffers();

    DrawGridAndAxes(gdc, camera, GetClientSize());
    event.Skip(false);
}

void GLViewport::OnResize(wxSizeEvent& event)
{
    Refresh();
    event.Skip();
}

void GLViewport::OnKeyDown(wxKeyEvent& event)
{
    const float step = 0.2f;
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    Vec3 forward{ sinYaw, cosYaw, 0.0f };
    Vec3 right{ cosYaw, -sinYaw, 0.0f };

    switch (event.GetKeyCode())
    {
    case 'W': case WXK_UP:
        camera.x += forward.x * step;
        camera.y += forward.y * step;
        camera.z += forward.z * step;
        break;
    case 'S': case WXK_DOWN:
        camera.x -= forward.x * step;
        camera.y -= forward.y * step;
        camera.z -= forward.z * step;
        break;
    case 'A': case WXK_LEFT:
        camera.x -= right.x * step;
        camera.y -= right.y * step;
        camera.z -= right.z * step;
        break;
    case 'D': case WXK_RIGHT:
        camera.x += right.x * step;
        camera.y += right.y * step;
        camera.z += right.z * step;
        break;
    default:
        event.Skip();
        return;
    }
    Refresh();
}

void GLViewport::OnMouseDown(wxMouseEvent& event)
{
    mouseDragging = true;
    lastMousePos = event.GetPosition();
    CaptureMouse();
}

void GLViewport::OnMouseUp(wxMouseEvent& event)
{
    if (mouseDragging && HasCapture())
        ReleaseMouse();
    mouseDragging = false;
}

void GLViewport::OnMouseMove(wxMouseEvent& event)
{
    if (!mouseDragging)
    {
        event.Skip();
        return;
    }

    wxPoint pos = event.GetPosition();
    wxPoint delta = pos - lastMousePos;
    lastMousePos = pos;

    const float sensitivity = 0.005f;
    if (event.ShiftDown())
    {
        const float panScale = 0.01f;
        float cosYaw = std::cos(camera.yaw);
        float sinYaw = std::sin(camera.yaw);
        Vec3 right{ cosYaw, -sinYaw, 0.0f };
        camera.x -= delta.x * panScale * right.x;
        camera.y -= delta.x * panScale * right.y;
        camera.z += delta.y * panScale;
    }
    else
    {
        camera.yaw += delta.x * sensitivity;
        camera.pitch += -delta.y * sensitivity;
        camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);
    }
    Refresh();
}

void GLViewport::OnMouseWheel(wxMouseEvent& event)
{
    int rotation = event.GetWheelRotation();
    int delta = event.GetWheelDelta();
    if (delta == 0 || rotation == 0)
    {
        event.Skip();
        return;
    }

    float steps = static_cast<float>(rotation) / static_cast<float>(delta);
    const float stepSize = 0.5f;
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    Vec3 forward{ sinYaw, cosYaw, 0.0f };
    camera.x += forward.x * stepSize * steps;
    camera.y += forward.y * stepSize * steps;

    Refresh();
}

void GLViewport::OnRenderTimer(wxTimerEvent&)
{
    Refresh(false);
}

void GLViewport::OnEraseBackground(wxEraseEvent&)
{
    // Prevent background clearing to avoid flicker
}

