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

    wxDECLARE_EVENT_TABLE();
};
