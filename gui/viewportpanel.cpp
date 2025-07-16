// gui/viewportpanel.cpp
#include "viewportpanel.h"
#include "vulkanviewport.h"
#include <wx/sizer.h>


ViewportPanel::ViewportPanel(wxWindow* parent)
    : wxPanel(parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    canvas = new VulkanViewport(this);  // Clase que implementaremos luego
    sizer->Add(canvas, 1, wxEXPAND | wxALL, 0);

    SetSizer(sizer);
}
