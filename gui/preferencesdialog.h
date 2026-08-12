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

#include "localization/app_language.h"
#include <array>
#include <wx/wx.h>

wxDECLARE_EVENT(EVT_UI_UNITS_CHANGED, wxCommandEvent);
wxDECLARE_EVENT(EVT_UI_PREFERENCES_APPLIED, wxCommandEvent);

class GdtfCredentialsPanel;

class PreferencesDialog : public wxDialog {
public:
  PreferencesDialog(wxWindow *parent);

private:
  void OnApplyButton(wxCommandEvent &event);
  void OnOkButton(wxCommandEvent &event);
  void NotifyPreferencesApplied();
  bool ApplyPreferences();
  void NotifyUnitsChanged();
  void RefreshRiderImportDistanceLabels();
  void ConvertRiderImportDistanceFields();
  void
  ShowLanguageRestartNoticeIfNeeded(localization::AppLanguage selectedLanguage);

  std::array<wxTextCtrl *, 6> lxHeightCtrls{};
  std::array<wxTextCtrl *, 6> lxPosCtrls{};
  std::array<wxTextCtrl *, 6> lxMarginCtrls{};
  std::array<wxStaticText *, 6> lxHeightLabels{};
  std::array<wxStaticText *, 6> lxPosLabels{};
  std::array<wxStaticText *, 6> lxMarginLabels{};
  wxCheckBox *autopatchCheck = nullptr;
  wxRadioButton *layerPosRadio = nullptr;
  wxRadioButton *layerTypeRadio = nullptr;
  wxRadioButton *viewer3dStandardRenderRadio = nullptr;
  wxRadioButton *viewer3dWhiteRenderRadio = nullptr;
  wxRadioButton *viewer3dWhiteModelRenderRadio = nullptr;
  wxRadioButton *viewer3dTexturedRenderRadio = nullptr;
  wxRadioButton *viewer3dWireframeRenderRadio = nullptr;
  wxRadioButton *viewer3dByDeviceTypeRenderRadio = nullptr;
  wxRadioButton *viewer3dByLayerRenderRadio = nullptr;
  wxRadioButton *viewer3dByUniverseRenderRadio = nullptr;
  wxCheckBox *viewer3dInvertOrbitCheck = nullptr;
  wxCheckBox *groupMoveFixtureCheck = nullptr;
  wxCheckBox *groupMoveTrussCheck = nullptr;
  wxCheckBox *groupMoveSupportCheck = nullptr;
  wxCheckBox *groupMoveSceneObjectCheck = nullptr;
  wxCheckBox *magnetAnchorReferencesCheck = nullptr;
  wxChoice *distanceUnitChoice = nullptr;
  wxChoice *weightUnitChoice = nullptr;
  wxChoice *updateCheckModeChoice = nullptr;
  wxChoice *mvrTrussGeometryExportModeChoice = nullptr;
  wxChoice *interfaceLanguageChoice = nullptr;
  int initialDistanceUnitSelection = wxNOT_FOUND;
  int initialWeightUnitSelection = wxNOT_FOUND;
  localization::AppLanguage lastRestartNoticeLanguage =
      localization::DefaultAppLanguage();
  GdtfCredentialsPanel *gdtfCredentialsPanel = nullptr;
};
