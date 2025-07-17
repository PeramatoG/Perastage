// gui/viewportpanel.h
#pragma once

#include <wx/panel.h>

class IRenderViewport;

class ViewportPanel : public wxPanel
{
public:
    explicit ViewportPanel(wxWindow* parent);

private:
    IRenderViewport* canvas = nullptr;
};
