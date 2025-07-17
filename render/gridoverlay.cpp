#include "gridoverlay.h"
#include <wx/pen.h>
#include <cmath>

struct Vec3 { float x; float y; float z; };

static wxPoint ProjectPoint(const SimpleCamera& cam, const wxSize& size, const Vec3& p)
{
    Vec3 d{ p.x - cam.x, p.y - cam.y, p.z - cam.z };
    float cosYaw = std::cos(cam.yaw);
    float sinYaw = std::sin(cam.yaw);
    float cosPitch = std::cos(cam.pitch);
    float sinPitch = std::sin(cam.pitch);

    float x = d.x * cosYaw - d.y * sinYaw;
    float y = d.x * sinYaw + d.y * cosYaw;
    float z = d.z;

    float z2 = z * cosPitch - y * sinPitch;
    y = z * sinPitch + y * cosPitch;

    if (y <= 0.001f)
        return wxPoint(1000000, 1000000);

    float f = size.GetWidth() / (2.0f * std::tan(cam.fov * 0.5f));
    int sx = static_cast<int>(size.GetWidth() / 2.0f + x * f / y);
    int sy = static_cast<int>(size.GetHeight() / 2.0f - z2 * f / y);
    return wxPoint(sx, sy);
}

void DrawGridAndAxes(wxDC& dc, const SimpleCamera& cam, const wxSize& size)
{
    dc.SetPen(wxPen(wxColour(180, 180, 180)));
    const int grid = 20;
    for (int i = -grid; i <= grid; ++i)
    {
        Vec3 a{ static_cast<float>(i), -static_cast<float>(grid), 0.0f };
        Vec3 b{ static_cast<float>(i),  static_cast<float>(grid), 0.0f };
        wxPoint p1 = ProjectPoint(cam, size, a);
        wxPoint p2 = ProjectPoint(cam, size, b);
        if (p1.x < 1000000 && p2.x < 1000000)
            dc.DrawLine(p1, p2);

        a = { -static_cast<float>(grid), static_cast<float>(i), 0.0f };
        b = {  static_cast<float>(grid), static_cast<float>(i), 0.0f };
        p1 = ProjectPoint(cam, size, a);
        p2 = ProjectPoint(cam, size, b);
        if (p1.x < 1000000 && p2.x < 1000000)
            dc.DrawLine(p1, p2);
    }

    wxPoint origin = ProjectPoint(cam, size, {0,0,0});
    dc.SetPen(wxPen(wxColour(255,0,0), 2));
    wxPoint xAxis = ProjectPoint(cam, size, {1,0,0});
    if (origin.x < 1000000 && xAxis.x < 1000000) dc.DrawLine(origin, xAxis);
    dc.SetPen(wxPen(wxColour(0,255,0), 2));
    wxPoint yAxis = ProjectPoint(cam, size, {0,1,0});
    if (origin.x < 1000000 && yAxis.x < 1000000) dc.DrawLine(origin, yAxis);
    dc.SetPen(wxPen(wxColour(0,0,255), 2));
    wxPoint zAxis = ProjectPoint(cam, size, {0,0,1});
    if (origin.x < 1000000 && zAxis.x < 1000000) dc.DrawLine(origin, zAxis);
}
