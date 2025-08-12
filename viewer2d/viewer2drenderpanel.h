#pragma once

#include <wx/wx.h>
#include <wx/clrpicker.h>
#include <wx/spinctrl.h>
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
    void OnLabelOption(wxCommandEvent& evt);
    void OnLabelSize(wxSpinDoubleEvent& evt);
    void OnLabelColor(wxColourPickerEvent& evt);

    wxRadioBox* m_radio = nullptr;
    wxCheckBox* m_showGrid = nullptr;
    wxRadioBox* m_gridStyle = nullptr;
    wxColourPickerCtrl* m_gridColor = nullptr;
    wxCheckBox* m_drawAbove = nullptr;
    wxCheckBox* m_showName = nullptr;
    wxCheckBox* m_showId = nullptr;
    wxCheckBox* m_showPatch = nullptr;
    wxSpinCtrlDouble* m_nameSize = nullptr;
    wxSpinCtrlDouble* m_idSize = nullptr;
    wxSpinCtrlDouble* m_patchSize = nullptr;
    wxCheckBox* m_nameItalic = nullptr;
    wxCheckBox* m_idItalic = nullptr;
    wxCheckBox* m_patchItalic = nullptr;
    wxCheckBox* m_nameBold = nullptr;
    wxCheckBox* m_idBold = nullptr;
    wxCheckBox* m_patchBold = nullptr;
    wxColourPickerCtrl* m_nameColor = nullptr;
    wxColourPickerCtrl* m_idColor = nullptr;
    wxColourPickerCtrl* m_patchColor = nullptr;
    static Viewer2DRenderPanel* s_instance;
};
