#pragma once

#include <wx/window.h>

class IRenderViewport
{
public:
    virtual ~IRenderViewport() = default;

    virtual wxWindow* GetWindow() = 0;
    virtual void InitRenderer() = 0;
};
