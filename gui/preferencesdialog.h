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

#include <array>
#include <wx/wx.h>

namespace print {
class PlanPrintSettings;
}

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
  wxRadioButton *pageSizeA3Radio = nullptr;
  wxRadioButton *pageSizeA4Radio = nullptr;
  wxRadioButton *portraitRadio = nullptr;
  wxRadioButton *landscapeRadio = nullptr;
  wxCheckBox *includeGridCheck = nullptr;
  wxRadioButton *detailedRadio = nullptr;
  wxRadioButton *schematicRadio = nullptr;

  void LoadPlanPrintSettings(const print::PlanPrintSettings &settings);
  void SavePlanPrintSettings(print::PlanPrintSettings &settings);
};
