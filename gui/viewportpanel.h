// gui/viewportpanel.h
#pragma once

#include <wx/panel.h>

class VulkanViewport;

class ViewportPanel : public wxPanel
{
public:
    explicit ViewportPanel(wxWindow* parent);

private:
    VulkanViewport* canvas = nullptr;
};
