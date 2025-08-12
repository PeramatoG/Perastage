#pragma once

#include <wx/wx.h>
#include <wx/clrpicker.h>
#include "viewer2dpanel.h"

class Viewer2DRenderPanel : public wxPanel {
public:
    explicit Viewer2DRenderPanel(wxWindow* parent);

    static Viewer2DRenderPanel* Instance();
    static void SetInstance(Viewer2DRenderPanel* p);

private:
    void OnRadio(wxCommandEvent& evt);
    void OnShowGrid(wxCommandEvent& evt);
    void OnGridStyle(wxCommandEvent& evt);
    void OnGridColor(wxColourPickerEvent& evt);
    void OnDrawAbove(wxCommandEvent& evt);

    wxRadioBox* m_radio = nullptr;
    wxCheckBox* m_showGrid = nullptr;
    wxRadioBox* m_gridStyle = nullptr;
    wxColourPickerCtrl* m_gridColor = nullptr;
    wxCheckBox* m_drawAbove = nullptr;
    static Viewer2DRenderPanel* s_instance;
};
