/*
 * File: viewer2dpanel.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of a simple 2D viewer panel.
 */

#include "viewer2dpanel.h"
#include <wx/dcbuffer.h>
#include <cmath>
#include <algorithm>
#include <array>
#include "scenedatamanager.h"
#include "configmanager.h"

// MVR coordinates are in millimeters. Convert to meters for rendering.
static constexpr float RENDER_SCALE = 0.001f;
// Pixels per meter at default zoom level.
static constexpr float PIXELS_PER_METER = 25.0f;

namespace {
Viewer2DPanel* g_instance = nullptr;
}

wxBEGIN_EVENT_TABLE(Viewer2DPanel, wxPanel)
    EVT_PAINT(Viewer2DPanel::OnPaint)
    EVT_LEFT_DOWN(Viewer2DPanel::OnMouseDown)
    EVT_LEFT_UP(Viewer2DPanel::OnMouseUp)
    EVT_MOTION(Viewer2DPanel::OnMouseMove)
    EVT_MOUSEWHEEL(Viewer2DPanel::OnMouseWheel)
    EVT_KEY_DOWN(Viewer2DPanel::OnKeyDown)
    EVT_ENTER_WINDOW(Viewer2DPanel::OnMouseEnter)
    EVT_LEAVE_WINDOW(Viewer2DPanel::OnMouseLeave)
wxEND_EVENT_TABLE()

Viewer2DPanel::Viewer2DPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

Viewer2DPanel* Viewer2DPanel::Instance()
{
    return g_instance;
}

void Viewer2DPanel::SetInstance(Viewer2DPanel* panel)
{
    g_instance = panel;
}

void Viewer2DPanel::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);
    int w, h;
    GetClientSize(&w, &h);

    dc.SetBackground(wxBrush(wxColour(20, 20, 20)));
    dc.Clear();

    float originX = w / 2.0f + m_offsetX * m_zoom;
    float originY = h / 2.0f + m_offsetY * m_zoom;
    int step = std::max(1, static_cast<int>(25 * m_zoom));

    dc.SetPen(wxPen(wxColour(60, 60, 60)));
    for (int x = static_cast<int>(originX); x < w; x += step)
        dc.DrawLine(x, 0, x, h);
    for (int x = static_cast<int>(originX) - step; x >= 0; x -= step)
        dc.DrawLine(x, 0, x, h);
    for (int y = static_cast<int>(originY); y < h; y += step)
        dc.DrawLine(0, y, w, y);
    for (int y = static_cast<int>(originY) - step; y >= 0; y -= step)
        dc.DrawLine(0, y, w, y);

    // Draw scene elements
    float ppm = PIXELS_PER_METER * m_zoom;

    // Fixtures
    dc.SetPen(wxPen(wxColour(200, 200, 200)));
    dc.SetBrush(wxBrush(wxColour(200, 200, 200)));
    const auto &fixtures = SceneDataManager::Instance().GetFixtures();
    for (const auto &[uuid, f] : fixtures)
    {
        if (!ConfigManager::Get().IsLayerVisible(f.layer))
            continue;
        float wxm = f.transform.o[0] * RENDER_SCALE;
        float wym = f.transform.o[1] * RENDER_SCALE;
        int sx = static_cast<int>(originX + wxm * ppm);
        int sy = static_cast<int>(originY + wym * ppm);
        int r = std::max(2, static_cast<int>(3 * m_zoom));
        dc.DrawCircle(sx, sy, r);
    }

    // Trusses
    dc.SetPen(wxPen(wxColour(100, 100, 255)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    const auto &trusses = SceneDataManager::Instance().GetTrusses();
    for (const auto &[uuid, t] : trusses)
    {
        if (!ConfigManager::Get().IsLayerVisible(t.layer))
            continue;
        float len = (t.lengthMm > 0 ? t.lengthMm : 400.0f) * RENDER_SCALE;
        float wid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;

        std::array<std::array<float, 2>, 4> corners = {
            std::array<float, 2>{-0.5f * len, -0.5f * wid},
            {0.5f * len, -0.5f * wid},
            {0.5f * len, 0.5f * wid},
            {-0.5f * len, 0.5f * wid}};
        float cx = t.transform.o[0] * RENDER_SCALE;
        float cy = t.transform.o[1] * RENDER_SCALE;
        float ux = t.transform.u[0];
        float uy = t.transform.u[1];
        float vx = t.transform.v[0];
        float vy = t.transform.v[1];
        std::array<wxPoint, 4> pts;
        for (int i = 0; i < 4; ++i)
        {
            float wxm = cx + corners[i][0] * ux + corners[i][1] * vx;
            float wym = cy + corners[i][0] * uy + corners[i][1] * vy;
            int sx = static_cast<int>(originX + wxm * ppm);
            int sy = static_cast<int>(originY + wym * ppm);
            pts[i] = wxPoint(sx, sy);
        }
        dc.DrawPolygon(4, pts.data());
    }

    // Scene objects
    dc.SetPen(wxPen(wxColour(255, 255, 0)));
    dc.SetBrush(wxBrush(wxColour(255, 255, 0)));
    const auto &objects = SceneDataManager::Instance().GetSceneObjects();
    for (const auto &[uuid, o] : objects)
    {
        if (!ConfigManager::Get().IsLayerVisible(o.layer))
            continue;
        float wxm = o.transform.o[0] * RENDER_SCALE;
        float wym = o.transform.o[1] * RENDER_SCALE;
        int sx = static_cast<int>(originX + wxm * ppm);
        int sy = static_cast<int>(originY + wym * ppm);
        int size = std::max(2, static_cast<int>(4 * m_zoom));
        dc.DrawRectangle(sx - size / 2, sy - size / 2, size, size);
    }

    // Axes
    dc.SetPen(wxPen(*wxRED_PEN));
    dc.DrawLine(0, static_cast<int>(originY), w, static_cast<int>(originY));
    dc.SetPen(wxPen(*wxGREEN_PEN));
    dc.DrawLine(static_cast<int>(originX), 0, static_cast<int>(originX), h);
}

void Viewer2DPanel::OnMouseDown(wxMouseEvent& event)
{
    if (event.LeftDown())
    {
        CaptureMouse();
        m_dragging = true;
        m_lastMousePos = event.GetPosition();
    }
}

void Viewer2DPanel::OnMouseUp(wxMouseEvent& event)
{
    if (event.LeftUp() && m_dragging)
    {
        m_dragging = false;
        if (HasCapture())
            ReleaseMouse();
    }
}

void Viewer2DPanel::OnMouseMove(wxMouseEvent& event)
{
    if (m_dragging && event.Dragging())
    {
        wxPoint pos = event.GetPosition();
        int dx = pos.x - m_lastMousePos.x;
        int dy = pos.y - m_lastMousePos.y;
        m_offsetX += dx / m_zoom;
        m_offsetY += dy / m_zoom;
        m_lastMousePos = pos;
        Refresh();
    }
}

void Viewer2DPanel::OnMouseWheel(wxMouseEvent& event)
{
    int rotation = event.GetWheelRotation();
    int deltaWheel = event.GetWheelDelta();
    float steps = 0.0f;
    if (deltaWheel != 0)
        steps = static_cast<float>(rotation) / static_cast<float>(deltaWheel);
    float factor = std::pow(1.1f, steps);
    m_zoom *= factor;
    if (m_zoom < 0.1f)
        m_zoom = 0.1f;
    Refresh();
}

void Viewer2DPanel::OnKeyDown(wxKeyEvent& event)
{
    if (!m_mouseInside) { event.Skip(); return; }

    bool alt = event.AltDown();
    float panStep = 10.0f / m_zoom;

    switch (event.GetKeyCode())
    {
    case WXK_LEFT:
        if (alt)
            m_zoom *= 1.1f;
        else
            m_offsetX += panStep;
        break;
    case WXK_RIGHT:
        if (alt)
            m_zoom /= 1.1f;
        else
            m_offsetX -= panStep;
        break;
    case WXK_UP:
        if (alt)
            m_zoom *= 1.1f;
        else
            m_offsetY += panStep;
        break;
    case WXK_DOWN:
        if (alt)
            m_zoom /= 1.1f;
        else
            m_offsetY -= panStep;
        break;
    default:
        event.Skip();
        return;
    }

    if (m_zoom < 0.1f)
        m_zoom = 0.1f;
    Refresh();
}

void Viewer2DPanel::OnMouseEnter(wxMouseEvent& event)
{
    m_mouseInside = true;
    SetFocus();
    event.Skip();
}

void Viewer2DPanel::OnMouseLeave(wxMouseEvent& event)
{
    m_mouseInside = false;
    event.Skip();
}
