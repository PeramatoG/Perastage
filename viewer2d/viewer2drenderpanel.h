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
#pragma once

#include "viewer2dpanel.h"
#include <wx/clrpicker.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

class Viewer2DRenderPanel : public wxScrolledWindow {
public:
  explicit Viewer2DRenderPanel(wxWindow *parent);
  ~Viewer2DRenderPanel() override;

  void ApplyConfig();
  void SetViewSelection(Viewer2DView view);
  void RefreshLabelControlsFromSelection();

  static Viewer2DRenderPanel *Instance();
  static void SetInstance(Viewer2DRenderPanel *p);

private:
  void OnRadio(wxCommandEvent &evt);
  void OnDarkMode(wxCommandEvent &evt);
  void OnTopFixturesInverted(wxCommandEvent &evt);
  void OnShowGrid(wxCommandEvent &evt);
  void OnGridStyle(wxCommandEvent &evt);
  void OnGridColor(wxColourPickerEvent &evt);
  void OnDrawAbove(wxCommandEvent &evt);
  void OnShowRuler(wxCommandEvent &evt);
  void OnRulerAxisXPosition(wxSpinDoubleEvent &evt);
  void OnRulerAxisYPosition(wxSpinDoubleEvent &evt);
  void OnRulerAxisZPosition(wxSpinDoubleEvent &evt);
  void OnRulerAxisXColor(wxColourPickerEvent &evt);
  void OnRulerAxisYColor(wxColourPickerEvent &evt);
  void OnRulerAxisZColor(wxColourPickerEvent &evt);
  void OnView(wxCommandEvent &evt);
  void OnBeginTextEdit(wxFocusEvent &evt);
  void OnEndTextEdit(wxFocusEvent &evt);
  void OnTextChange(wxCommandEvent &evt);
  void OnTextEnter(wxCommandEvent &evt);
  void OnNumericCharHook(wxKeyEvent &evt);
  wxWindow *ResolveNumericCommitSource(const wxWindow *focused) const;
  void ApplyViewSelection(int selection);
  void UpdateRulerControlState();

  wxRadioBox *m_radio = nullptr;
  wxCheckBox *m_darkMode = nullptr;
  wxCheckBox *m_topFixturesInverted = nullptr;
  wxRadioBox *m_view = nullptr;
  wxCheckBox *m_showGrid = nullptr;
  wxRadioBox *m_gridStyle = nullptr;
  wxColourPickerCtrl *m_gridColor = nullptr;
  wxCheckBox *m_drawAbove = nullptr;
  wxCheckBox *m_showRuler = nullptr;
  wxSpinCtrlDouble *m_rulerAxisXPosition = nullptr;
  wxSpinCtrlDouble *m_rulerAxisYPosition = nullptr;
  wxSpinCtrlDouble *m_rulerAxisZPosition = nullptr;
  wxColourPickerCtrl *m_rulerAxisXColor = nullptr;
  wxColourPickerCtrl *m_rulerAxisYColor = nullptr;
  wxColourPickerCtrl *m_rulerAxisZColor = nullptr;
  static Viewer2DRenderPanel *s_instance;
};
