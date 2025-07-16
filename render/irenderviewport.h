// render/irenderviewport.h
#pragma once

#include <wx/window.h>

class IRenderViewport : public wxWindow
{
public:
    // Full constructor replicating wxWindow parameters
    IRenderViewport(wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0)
        : wxWindow(parent, id, pos, size, style)
    {
    }

    virtual ~IRenderViewport() = default;

    virtual void InitRenderer() = 0;
};
