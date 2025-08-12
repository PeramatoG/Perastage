#include "viewer2drenderpanel.h"

#include <array>
#include "configmanager.h"

Viewer2DRenderPanel* Viewer2DRenderPanel::s_instance = nullptr;

Viewer2DRenderPanel::Viewer2DRenderPanel(wxWindow* parent) : wxPanel(parent) {
    SetInstance(this);
    ConfigManager &cfg = ConfigManager::Get();
    wxString choices[] = {"Wireframe", "White", "By device type", "By layer"};
    m_radio = new wxRadioBox(this, wxID_ANY, "Render mode", wxDefaultPosition,
                             wxDefaultSize, 4, choices, 1, wxRA_SPECIFY_COLS);
    m_radio->SetSelection(1); // default to White
    m_radio->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnRadio, this);

    m_showGrid = new wxCheckBox(this, wxID_ANY, "Show grid");
    m_showGrid->SetValue(cfg.GetFloat("grid_show") != 0.0f);
    m_showGrid->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowGrid, this);

    wxString gridChoices[] = {"Lines", "Points", "Crosses"};
    m_gridStyle = new wxRadioBox(this, wxID_ANY, "Grid style", wxDefaultPosition,
                                 wxDefaultSize, 3, gridChoices, 1,
                                 wxRA_SPECIFY_COLS);
    m_gridStyle->SetSelection(static_cast<int>(cfg.GetFloat("grid_style")));
    m_gridStyle->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnGridStyle, this);

    int rr = static_cast<int>(cfg.GetFloat("grid_color_r") * 255.0f);
    int gg = static_cast<int>(cfg.GetFloat("grid_color_g") * 255.0f);
    int bb = static_cast<int>(cfg.GetFloat("grid_color_b") * 255.0f);
    m_gridColor = new wxColourPickerCtrl(this, wxID_ANY, wxColour(rr, gg, bb));
    m_gridColor->Bind(wxEVT_COLOURPICKER_CHANGED,
                      &Viewer2DRenderPanel::OnGridColor, this);

    m_drawAbove = new wxCheckBox(this, wxID_ANY, "Draw grid on top");
    m_drawAbove->SetValue(cfg.GetFloat("grid_draw_above") != 0.0f);
    m_drawAbove->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnDrawAbove, this);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_radio, 0, wxALL, 5);

    auto *gridBox = new wxStaticBoxSizer(wxVERTICAL, this, "Grid");
    gridBox->Add(m_showGrid, 0, wxALL, 5);
    gridBox->Add(m_gridStyle, 0, wxALL, 5);
    auto *colorSizer = new wxBoxSizer(wxHORIZONTAL);
    colorSizer->Add(new wxStaticText(this, wxID_ANY, "Color"), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    colorSizer->Add(m_gridColor, 0);
    gridBox->Add(colorSizer, 0, wxALL, 5);
    gridBox->Add(m_drawAbove, 0, wxALL, 5);
    sizer->Add(gridBox, 0, wxALL, 5);

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

void Viewer2DRenderPanel::OnShowGrid(wxCommandEvent& evt) {
    ConfigManager::Get().SetFloat("grid_show", m_showGrid->GetValue() ? 1.0f : 0.0f);
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}

void Viewer2DRenderPanel::OnGridStyle(wxCommandEvent& evt) {
    ConfigManager::Get().SetFloat("grid_style", static_cast<float>(m_gridStyle->GetSelection()));
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}

void Viewer2DRenderPanel::OnGridColor(wxColourPickerEvent& evt) {
    wxColour c = evt.GetColour();
    ConfigManager &cfg = ConfigManager::Get();
    cfg.SetFloat("grid_color_r", c.Red() / 255.0f);
    cfg.SetFloat("grid_color_g", c.Green() / 255.0f);
    cfg.SetFloat("grid_color_b", c.Blue() / 255.0f);
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}

void Viewer2DRenderPanel::OnDrawAbove(wxCommandEvent& evt) {
    ConfigManager::Get().SetFloat("grid_draw_above", m_drawAbove->GetValue() ? 1.0f : 0.0f);
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}
