#include "viewer2drenderpanel.h"

#include <array>

Viewer2DRenderPanel* Viewer2DRenderPanel::s_instance = nullptr;

Viewer2DRenderPanel::Viewer2DRenderPanel(wxWindow* parent) : wxPanel(parent) {
    SetInstance(this);
    wxString choices[] = {"Wireframe", "White", "By device type", "By layer"};
    m_radio = new wxRadioBox(this, wxID_ANY, "Render mode", wxDefaultPosition,
                             wxDefaultSize, 4, choices, 1, wxRA_SPECIFY_COLS);
    m_radio->SetSelection(1); // default to White
    m_radio->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnRadio, this);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_radio, 0, wxALL, 5);
    SetSizerAndFit(sizer);
}

Viewer2DRenderPanel* Viewer2DRenderPanel::Instance() { return s_instance; }

void Viewer2DRenderPanel::SetInstance(Viewer2DRenderPanel* p) { s_instance = p; }

void Viewer2DRenderPanel::OnRadio(wxCommandEvent& evt) {
    if (auto* vp = Viewer2DPanel::Instance()) {
        vp->SetRenderMode(static_cast<Viewer2DRenderMode>(m_radio->GetSelection()));
        vp->UpdateScene();
    }
    evt.Skip();
}
