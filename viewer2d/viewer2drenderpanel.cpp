#include "viewer2drenderpanel.h"

#include "configmanager.h"
#include "mainwindow.h"
#include <array>

namespace {
const std::array<const char *, 3> DIST_KEYS = {"label_offset_distance_top",
                                               "label_offset_distance_front",
                                               "label_offset_distance_side"};
const std::array<const char *, 3> ANGLE_KEYS = {"label_offset_angle_top",
                                                "label_offset_angle_front",
                                                "label_offset_angle_side"};
const std::array<const char *, 3> NAME_KEYS = {"label_show_name_top",
                                               "label_show_name_front",
                                               "label_show_name_side"};
const std::array<const char *, 3> ID_KEYS = {"label_show_id_top",
                                             "label_show_id_front",
                                             "label_show_id_side"};
const std::array<const char *, 3> DMX_KEYS = {"label_show_dmx_top",
                                              "label_show_dmx_front",
                                              "label_show_dmx_side"};
} // namespace

Viewer2DRenderPanel *Viewer2DRenderPanel::s_instance = nullptr;

Viewer2DRenderPanel::Viewer2DRenderPanel(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL) {
  SetInstance(this);
  ConfigManager &cfg = ConfigManager::Get();
  wxString choices[] = {"Wireframe", "White", "By device type", "By layer"};
  m_radio = new wxRadioBox(this, wxID_ANY, "Render mode", wxDefaultPosition,
                           wxDefaultSize, 4, choices, 1, wxRA_SPECIFY_COLS);
  m_radio->SetSelection(static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_radio->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnRadio, this);

  wxString viewChoices[] = {"Top", "Front", "Side"};
  m_view = new wxRadioBox(this, wxID_ANY, "View", wxDefaultPosition,
                          wxDefaultSize, 3, viewChoices, 1, wxRA_SPECIFY_COLS);
  m_view->SetSelection(static_cast<int>(cfg.GetFloat("view2d_view")));
  m_view->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnView, this);

  m_showGrid = new wxCheckBox(this, wxID_ANY, "Show grid");
  m_showGrid->SetValue(cfg.GetFloat("grid_show") != 0.0f);
  m_showGrid->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowGrid, this);

  wxString gridChoices[] = {"Lines", "Points", "Crosses"};
  m_gridStyle =
      new wxRadioBox(this, wxID_ANY, "Grid style", wxDefaultPosition,
                     wxDefaultSize, 3, gridChoices, 1, wxRA_SPECIFY_COLS);
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

  m_showLabelName = new wxCheckBox(this, wxID_ANY, "Show name");
  m_showLabelName->SetValue(
      cfg.GetFloat(NAME_KEYS[m_view->GetSelection()]) != 0.0f);
  m_showLabelName->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowLabelName,
                        this);
  m_labelNameSize =
      new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                     wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_labelNameSize->SetRange(1, 5);
  m_labelNameSize->SetValue(
      static_cast<int>(cfg.GetFloat("label_font_size_name")));
  m_labelNameSize->Bind(wxEVT_SPINCTRL, &Viewer2DRenderPanel::OnLabelNameSize,
                        this);
  m_labelNameSize->Bind(wxEVT_SET_FOCUS, &Viewer2DRenderPanel::OnBeginTextEdit,
                        this);
  m_labelNameSize->Bind(wxEVT_KILL_FOCUS, &Viewer2DRenderPanel::OnEndTextEdit,
                        this);
  m_labelNameSize->Bind(wxEVT_TEXT_ENTER, &Viewer2DRenderPanel::OnTextEnter,
                        this);

  m_showLabelId = new wxCheckBox(this, wxID_ANY, "Show ID");
  m_showLabelId->SetValue(
      cfg.GetFloat(ID_KEYS[m_view->GetSelection()]) != 0.0f);
  m_showLabelId->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowLabelId,
                      this);
  m_labelIdSize =
      new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                     wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_labelIdSize->SetRange(1, 5);
  m_labelIdSize->SetValue(static_cast<int>(cfg.GetFloat("label_font_size_id")));
  m_labelIdSize->Bind(wxEVT_SPINCTRL, &Viewer2DRenderPanel::OnLabelIdSize,
                      this);
  m_labelIdSize->Bind(wxEVT_SET_FOCUS, &Viewer2DRenderPanel::OnBeginTextEdit,
                      this);
  m_labelIdSize->Bind(wxEVT_KILL_FOCUS, &Viewer2DRenderPanel::OnEndTextEdit,
                      this);
  m_labelIdSize->Bind(wxEVT_TEXT_ENTER, &Viewer2DRenderPanel::OnTextEnter,
                      this);

  m_showLabelAddress = new wxCheckBox(this, wxID_ANY, "Show DMX address");
  m_showLabelAddress->SetValue(
      cfg.GetFloat(DMX_KEYS[m_view->GetSelection()]) != 0.0f);
  m_showLabelAddress->Bind(wxEVT_CHECKBOX,
                           &Viewer2DRenderPanel::OnShowLabelAddress, this);
  m_labelAddressSize =
      new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                     wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_labelAddressSize->SetRange(1, 5);
  m_labelAddressSize->SetValue(
      static_cast<int>(cfg.GetFloat("label_font_size_dmx")));
  m_labelAddressSize->Bind(wxEVT_SPINCTRL,
                           &Viewer2DRenderPanel::OnLabelAddressSize, this);
  m_labelAddressSize->Bind(wxEVT_SET_FOCUS,
                           &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_labelAddressSize->Bind(wxEVT_KILL_FOCUS,
                           &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_labelAddressSize->Bind(wxEVT_TEXT_ENTER,
                           &Viewer2DRenderPanel::OnTextEnter, this);

  m_labelOffsetDistance = new wxSpinCtrlDouble(
      this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_labelOffsetDistance->SetRange(0.0, 1.0);
  m_labelOffsetDistance->SetIncrement(0.1);
  m_labelOffsetDistance->SetDigits(2);
  m_labelOffsetDistance->SetValue(
      cfg.GetFloat(DIST_KEYS[m_view->GetSelection()]));
  m_labelOffsetDistance->Bind(
      wxEVT_SPINCTRLDOUBLE, &Viewer2DRenderPanel::OnLabelOffsetDistance, this);
  m_labelOffsetDistance->Bind(wxEVT_SET_FOCUS,
                              &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_labelOffsetDistance->Bind(wxEVT_KILL_FOCUS,
                              &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_labelOffsetDistance->Bind(wxEVT_TEXT_ENTER,
                              &Viewer2DRenderPanel::OnTextEnter, this);

  m_labelOffsetAngle =
      new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                     wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_labelOffsetAngle->SetRange(0, 360);
  m_labelOffsetAngle->SetValue(
      static_cast<int>(cfg.GetFloat(ANGLE_KEYS[m_view->GetSelection()])));
  m_labelOffsetAngle->Bind(wxEVT_SPINCTRL,
                           &Viewer2DRenderPanel::OnLabelOffsetAngle, this);
  m_labelOffsetAngle->Bind(wxEVT_SET_FOCUS,
                           &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_labelOffsetAngle->Bind(wxEVT_KILL_FOCUS,
                           &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_labelOffsetAngle->Bind(wxEVT_TEXT_ENTER, &Viewer2DRenderPanel::OnTextEnter,
                           this);

  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(m_radio, 0, wxALL, 5);
  sizer->Add(m_view, 0, wxALL, 5);

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
  auto *nameSizer = new wxBoxSizer(wxHORIZONTAL);
  nameSizer->Add(m_showLabelName, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  nameSizer->Add(new wxStaticText(this, wxID_ANY, "Size"), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  nameSizer->Add(m_labelNameSize, 0);
  labelBox->Add(nameSizer, 0, wxALL, 5);

  auto *idSizer = new wxBoxSizer(wxHORIZONTAL);
  idSizer->Add(m_showLabelId, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  idSizer->Add(new wxStaticText(this, wxID_ANY, "Size"), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  idSizer->Add(m_labelIdSize, 0);
  labelBox->Add(idSizer, 0, wxALL, 5);

  auto *addrSizer = new wxBoxSizer(wxHORIZONTAL);
  addrSizer->Add(m_showLabelAddress, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  addrSizer->Add(new wxStaticText(this, wxID_ANY, "Size"), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  addrSizer->Add(m_labelAddressSize, 0);
  labelBox->Add(addrSizer, 0, wxALL, 5);

  auto *distSizer = new wxBoxSizer(wxHORIZONTAL);
  distSizer->Add(new wxStaticText(this, wxID_ANY, "Distance"), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  distSizer->Add(m_labelOffsetDistance, 0);
  labelBox->Add(distSizer, 0, wxALL, 5);

  auto *angleSizer = new wxBoxSizer(wxHORIZONTAL);
  angleSizer->Add(new wxStaticText(this, wxID_ANY, "Angle"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  angleSizer->Add(m_labelOffsetAngle, 0);
  labelBox->Add(angleSizer, 0, wxALL, 5);

  sizer->Add(labelBox, 0, wxALL, 5);

  SetSizer(sizer);
  FitInside();
  SetScrollRate(0, 10);
  Layout();
  ApplyConfig();
}

Viewer2DRenderPanel *Viewer2DRenderPanel::Instance() { return s_instance; }

void Viewer2DRenderPanel::SetInstance(Viewer2DRenderPanel *p) {
  s_instance = p;
}

void Viewer2DRenderPanel::ApplyConfig() {
  ConfigManager &cfg = ConfigManager::Get();
  m_radio->SetSelection(static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_view->SetSelection(static_cast<int>(cfg.GetFloat("view2d_view")));
  m_showGrid->SetValue(cfg.GetFloat("grid_show") != 0.0f);
  m_gridStyle->SetSelection(static_cast<int>(cfg.GetFloat("grid_style")));
  int rr = static_cast<int>(cfg.GetFloat("grid_color_r") * 255.0f);
  int gg = static_cast<int>(cfg.GetFloat("grid_color_g") * 255.0f);
  int bb = static_cast<int>(cfg.GetFloat("grid_color_b") * 255.0f);
  m_gridColor->SetColour(wxColour(rr, gg, bb));
  m_drawAbove->SetValue(cfg.GetFloat("grid_draw_above") != 0.0f);
  int viewIndex = m_view->GetSelection();
  m_showLabelName->SetValue(cfg.GetFloat(NAME_KEYS[viewIndex]) != 0.0f);
  m_labelNameSize->SetValue(
      static_cast<int>(cfg.GetFloat("label_font_size_name")));
  m_showLabelId->SetValue(cfg.GetFloat(ID_KEYS[viewIndex]) != 0.0f);
  m_labelIdSize->SetValue(static_cast<int>(cfg.GetFloat("label_font_size_id")));
  m_showLabelAddress->SetValue(cfg.GetFloat(DMX_KEYS[viewIndex]) != 0.0f);
  m_labelAddressSize->SetValue(
      static_cast<int>(cfg.GetFloat("label_font_size_dmx")));
  m_labelOffsetDistance->SetValue(cfg.GetFloat(DIST_KEYS[viewIndex]));
  m_labelOffsetAngle->SetValue(
      static_cast<int>(cfg.GetFloat(ANGLE_KEYS[viewIndex])));
  if (auto *vp = Viewer2DPanel::Instance()) {
    vp->SetRenderMode(static_cast<Viewer2DRenderMode>(m_radio->GetSelection()));
    vp->SetView(static_cast<Viewer2DView>(m_view->GetSelection()));
    vp->UpdateScene(false);
  }
}

void Viewer2DRenderPanel::OnRadio(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("view2d_render_mode",
                                static_cast<float>(m_radio->GetSelection()));
  if (auto *vp = Viewer2DPanel::Instance()) {
    vp->SetRenderMode(static_cast<Viewer2DRenderMode>(m_radio->GetSelection()));
    vp->UpdateScene(false);
  }
  evt.Skip();
}

void Viewer2DRenderPanel::OnShowGrid(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("grid_show",
                                m_showGrid->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnGridStyle(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat(
      "grid_style", static_cast<float>(m_gridStyle->GetSelection()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnGridColor(wxColourPickerEvent &evt) {
  wxColour c = evt.GetColour();
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("grid_color_r", c.Red() / 255.0f);
  cfg.SetFloat("grid_color_g", c.Green() / 255.0f);
  cfg.SetFloat("grid_color_b", c.Blue() / 255.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnDrawAbove(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("grid_draw_above",
                                m_drawAbove->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnShowLabelName(wxCommandEvent &evt) {
  int view = m_view->GetSelection();
  ConfigManager::Get().SetFloat(NAME_KEYS[view],
                                m_showLabelName->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnShowLabelId(wxCommandEvent &evt) {
  int view = m_view->GetSelection();
  ConfigManager::Get().SetFloat(ID_KEYS[view],
                                m_showLabelId->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnShowLabelAddress(wxCommandEvent &evt) {
  int view = m_view->GetSelection();
  ConfigManager::Get().SetFloat(
      DMX_KEYS[view], m_showLabelAddress->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnLabelNameSize(wxSpinEvent &evt) {
  ConfigManager::Get().SetFloat(
      "label_font_size_name", static_cast<float>(m_labelNameSize->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnLabelIdSize(wxSpinEvent &evt) {
  ConfigManager::Get().SetFloat("label_font_size_id",
                                static_cast<float>(m_labelIdSize->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnLabelAddressSize(wxSpinEvent &evt) {
  ConfigManager::Get().SetFloat(
      "label_font_size_dmx",
      static_cast<float>(m_labelAddressSize->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnLabelOffsetDistance(wxSpinDoubleEvent &evt) {
  int view = m_view->GetSelection();
  ConfigManager::Get().SetFloat(
      DIST_KEYS[view], static_cast<float>(m_labelOffsetDistance->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnLabelOffsetAngle(wxSpinEvent &evt) {
  int view = m_view->GetSelection();
  ConfigManager::Get().SetFloat(
      ANGLE_KEYS[view], static_cast<float>(m_labelOffsetAngle->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnView(wxCommandEvent &evt) {
  int sel = m_view->GetSelection();
  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetFloat("view2d_view", static_cast<float>(sel));
  m_labelOffsetDistance->SetValue(cfg.GetFloat(DIST_KEYS[sel]));
  m_labelOffsetAngle->SetValue(static_cast<int>(cfg.GetFloat(ANGLE_KEYS[sel])));
  m_showLabelName->SetValue(cfg.GetFloat(NAME_KEYS[sel]) != 0.0f);
  m_showLabelId->SetValue(cfg.GetFloat(ID_KEYS[sel]) != 0.0f);
  m_showLabelAddress->SetValue(cfg.GetFloat(DMX_KEYS[sel]) != 0.0f);
  if (auto *vp = Viewer2DPanel::Instance()) {
    vp->SetView(static_cast<Viewer2DView>(sel));
    vp->UpdateScene(false);
  }
  evt.Skip();
}

void Viewer2DRenderPanel::OnBeginTextEdit(wxFocusEvent &evt) {
  if (auto *mw = MainWindow::Instance())
    mw->EnableShortcuts(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnEndTextEdit(wxFocusEvent &evt) {
  if (auto *mw = MainWindow::Instance())
    mw->EnableShortcuts(true);
  evt.Skip();
}

void Viewer2DRenderPanel::OnTextEnter(wxCommandEvent &evt) {
  ConfigManager &cfg = ConfigManager::Get();
  if (evt.GetEventObject() == m_labelNameSize) {
    cfg.SetFloat("label_font_size_name",
                 static_cast<float>(m_labelNameSize->GetValue()));
  } else if (evt.GetEventObject() == m_labelIdSize) {
    cfg.SetFloat("label_font_size_id",
                 static_cast<float>(m_labelIdSize->GetValue()));
  } else if (evt.GetEventObject() == m_labelAddressSize) {
    cfg.SetFloat("label_font_size_dmx",
                 static_cast<float>(m_labelAddressSize->GetValue()));
  } else if (evt.GetEventObject() == m_labelOffsetDistance) {
    int view = m_view->GetSelection();
    cfg.SetFloat(DIST_KEYS[view],
                 static_cast<float>(m_labelOffsetDistance->GetValue()));
  } else if (evt.GetEventObject() == m_labelOffsetAngle) {
    int view = m_view->GetSelection();
    cfg.SetFloat(ANGLE_KEYS[view],
                 static_cast<float>(m_labelOffsetAngle->GetValue()));
  }
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  if (auto *mw = MainWindow::Instance())
    mw->EnableShortcuts(true);
  evt.Skip();
}
