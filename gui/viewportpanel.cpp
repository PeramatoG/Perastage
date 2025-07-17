// gui/viewportpanel.cpp
#include "viewportpanel.h"
#include "glviewport.h"
#include "vulkanviewport.h"
#include <wx/sizer.h>
#include <wx/msgdlg.h>


ViewportPanel::ViewportPanel(wxWindow* parent)
    : wxPanel(parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    try {
        canvas = new GLViewport(this);
        sizer->Add(canvas->GetWindow(), 1, wxEXPAND | wxALL, 0);
    }
    catch (const std::exception& e) {
        wxMessageBox(e.what(), "OpenGL Error", wxICON_ERROR);
    }

    SetSizer(sizer);
}
