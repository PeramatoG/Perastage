#pragma once

#include "viewer2dpanel.h"
#include <wx/clrpicker.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

class Viewer2DRenderPanel : public wxScrolledWindow {
public:
  explicit Viewer2DRenderPanel(wxWindow *parent);

  void ApplyConfig();

  static Viewer2DRenderPanel *Instance();
  static void SetInstance(Viewer2DRenderPanel *p);

private:
  void OnRadio(wxCommandEvent &evt);
  void OnShowGrid(wxCommandEvent &evt);
  void OnGridStyle(wxCommandEvent &evt);
  void OnGridColor(wxColourPickerEvent &evt);
  void OnDrawAbove(wxCommandEvent &evt);
  void OnShowLabelName(wxCommandEvent &evt);
  void OnShowLabelId(wxCommandEvent &evt);
  void OnShowLabelAddress(wxCommandEvent &evt);
  void OnLabelNameSize(wxSpinEvent &evt);
  void OnLabelIdSize(wxSpinEvent &evt);
  void OnLabelAddressSize(wxSpinEvent &evt);
  void OnLabelOffsetDistance(wxSpinDoubleEvent &evt);
  void OnLabelOffsetAngle(wxSpinEvent &evt);
  void OnView(wxCommandEvent &evt);
  void OnBeginTextEdit(wxFocusEvent &evt);
  void OnEndTextEdit(wxFocusEvent &evt);
  void OnTextEnter(wxCommandEvent &evt);

  wxRadioBox *m_radio = nullptr;
  wxRadioBox *m_view = nullptr;
  wxCheckBox *m_showGrid = nullptr;
  wxRadioBox *m_gridStyle = nullptr;
  wxColourPickerCtrl *m_gridColor = nullptr;
  wxCheckBox *m_drawAbove = nullptr;
  wxCheckBox *m_showLabelName = nullptr;
  wxCheckBox *m_showLabelId = nullptr;
  wxCheckBox *m_showLabelAddress = nullptr;
  wxSpinCtrl *m_labelNameSize = nullptr;
  wxSpinCtrl *m_labelIdSize = nullptr;
  wxSpinCtrl *m_labelAddressSize = nullptr;
  wxSpinCtrlDouble *m_labelOffsetDistance = nullptr;
  wxSpinCtrl *m_labelOffsetAngle = nullptr;
  static Viewer2DRenderPanel *s_instance;
};
