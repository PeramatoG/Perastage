/*
 * File: viewer2dpanel.cpp
 * Author: Luisma Peramato
 * License: MIT
 * Description: Implementation of a simple 2D viewer panel.
 */

#include "viewer2dpanel.h"
#include <wx/dcbuffer.h>

namespace {
Viewer2DPanel* g_instance = nullptr;
}

wxBEGIN_EVENT_TABLE(Viewer2DPanel, wxPanel)
    EVT_PAINT(Viewer2DPanel::OnPaint)
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

    dc.SetPen(wxPen(wxColour(60, 60, 60)));
    for (int x = 0; x < w; x += 25)
        dc.DrawLine(x, 0, x, h);
    for (int y = 0; y < h; y += 25)
        dc.DrawLine(0, y, w, y);

    dc.SetPen(wxPen(*wxRED_PEN));
    dc.DrawLine(0, h / 2, w, h / 2);
    dc.SetPen(wxPen(*wxGREEN_PEN));
    dc.DrawLine(w / 2, 0, w / 2, h);
}
