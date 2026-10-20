/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "viewer2drenderpanel.h"

#include "configmanager.h"
#include "fixture_label_editor.h"
#include "mainwindow.h"
#include <algorithm>
#include <array>
#include <wx/settings.h>

namespace {
constexpr int kRulerAxisPositionWidth = 72;
constexpr int kRulerAxisColorSwatchSize = 36;

wxColour BuildColorFromConfig(const ConfigManager &cfg, const char *rKey,
                              const char *gKey, const char *bKey) {
  return wxColour(static_cast<int>(cfg.GetFloat(rKey) * 255.0f),
                  static_cast<int>(cfg.GetFloat(gKey) * 255.0f),
                  static_cast<int>(cfg.GetFloat(bKey) * 255.0f));
}

void SaveColorToConfig(ConfigManager &cfg, const wxColour &color,
                       const char *rKey, const char *gKey, const char *bKey) {
  cfg.SetFloat(rKey, color.Red() / 255.0f);
  cfg.SetFloat(gKey, color.Green() / 255.0f);
  cfg.SetFloat(bKey, color.Blue() / 255.0f);
}

float CurrentSpinDoubleValue(const wxSpinCtrlDouble *control) {
  if (!control)
    return 0.0f;

  double typedValue = 0.0;
  if (control->GetTextValue().ToDouble(&typedValue))
    return static_cast<float>(typedValue);
  return static_cast<float>(control->GetValue());
}

} // namespace

Viewer2DRenderPanel *Viewer2DRenderPanel::s_instance = nullptr;

Viewer2DRenderPanel::Viewer2DRenderPanel(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL) {
  SetInstance(this);
  Bind(wxEVT_CHAR_HOOK, &Viewer2DRenderPanel::OnNumericCharHook, this);
  ConfigManager &cfg = ConfigManager::Get();
  wxString choices[] = {_("Wireframe"), _("White"), _("By device type"),
                        _("By layer"), _("By universe")};
  m_radio = new wxRadioBox(this, wxID_ANY, _("Render mode"), wxDefaultPosition,
                           wxDefaultSize, 5, choices, 1, wxRA_SPECIFY_COLS);
  m_radio->SetSelection(static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_radio->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnRadio, this);

  m_darkMode = new wxCheckBox(this, wxID_ANY, _("Dark mode"));
  m_darkMode->SetValue(cfg.GetFloat("view2d_dark_mode") != 0.0f);
  m_darkMode->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnDarkMode, this);

  m_topFixturesInverted =
      new wxCheckBox(this, wxID_ANY, _("Force bottom view"));
  m_topFixturesInverted->SetValue(
      cfg.GetFloat("view2d_top_fixtures_inverted") != 0.0f);
  m_topFixturesInverted->Bind(wxEVT_CHECKBOX,
                              &Viewer2DRenderPanel::OnTopFixturesInverted,
                              this);

  wxString viewChoices[] = {_("Top"), _("Front"), _("Side")};
  m_view = new wxRadioBox(this, wxID_ANY, _("View"), wxDefaultPosition,
                          wxDefaultSize, 3, viewChoices, 1, wxRA_SPECIFY_COLS);
  m_view->SetSelection(static_cast<int>(cfg.GetFloat("view2d_view")));
  m_view->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnView, this);

  auto *gridBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Grid"));
  wxWindow *gridBoxParent = gridBox->GetStaticBox();

  m_showGrid = new wxCheckBox(gridBoxParent, wxID_ANY, _("Show grid"));
  m_showGrid->SetValue(cfg.GetFloat("grid_show") != 0.0f);
  m_showGrid->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowGrid, this);

  wxString gridChoices[] = {_("Lines"), _("Points"), _("Crosses")};
  m_gridStyle = new wxRadioBox(gridBoxParent, wxID_ANY, _("Grid style"),
                               wxDefaultPosition, wxDefaultSize, 3,
                               gridChoices, 1, wxRA_SPECIFY_COLS);
  m_gridStyle->SetSelection(static_cast<int>(cfg.GetFloat("grid_style")));
  m_gridStyle->Bind(wxEVT_RADIOBOX, &Viewer2DRenderPanel::OnGridStyle, this);

  int rr = static_cast<int>(cfg.GetFloat("grid_color_r") * 255.0f);
  int gg = static_cast<int>(cfg.GetFloat("grid_color_g") * 255.0f);
  int bb = static_cast<int>(cfg.GetFloat("grid_color_b") * 255.0f);
  m_gridColor =
      new wxColourPickerCtrl(gridBoxParent, wxID_ANY, wxColour(rr, gg, bb));
  m_gridColor->Bind(wxEVT_COLOURPICKER_CHANGED,
                    &Viewer2DRenderPanel::OnGridColor, this);

  m_drawAbove = new wxCheckBox(gridBoxParent, wxID_ANY, _("Draw grid on top"));
  m_drawAbove->SetValue(cfg.GetFloat("grid_draw_above") != 0.0f);
  m_drawAbove->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnDrawAbove, this);

  auto *rulerBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Ruler"));
  wxWindow *rulerBoxParent = rulerBox->GetStaticBox();

  m_showRuler = new wxCheckBox(rulerBoxParent, wxID_ANY, _("Show ruler"));
  m_showRuler->SetValue(cfg.GetFloat("ruler_show") != 0.0f);
  m_showRuler->Bind(wxEVT_CHECKBOX, &Viewer2DRenderPanel::OnShowRuler, this);
  m_rulerAxisXPosition = new wxSpinCtrlDouble(
      rulerBoxParent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_rulerAxisXPosition->SetRange(-100.0, 100.0);
  m_rulerAxisXPosition->SetIncrement(0.1);
  m_rulerAxisXPosition->SetDigits(2);
  m_rulerAxisXPosition->SetValue(cfg.GetFloat("ruler_axis_x_position"));
  m_rulerAxisXPosition->Bind(wxEVT_SPINCTRLDOUBLE,
                             &Viewer2DRenderPanel::OnRulerAxisXPosition, this);
  m_rulerAxisXPosition->Bind(wxEVT_SET_FOCUS,
                             &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_rulerAxisXPosition->Bind(wxEVT_KILL_FOCUS,
                             &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_rulerAxisXPosition->Bind(wxEVT_TEXT, &Viewer2DRenderPanel::OnTextChange,
                             this);
  m_rulerAxisXPosition->Bind(wxEVT_TEXT_ENTER,
                             &Viewer2DRenderPanel::OnTextEnter, this);
  m_rulerAxisXPosition->SetMinSize(wxSize(kRulerAxisPositionWidth, -1));
  m_rulerAxisXColor = new wxColourPickerCtrl(
      rulerBoxParent, wxID_ANY,
      BuildColorFromConfig(cfg, "ruler_axis_x_color_r", "ruler_axis_x_color_g",
                           "ruler_axis_x_color_b"));
  m_rulerAxisXColor->SetMinSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisXColor->SetMaxSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisXColor->Bind(wxEVT_COLOURPICKER_CHANGED,
                          &Viewer2DRenderPanel::OnRulerAxisXColor, this);

  m_rulerAxisYPosition = new wxSpinCtrlDouble(
      rulerBoxParent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_rulerAxisYPosition->SetRange(-100.0, 100.0);
  m_rulerAxisYPosition->SetIncrement(0.1);
  m_rulerAxisYPosition->SetDigits(2);
  m_rulerAxisYPosition->SetValue(cfg.GetFloat("ruler_axis_y_position"));
  m_rulerAxisYPosition->Bind(wxEVT_SPINCTRLDOUBLE,
                             &Viewer2DRenderPanel::OnRulerAxisYPosition, this);
  m_rulerAxisYPosition->Bind(wxEVT_SET_FOCUS,
                             &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_rulerAxisYPosition->Bind(wxEVT_KILL_FOCUS,
                             &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_rulerAxisYPosition->Bind(wxEVT_TEXT, &Viewer2DRenderPanel::OnTextChange,
                             this);
  m_rulerAxisYPosition->Bind(wxEVT_TEXT_ENTER,
                             &Viewer2DRenderPanel::OnTextEnter, this);
  m_rulerAxisYPosition->SetMinSize(wxSize(kRulerAxisPositionWidth, -1));
  m_rulerAxisYColor = new wxColourPickerCtrl(
      rulerBoxParent, wxID_ANY,
      BuildColorFromConfig(cfg, "ruler_axis_y_color_r", "ruler_axis_y_color_g",
                           "ruler_axis_y_color_b"));
  m_rulerAxisYColor->SetMinSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisYColor->SetMaxSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisYColor->Bind(wxEVT_COLOURPICKER_CHANGED,
                          &Viewer2DRenderPanel::OnRulerAxisYColor, this);
  m_rulerAxisZPosition = new wxSpinCtrlDouble(
      rulerBoxParent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER);
  m_rulerAxisZPosition->SetRange(-100.0, 100.0);
  m_rulerAxisZPosition->SetIncrement(0.1);
  m_rulerAxisZPosition->SetDigits(2);
  m_rulerAxisZPosition->SetValue(cfg.GetFloat("ruler_axis_z_position"));
  m_rulerAxisZPosition->Bind(wxEVT_SPINCTRLDOUBLE,
                             &Viewer2DRenderPanel::OnRulerAxisZPosition, this);
  m_rulerAxisZPosition->Bind(wxEVT_SET_FOCUS,
                             &Viewer2DRenderPanel::OnBeginTextEdit, this);
  m_rulerAxisZPosition->Bind(wxEVT_KILL_FOCUS,
                             &Viewer2DRenderPanel::OnEndTextEdit, this);
  m_rulerAxisZPosition->Bind(wxEVT_TEXT, &Viewer2DRenderPanel::OnTextChange,
                             this);
  m_rulerAxisZPosition->Bind(wxEVT_TEXT_ENTER,
                             &Viewer2DRenderPanel::OnTextEnter, this);
  m_rulerAxisZPosition->SetMinSize(wxSize(kRulerAxisPositionWidth, -1));
  m_rulerAxisZColor = new wxColourPickerCtrl(
      rulerBoxParent, wxID_ANY,
      BuildColorFromConfig(cfg, "ruler_axis_z_color_r", "ruler_axis_z_color_g",
                           "ruler_axis_z_color_b"));
  m_rulerAxisZColor->SetMinSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisZColor->SetMaxSize(
      wxSize(kRulerAxisColorSwatchSize, kRulerAxisColorSwatchSize));
  m_rulerAxisZColor->Bind(wxEVT_COLOURPICKER_CHANGED,
                          &Viewer2DRenderPanel::OnRulerAxisZColor, this);

  auto *sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(m_radio, 0, wxEXPAND | wxALL, 5);
  sizer->Add(m_darkMode, 0, wxALL, 5);
  sizer->Add(m_topFixturesInverted, 0, wxALL, 5);
  sizer->Add(m_view, 0, wxEXPAND | wxALL, 5);

  gridBox->Add(m_showGrid, 0, wxALL, 5);
  gridBox->Add(m_gridStyle, 0, wxALL, 5);
  auto *colorSizer = new wxBoxSizer(wxHORIZONTAL);
  colorSizer->Add(new wxStaticText(gridBoxParent, wxID_ANY, _("Color")), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  colorSizer->Add(m_gridColor, 0);
  gridBox->Add(colorSizer, 0, wxALL, 5);
  gridBox->Add(m_drawAbove, 0, wxALL, 5);
  sizer->Add(gridBox, 0, wxEXPAND | wxALL, 5);

  rulerBox->Add(m_showRuler, 0, wxALL, 5);
  auto *rulerAxisXSizer = new wxBoxSizer(wxHORIZONTAL);
  rulerAxisXSizer->Add(new wxStaticText(rulerBoxParent, wxID_ANY, _("X ruler")),
                       0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  rulerAxisXSizer->Add(m_rulerAxisXPosition, 0, wxRIGHT, 4);
  rulerAxisXSizer->Add(m_rulerAxisXColor, 0);
  rulerBox->Add(rulerAxisXSizer, 0, wxALL, 5);
  auto *rulerAxisYSizer = new wxBoxSizer(wxHORIZONTAL);
  rulerAxisYSizer->Add(new wxStaticText(rulerBoxParent, wxID_ANY, _("Y ruler")),
                       0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  rulerAxisYSizer->Add(m_rulerAxisYPosition, 0, wxRIGHT, 4);
  rulerAxisYSizer->Add(m_rulerAxisYColor, 0);
  rulerBox->Add(rulerAxisYSizer, 0, wxALL, 5);
  auto *rulerAxisZSizer = new wxBoxSizer(wxHORIZONTAL);
  rulerAxisZSizer->Add(new wxStaticText(rulerBoxParent, wxID_ANY, _("Z ruler")),
                       0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  rulerAxisZSizer->Add(m_rulerAxisZPosition, 0, wxRIGHT, 4);
  rulerAxisZSizer->Add(m_rulerAxisZColor, 0);
  rulerBox->Add(rulerAxisZSizer, 0, wxALL, 5);
  sizer->Add(rulerBox, 0, wxEXPAND | wxALL, 5);

  auto *editLabels = new wxButton(this, wxID_ANY, _("Edit Fixture Labels..."));
  editLabels->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
    FixtureLabelEditor editor(this, ConfigManager::Get(), m_view->GetSelection());
    editor.ShowModal();
  });
  sizer->Add(editLabels, 0, wxEXPAND | wxALL, 5);

  SetSizer(sizer);
  FitInside();
  SetScrollRate(0, 10);
  Layout();
  ApplyConfig();
}

Viewer2DRenderPanel::~Viewer2DRenderPanel() {
  if (s_instance == this)
    s_instance = nullptr;
}

Viewer2DRenderPanel *Viewer2DRenderPanel::Instance() { return s_instance; }

void Viewer2DRenderPanel::SetInstance(Viewer2DRenderPanel *p) {
  s_instance = p;
}

void Viewer2DRenderPanel::SetViewSelection(Viewer2DView view) {
  ApplyViewSelection(static_cast<int>(view));
}

void Viewer2DRenderPanel::RefreshLabelControlsFromSelection() {}

void Viewer2DRenderPanel::ApplyConfig() {
  ConfigManager &cfg = ConfigManager::Get();
  m_radio->SetSelection(static_cast<int>(cfg.GetFloat("view2d_render_mode")));
  m_darkMode->SetValue(cfg.GetFloat("view2d_dark_mode") != 0.0f);
  m_topFixturesInverted->SetValue(
      cfg.GetFloat("view2d_top_fixtures_inverted") != 0.0f);
  m_view->SetSelection(static_cast<int>(cfg.GetFloat("view2d_view")));
  m_showGrid->SetValue(cfg.GetFloat("grid_show") != 0.0f);
  m_gridStyle->SetSelection(static_cast<int>(cfg.GetFloat("grid_style")));
  int rr = static_cast<int>(cfg.GetFloat("grid_color_r") * 255.0f);
  int gg = static_cast<int>(cfg.GetFloat("grid_color_g") * 255.0f);
  int bb = static_cast<int>(cfg.GetFloat("grid_color_b") * 255.0f);
  m_gridColor->SetColour(wxColour(rr, gg, bb));
  m_drawAbove->SetValue(cfg.GetFloat("grid_draw_above") != 0.0f);
  m_showRuler->SetValue(cfg.GetFloat("ruler_show") != 0.0f);
  m_rulerAxisXPosition->SetValue(cfg.GetFloat("ruler_axis_x_position"));
  m_rulerAxisYPosition->SetValue(cfg.GetFloat("ruler_axis_y_position"));
  m_rulerAxisZPosition->SetValue(cfg.GetFloat("ruler_axis_z_position"));
  m_rulerAxisXColor->SetColour(
      BuildColorFromConfig(cfg, "ruler_axis_x_color_r", "ruler_axis_x_color_g",
                           "ruler_axis_x_color_b"));
  m_rulerAxisYColor->SetColour(
      BuildColorFromConfig(cfg, "ruler_axis_y_color_r", "ruler_axis_y_color_g",
                           "ruler_axis_y_color_b"));
  m_rulerAxisZColor->SetColour(
      BuildColorFromConfig(cfg, "ruler_axis_z_color_r", "ruler_axis_z_color_g",
                           "ruler_axis_z_color_b"));
  UpdateRulerControlState();
  RefreshLabelControlsFromSelection();
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

void Viewer2DRenderPanel::OnDarkMode(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("view2d_dark_mode",
                                m_darkMode->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance()) {
    vp->UpdateScene(true);
    vp->Update();
  }
  evt.Skip();
}

void Viewer2DRenderPanel::OnTopFixturesInverted(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("view2d_top_fixtures_inverted",
                                m_topFixturesInverted->GetValue() ? 1.0f
                                                                  : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
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

void Viewer2DRenderPanel::OnShowRuler(wxCommandEvent &evt) {
  ConfigManager::Get().SetFloat("ruler_show",
                                m_showRuler->GetValue() ? 1.0f : 0.0f);
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisXPosition(wxSpinDoubleEvent &evt) {
  ConfigManager::Get().SetFloat(
      "ruler_axis_x_position",
      static_cast<float>(m_rulerAxisXPosition->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisYPosition(wxSpinDoubleEvent &evt) {
  ConfigManager::Get().SetFloat(
      "ruler_axis_y_position",
      static_cast<float>(m_rulerAxisYPosition->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisZPosition(wxSpinDoubleEvent &evt) {
  ConfigManager::Get().SetFloat(
      "ruler_axis_z_position",
      static_cast<float>(m_rulerAxisZPosition->GetValue()));
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisXColor(wxColourPickerEvent &evt) {
  ConfigManager &cfg = ConfigManager::Get();
  SaveColorToConfig(cfg, evt.GetColour(), "ruler_axis_x_color_r",
                    "ruler_axis_x_color_g", "ruler_axis_x_color_b");
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisYColor(wxColourPickerEvent &evt) {
  ConfigManager &cfg = ConfigManager::Get();
  SaveColorToConfig(cfg, evt.GetColour(), "ruler_axis_y_color_r",
                    "ruler_axis_y_color_g", "ruler_axis_y_color_b");
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnRulerAxisZColor(wxColourPickerEvent &evt) {
  ConfigManager &cfg = ConfigManager::Get();
  SaveColorToConfig(cfg, evt.GetColour(), "ruler_axis_z_color_r",
                    "ruler_axis_z_color_g", "ruler_axis_z_color_b");
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnView(wxCommandEvent &evt) {
  ApplyViewSelection(m_view->GetSelection());
  evt.Skip();
}

void Viewer2DRenderPanel::ApplyViewSelection(int selection) {
  ConfigManager &cfg = ConfigManager::Get();
  if (auto *mw = MainWindow::Instance(); mw && mw->IsLayout2DViewEditing())
    mw->PersistLayout2DViewState();
  const int sel = std::clamp(selection, 0, 2);
  m_view->SetSelection(sel);
  cfg.SetFloat("view2d_view", static_cast<float>(sel));
  RefreshLabelControlsFromSelection();
  UpdateRulerControlState();
  if (auto *vp = Viewer2DPanel::Instance()) {
    vp->SetView(static_cast<Viewer2DView>(sel));
    vp->UpdateScene(false);
  }
}

void Viewer2DRenderPanel::UpdateRulerControlState() {
  const Viewer2DView selectedView =
      static_cast<Viewer2DView>(m_view->GetSelection());
  const bool showX = selectedView == Viewer2DView::Top ||
                     selectedView == Viewer2DView::Bottom ||
                     selectedView == Viewer2DView::Front;
  const bool showY = selectedView == Viewer2DView::Top ||
                     selectedView == Viewer2DView::Bottom ||
                     selectedView == Viewer2DView::Side;
  const bool showZ = selectedView == Viewer2DView::Front ||
                     selectedView == Viewer2DView::Side;
  m_rulerAxisXPosition->Enable(showX);
  m_rulerAxisYPosition->Enable(showY);
  m_rulerAxisZPosition->Enable(showZ);
  m_rulerAxisXColor->Enable(showX);
  m_rulerAxisYColor->Enable(showY);
  m_rulerAxisZColor->Enable(showZ);
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

void Viewer2DRenderPanel::OnTextChange(wxCommandEvent &evt) {
  if (auto *mw = MainWindow::Instance())
    mw->EnableShortcuts(false);
  evt.Skip();
}

void Viewer2DRenderPanel::OnTextEnter(wxCommandEvent &evt) {
  ConfigManager &cfg = ConfigManager::Get();
  if (evt.GetEventObject() == m_rulerAxisXPosition) {
    cfg.SetFloat("ruler_axis_x_position", CurrentSpinDoubleValue(m_rulerAxisXPosition));
  } else if (evt.GetEventObject() == m_rulerAxisYPosition) {
    cfg.SetFloat("ruler_axis_y_position", CurrentSpinDoubleValue(m_rulerAxisYPosition));
  } else if (evt.GetEventObject() == m_rulerAxisZPosition) {
    cfg.SetFloat("ruler_axis_z_position", CurrentSpinDoubleValue(m_rulerAxisZPosition));
  }
  if (auto *vp = Viewer2DPanel::Instance())
    vp->UpdateScene(false);
  if (auto *mw = MainWindow::Instance())
    mw->EnableShortcuts(true);
  evt.Skip();
}

wxWindow *
Viewer2DRenderPanel::ResolveNumericCommitSource(const wxWindow *focused) const {
  if (!focused)
    return nullptr;

  auto isChildOrSame = [focused](const wxWindow *parent) {
    const wxWindow *current = focused;
    while (current) {
      if (current == parent)
        return true;
      current = current->GetParent();
    }
    return false;
  };

  if (m_rulerAxisXPosition && isChildOrSame(m_rulerAxisXPosition))
    return m_rulerAxisXPosition;
  if (m_rulerAxisYPosition && isChildOrSame(m_rulerAxisYPosition))
    return m_rulerAxisYPosition;
  if (m_rulerAxisZPosition && isChildOrSame(m_rulerAxisZPosition))
    return m_rulerAxisZPosition;

  return nullptr;
}

void Viewer2DRenderPanel::OnNumericCharHook(wxKeyEvent &evt) {
  const int keyCode = evt.GetKeyCode();
  if (keyCode != WXK_RETURN && keyCode != WXK_NUMPAD_ENTER) {
    evt.Skip();
    return;
  }

  wxWindow *source = ResolveNumericCommitSource(wxWindow::FindFocus());
  if (!source) {
    evt.Skip();
    return;
  }

  wxCommandEvent enterEvent(wxEVT_TEXT_ENTER, source->GetId());
  enterEvent.SetEventObject(source);
  OnTextEnter(enterEvent);
}
