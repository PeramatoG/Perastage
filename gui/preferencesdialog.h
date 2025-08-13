#pragma once

#include <array>
#include <wx/wx.h>

class PreferencesDialog : public wxDialog {
public:
  PreferencesDialog(wxWindow *parent);

private:
  std::array<wxTextCtrl *, 6> lxHeightCtrls{};
  std::array<wxTextCtrl *, 6> lxPosCtrls{};
  std::array<wxTextCtrl *, 6> lxMarginCtrls{};
  wxCheckBox *autopatchCheck = nullptr;
  wxRadioButton *layerPosRadio = nullptr;
  wxRadioButton *layerTypeRadio = nullptr;
};
