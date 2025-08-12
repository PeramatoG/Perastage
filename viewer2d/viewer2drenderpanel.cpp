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

    auto *labelBox = new wxStaticBoxSizer(wxVERTICAL, this, "Labels");
    auto createLabelControls = [&](const wxString &title, wxCheckBox *&show,
                                   wxSpinCtrlDouble *&size, wxCheckBox *&italic,
                                   wxCheckBox *&bold, wxColourPickerCtrl *&color,
                                   const char *showKey, const char *sizeKey,
                                   const char *italicKey, const char *boldKey,
                                   const char *rKey, const char *gKey,
                                   const char *bKey) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        show = new wxCheckBox(labelBox->GetStaticBox(), wxID_ANY, title);
        show->SetValue(cfg.GetFloat(showKey) != 0.0f);
        row->Add(show, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        size = new wxSpinCtrlDouble(labelBox->GetStaticBox(), wxID_ANY);
        size->SetRange(1.0, 100.0);
        size->SetDigits(1);
        size->SetValue(cfg.GetFloat(sizeKey));
        row->Add(size, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        italic = new wxCheckBox(labelBox->GetStaticBox(), wxID_ANY, "Ital");
        italic->SetValue(cfg.GetFloat(italicKey) != 0.0f);
        row->Add(italic, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        bold = new wxCheckBox(labelBox->GetStaticBox(), wxID_ANY, "Bold");
        bold->SetValue(cfg.GetFloat(boldKey) != 0.0f);
        row->Add(bold, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        int r = static_cast<int>(cfg.GetFloat(rKey) * 255.0f);
        int g = static_cast<int>(cfg.GetFloat(gKey) * 255.0f);
        int b = static_cast<int>(cfg.GetFloat(bKey) * 255.0f);
        color = new wxColourPickerCtrl(labelBox->GetStaticBox(), wxID_ANY,
                                       wxColour(r, g, b));
        row->Add(color, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
        labelBox->Add(row, 0, wxALL, 5);

        show->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnLabelOption, this);
        bold->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnLabelOption, this);
        italic->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnLabelOption, this);
        size->Bind(wxEVT_SPINCTRLDOUBLE, &Viewer2DRenderPanel::OnLabelSize, this);
        color->Bind(wxEVT_COLOURPICKER_CHANGED,
                    &Viewer2DRenderPanel::OnLabelColor, this);

        show->SetName(showKey);
        bold->SetName(boldKey);
        italic->SetName(italicKey);
        size->SetName(sizeKey);
        color->SetName(rKey); // store rKey in name, g/b via ClientData
        color->SetClientData((void*)gKey);
        color->SetToolTip(bKey); // use tooltip to store bKey
    };

    createLabelControls("Name", m_showName, m_nameSize, m_nameItalic,
                        m_nameBold, m_nameColor, "label_show_name",
                        "label_name_size", "label_name_italic",
                        "label_name_bold", "label_name_color_r",
                        "label_name_color_g", "label_name_color_b");
    createLabelControls("ID", m_showId, m_idSize, m_idItalic, m_idBold,
                        m_idColor, "label_show_id", "label_id_size",
                        "label_id_italic", "label_id_bold",
                        "label_id_color_r", "label_id_color_g",
                        "label_id_color_b");
    createLabelControls("Patch", m_showPatch, m_patchSize, m_patchItalic,
                        m_patchBold, m_patchColor, "label_show_patch",
                        "label_patch_size", "label_patch_italic",
                        "label_patch_bold", "label_patch_color_r",
                        "label_patch_color_g", "label_patch_color_b");

    sizer->Add(labelBox, 0, wxALL, 5);

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

void Viewer2DRenderPanel::OnLabelOption(wxCommandEvent& evt) {
    auto* obj = dynamic_cast<wxCheckBox*>(evt.GetEventObject());
    if (!obj) return;
    std::string key = obj->GetName().ToStdString();
    ConfigManager::Get().SetFloat(key, obj->GetValue() ? 1.0f : 0.0f);
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}

void Viewer2DRenderPanel::OnLabelSize(wxSpinDoubleEvent& evt) {
    auto* obj = dynamic_cast<wxSpinCtrlDouble*>(evt.GetEventObject());
    if (!obj) return;
    std::string key = obj->GetName().ToStdString();
    ConfigManager::Get().SetFloat(key, obj->GetValue());
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}

void Viewer2DRenderPanel::OnLabelColor(wxColourPickerEvent& evt) {
    auto* obj = static_cast<wxColourPickerCtrl*>(evt.GetEventObject());
    wxColour c = evt.GetColour();
    ConfigManager &cfg = ConfigManager::Get();
    std::string rKey = obj->GetName().ToStdString();
    std::string gKey = static_cast<const char*>(obj->GetClientData());
    std::string bKey = obj->GetToolTip()->GetTip().ToStdString();
    cfg.SetFloat(rKey, c.Red() / 255.0f);
    cfg.SetFloat(gKey, c.Green() / 255.0f);
    cfg.SetFloat(bKey, c.Blue() / 255.0f);
    if (auto* vp = Viewer2DPanel::Instance())
        vp->UpdateScene();
    evt.Skip();
}
