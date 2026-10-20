#pragma once
#include "fixture_label_composition.h"
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/spinctrl.h>
#include <wx/clrpicker.h>
#include <functional>

class FixtureLabelPreview;
class FixtureLabelEditor : public wxDialog {
public:
  FixtureLabelEditor(wxWindow *parent, ConfigManager &cfg, int initialView);
private:
  void LoadField();
  void StoreField();
  void Apply();
  void Reset();
  void RefreshPreview();
  ConfigManager &cfg_;
  viewer2d::FixtureLabelOverrideMap draft_;
  std::vector<std::string> selection_;
  Fixture fixture_;
  wxChoice *scope_ = nullptr, *view_ = nullptr, *anchor_ = nullptr, *align_ = nullptr;
  wxListBox *fields_ = nullptr;
  wxCheckBox *visible_ = nullptr, *bold_ = nullptr;
  wxSpinCtrlDouble *size_ = nullptr, *x_ = nullptr, *y_ = nullptr;
  wxColourPickerCtrl *color_ = nullptr;
  FixtureLabelPreview *preview_ = nullptr;
  bool loading_ = false, changed_ = false;
  std::string ScopeUuid() const;
};
