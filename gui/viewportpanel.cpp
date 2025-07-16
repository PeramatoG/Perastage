// gui/viewportpanel.cpp
#include "viewportpanel.h"
#include "vulkanviewport.h"
#include <wx/sizer.h>
#include <wx/msgdlg.h>


ViewportPanel::ViewportPanel(wxWindow* parent)
    : wxPanel(parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    try {
        canvas = new VulkanViewport(this);
        canvas->InitRenderer();
        sizer->Add(canvas, 1, wxEXPAND | wxALL, 0);
    }
    catch (const std::exception& e) {
        wxMessageBox(e.what(), "Vulkan Error", wxICON_ERROR);
    }

    SetSizer(sizer);
}
