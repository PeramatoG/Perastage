#pragma once

#include <wx/wx.h>
#include "viewer2dpanel.h"

class Viewer2DRenderPanel : public wxPanel {
public:
    explicit Viewer2DRenderPanel(wxWindow* parent);

    static Viewer2DRenderPanel* Instance();
    static void SetInstance(Viewer2DRenderPanel* p);

private:
    void OnRadio(wxCommandEvent& evt);
    wxRadioBox* m_radio = nullptr;
    static Viewer2DRenderPanel* s_instance;
};
