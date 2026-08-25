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
#include "preferencesdialog.h"
#include "../viewer_common/viewport_mouse_navigation.h"
#include "configmanager.h"
#include "guiconfigservices.h"
#include "localization/localization_manager.h"
#include "magnet_snap.h"
#include "mvr_preferences.h"
#include "preferences/gdtf_credentials_panel.h"
#include "selection_movement_settings.h"
#include "units/units.h"
#include "update/update_check_preferences.h"
#include "viewer3d_render_style.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/font.h>
#include <wx/notebook.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/statline.h>

wxDEFINE_EVENT(EVT_UI_UNITS_CHANGED, wxCommandEvent);
wxDEFINE_EVENT(EVT_UI_PREFERENCES_APPLIED, wxCommandEvent);

namespace {

// Resolves the stable distance unit system from the selected choice index.
Units::DistanceUnitSystem DistanceUnitSystemFromChoice(const wxChoice *choice) {
  if (choice && choice->GetSelection() == 1)
    return Units::DistanceUnitSystem::Imperial;
  return Units::DistanceUnitSystem::Metric;
}

// Returns the native display label for a supported language option.
wxString NativeLanguageDisplayName(localization::AppLanguage language) {
  switch (language) {
  case localization::AppLanguage::Spanish: {
    wxString name("Espa");
    name += wxUniChar(0x00F1);
    name += "ol";
    return name;
  }
  case localization::AppLanguage::SimplifiedChinese: {
    wxString name;
    name += wxUniChar(0x7B80);
    name += wxUniChar(0x4F53);
    name += wxUniChar(0x4E2D);
    name += wxUniChar(0x6587);
    return name;
  }
  case localization::AppLanguage::English:
  default:
    return "English";
  }
}

} // namespace

// Builds the preferences dialog and initializes controls from stored
// configuration.
PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _("Preferences"), wxDefaultPosition,
               wxDefaultSize) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxNotebook *book = new wxNotebook(this, wxID_ANY);

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();

  // Rider Import page
  wxPanel *riderPanel = new wxPanel(book);
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  autopatchCheck =
      new wxCheckBox(riderPanel, wxID_ANY, _("Auto patch after import"));
  auto autoVal = cfg.GetValue("rider_autopatch");
  autopatchCheck->SetValue(!autoVal || *autoVal != "0");
  riderSizer->Add(autopatchCheck, 0, wxALL, 10);
  layerPosRadio = new wxRadioButton(
      riderPanel, wxID_ANY, _("Auto-create layers by position"),
      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  layerTypeRadio = new wxRadioButton(riderPanel, wxID_ANY,
                                     _("Auto-create layers by fixture type"));
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool byType = modeVal && *modeVal == "type";
  layerTypeRadio->SetValue(byType);
  layerPosRadio->SetValue(!byType);
  riderSizer->Add(layerPosRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  riderSizer->Add(layerTypeRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  wxFlexGridSizer *grid = new wxFlexGridSizer(6, 5, 5);
  grid->AddGrowableCol(1, 1);
  grid->AddGrowableCol(3, 1);
  grid->AddGrowableCol(5, 1);
  const auto riderDistanceUnit =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));

  for (int i = 0; i < 6; ++i) {
    lxHeightLabels[i] = new wxStaticText(
        riderPanel, wxID_ANY, wxString::Format(_("LX%d height:"), i + 1));
    grid->Add(lxHeightLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valH = static_cast<double>(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_height"));
    lxHeightCtrls[i] =
        new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                          valH * 1000.0, riderDistanceUnit,
                                          Units::ValueFormatContext::Label)));
    grid->Add(lxHeightCtrls[i], 1, wxEXPAND);

    lxPosLabels[i] = new wxStaticText(
        riderPanel, wxID_ANY, wxString::Format(_("LX%d position:"), i + 1));
    grid->Add(lxPosLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valP = static_cast<double>(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_pos"));
    lxPosCtrls[i] =
        new wxTextCtrl(riderPanel, wxID_ANY,
                                   wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                       valP * 1000.0, riderDistanceUnit,
                                       Units::ValueFormatContext::Label)));
    grid->Add(lxPosCtrls[i], 1, wxEXPAND);

    lxMarginLabels[i] = new wxStaticText(
        riderPanel, wxID_ANY, wxString::Format(_("LX%d margin:"), i + 1));
    grid->Add(lxMarginLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valM = static_cast<double>(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_margin"));
    lxMarginCtrls[i] =
        new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                          valM * 1000.0, riderDistanceUnit,
                                          Units::ValueFormatContext::Label)));
    grid->Add(lxMarginCtrls[i], 1, wxEXPAND);
  }
  riderSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
  riderPanel->SetSizer(riderSizer);
  book->AddPage(riderPanel, _("Rider Import"));

  // Units page
  wxPanel *unitsPanel = new wxPanel(book);
  wxBoxSizer *unitsSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *unitsGrid = new wxFlexGridSizer(2, 2, 10, 10);
  unitsGrid->AddGrowableCol(1, 1);

  unitsGrid->Add(new wxStaticText(unitsPanel, wxID_ANY, _("Distance system:")),
                 0, wxALIGN_CENTER_VERTICAL);
  distanceUnitChoice = new wxChoice(unitsPanel, wxID_ANY);
  distanceUnitChoice->Append(_("Metric"));
  distanceUnitChoice->Append(_("Imperial"));
  auto distanceUnitValue = cfg.GetValue("ui_distance_unit_system");
  const bool hasImperialDistance =
      distanceUnitValue && *distanceUnitValue == "imperial";
  distanceUnitChoice->SetSelection(hasImperialDistance ? 1 : 0);
  RefreshRiderImportDistanceLabels();
  distanceUnitChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
    ConvertRiderImportDistanceFields();
    RefreshRiderImportDistanceLabels();
  });
  initialDistanceUnitSelection = distanceUnitChoice->GetSelection();
  unitsGrid->Add(distanceUnitChoice, 1, wxEXPAND);

  unitsGrid->Add(new wxStaticText(unitsPanel, wxID_ANY, _("Weight system:")), 0,
                 wxALIGN_CENTER_VERTICAL);
  weightUnitChoice = new wxChoice(unitsPanel, wxID_ANY);
  weightUnitChoice->Append(_("Metric"));
  weightUnitChoice->Append(_("Imperial"));
  auto weightUnitValue = cfg.GetValue("ui_weight_unit_system");
  const bool hasImperialWeight =
      weightUnitValue && *weightUnitValue == "imperial";
  weightUnitChoice->SetSelection(hasImperialWeight ? 1 : 0);
  initialWeightUnitSelection = weightUnitChoice->GetSelection();
  unitsGrid->Add(weightUnitChoice, 1, wxEXPAND);

  unitsSizer->Add(unitsGrid, 0, wxALL | wxEXPAND, 10);
  unitsPanel->SetSizer(unitsSizer);
  book->AddPage(unitsPanel, _("Units"));

  // Language page
  wxPanel *languagePanel = new wxPanel(book);
  wxBoxSizer *languageSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *languageGrid = new wxFlexGridSizer(1, 2, 10, 10);
  languageGrid->AddGrowableCol(1, 1);
  languageGrid->Add(
      new wxStaticText(languagePanel, wxID_ANY, _("Interface language:")), 0,
      wxALIGN_CENTER_VERTICAL);
  interfaceLanguageChoice = new wxChoice(languagePanel, wxID_ANY);
  for (const auto &option : localization::SupportedAppLanguages()) {
    interfaceLanguageChoice->Append(NativeLanguageDisplayName(option.language));
  }
  const auto configuredLanguage = localization::ParseAppLanguageCode(
      cfg.GetValue(localization::kUiLanguageConfigKey).value_or(""));
  int languageSelection = 0;
  const auto &languages = localization::SupportedAppLanguages();
  for (std::size_t i = 0; i < languages.size(); ++i) {
    if (languages[i].language == configuredLanguage) {
      languageSelection = static_cast<int>(i);
      break;
    }
  }
  interfaceLanguageChoice->SetSelection(languageSelection);
  lastRestartNoticeLanguage =
      localization::LocalizationManager::Get().ActiveLanguage();
  languageGrid->Add(interfaceLanguageChoice, 1, wxEXPAND);
  languageSizer->Add(languageGrid, 0, wxALL | wxEXPAND, 10);
  wxStaticText *languageHint = new wxStaticText(
      languagePanel, wxID_ANY,
      _("Language changes will be applied after restarting Perastage."));
  languageHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  languageHint->Wrap(740);
  languageSizer->Add(languageHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  languagePanel->SetSizer(languageSizer);
  book->AddPage(languagePanel, _("Language"));

  // Updates page
  wxPanel *updatesPanel = new wxPanel(book);
  wxBoxSizer *updatesSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *updatesGrid = new wxFlexGridSizer(1, 2, 10, 10);
  updatesGrid->AddGrowableCol(1, 1);
  updatesGrid->Add(
      new wxStaticText(updatesPanel, wxID_ANY, _("Automatic update checks:")),
                   0, wxALIGN_CENTER_VERTICAL);
  updateCheckModeChoice = new wxChoice(updatesPanel, wxID_ANY);
  updateCheckModeChoice->Append(_("Check on startup (recommended)"));
  updateCheckModeChoice->Append(_("Manual only"));
  const auto startupMode = gui::update::ReadStartupCheckMode(
      GetDefaultGuiConfigServices().Preferences());
  if (startupMode == gui::update::StartupCheckMode::ManualOnly)
    updateCheckModeChoice->SetSelection(1);
  else
    updateCheckModeChoice->SetSelection(0);
  updatesGrid->Add(updateCheckModeChoice, 1, wxEXPAND);
  updatesSizer->Add(updatesGrid, 0, wxALL | wxEXPAND, 10);
  updatesSizer->Add(new wxStaticText(updatesPanel, wxID_ANY,
                                     _("Manual checks are always available "
                                       "from Help -> Check for Updates.")),
                    0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  updatesPanel->SetSizer(updatesSizer);
  book->AddPage(updatesPanel, _("Updates"));

  // GDTF page
  gdtfCredentialsPanel = new GdtfCredentialsPanel(book);
  gdtfCredentialsPanel->LoadCredentials();
  book->AddPage(gdtfCredentialsPanel, _("GDTF"));

  // MVR Import / Export page
  wxPanel *mvrPanel = new wxPanel(book);
  wxBoxSizer *mvrSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer *mvrExportSizer =
      new wxStaticBoxSizer(wxVERTICAL, mvrPanel, _("Export"));
  wxFlexGridSizer *mvrExportGrid = new wxFlexGridSizer(1, 2, 10, 10);
  mvrExportGrid->AddGrowableCol(1, 1);
  mvrExportGrid->Add(new wxStaticText(mvrExportSizer->GetStaticBox(), wxID_ANY,
                                      _("Truss geometry export mode:")),
                     0, wxALIGN_CENTER_VERTICAL);
  mvrTrussGeometryExportModeChoice =
      new wxChoice(mvrExportSizer->GetStaticBox(), wxID_ANY);
  mvrTrussGeometryExportModeChoice->Append(_("Standard MVR representation"));
  mvrTrussGeometryExportModeChoice->Append(
      _("Direct Geometry3D for truss symbols"));
  const MvrExportOptions mvrExportOptions =
      mvr::preferences::LoadExportOptions(cfg);
  mvrTrussGeometryExportModeChoice->SetSelection(
      mvrExportOptions.trussGeometryExportMode ==
              MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols
          ? 1
          : 0);
  mvrExportGrid->Add(mvrTrussGeometryExportModeChoice, 1, wxEXPAND);
  mvrExportSizer->Add(mvrExportGrid, 0, wxALL | wxEXPAND, 8);
  wxStaticText *mvrExportHint =
      new wxStaticText(mvrExportSizer->GetStaticBox(), wxID_ANY,
                       _("Standard MVR representation: preserves imported "
                         "Symbol/Symdef references when possible and is "
                         "Perastage's canonical representation.\n"
                         "Direct Geometry3D for truss symbols: expands truss "
                         "Symbol/Symdef references into direct Geometry3D "
                         "entries for compatibility with applications that "
                         "do not correctly support Symbol/Symdef. Both modes "
                         "are MVR 1.6 compliant."));
  mvrExportHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  mvrExportHint->Wrap(740);
  mvrExportSizer->Add(mvrExportHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  mvrSizer->Add(mvrExportSizer, 0, wxALL | wxEXPAND, 10);
  mvrPanel->SetSizer(mvrSizer);
  book->AddPage(mvrPanel, _("MVR Import / Export"));

  // Selection & Movement page
  wxPanel *selectionPanel = new wxPanel(book);
  wxBoxSizer *selectionSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText *selectionTitle =
      new wxStaticText(selectionPanel, wxID_ANY, _("Selection & Movement"));
  wxFont selectionTitleFont = selectionTitle->GetFont();
  selectionTitleFont.MakeBold();
  selectionTitleFont.SetPointSize(selectionTitleFont.GetPointSize() + 2);
  selectionTitle->SetFont(selectionTitleFont);
  selectionSizer->Add(selectionTitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                      12);
  wxStaticText *selectionSubtitle = new wxStaticText(
      selectionPanel, wxID_ANY,
      _("Choose how interactive transforms handle objects inside groups."));
  selectionSubtitle->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  selectionSizer->Add(selectionSubtitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                      6);

  wxStaticBoxSizer *groupMoveSizer = new wxStaticBoxSizer(
      wxVERTICAL, selectionPanel, _("Grouped object movement"));
  groupMoveFixtureCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Fixtures move the containing group"));
  groupMoveTrussCheck = new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                                       _("Trusses move the containing group"));
  groupMoveSupportCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Supports / Hoists move the containing group"));
  groupMoveSceneObjectCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Scene Objects move the containing group"));
  const auto groupMovePolicy =
      selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
  groupMoveFixtureCheck->SetValue(groupMovePolicy.promoteFixturesToGroup);
  groupMoveTrussCheck->SetValue(groupMovePolicy.promoteTrussesToGroup);
  groupMoveSupportCheck->SetValue(groupMovePolicy.promoteSupportsToGroup);
  groupMoveSceneObjectCheck->SetValue(
      groupMovePolicy.promoteSceneObjectsToGroup);
  for (wxCheckBox *check : {groupMoveFixtureCheck, groupMoveTrussCheck,
                            groupMoveSupportCheck, groupMoveSceneObjectCheck})
    groupMoveSizer->Add(check, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  wxStaticText *groupMoveHint = new wxStaticText(
      groupMoveSizer->GetStaticBox(), wxID_ANY,
      _("When enabled, selecting this type inside a group moves the highest "
        "parent GroupObject during mouse, CLI, and Magnet transformations.\n"
        "Table edits always modify only the edited object.\n"
        "Selecting a GroupObject directly always moves that group."));
  groupMoveHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  groupMoveHint->Wrap(740);
  groupMoveSizer->Add(groupMoveHint, 0, wxALL, 8);
  selectionSizer->Add(groupMoveSizer, 0, wxALL | wxEXPAND, 10);
  wxStaticBoxSizer *magnetSizer = new wxStaticBoxSizer(
      wxVERTICAL, selectionPanel, _("Magnet visual feedback"));
  magnetAnchorReferencesCheck = new wxCheckBox(
      magnetSizer->GetStaticBox(), wxID_ANY,
      _("Show anchor references while moving or inserting elements"));
  const auto magnetReferenceValue =
      cfg.GetValue(magnet_snap::kShowAnchorReferencesConfigKey);
  magnetAnchorReferencesCheck->SetValue(!magnetReferenceValue ||
                                        *magnetReferenceValue != "0");
  magnetSizer->Add(magnetAnchorReferencesCheck, 0, wxALL, 8);
  selectionSizer->Add(magnetSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                      10);
  selectionPanel->SetSizer(selectionSizer);
  book->AddPage(selectionPanel, _("Selection & Movement"));

  // 3D Viewer page
  wxPanel *viewer3dPanel = new wxPanel(book);
  viewer3dPanel->SetBackgroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
  wxBoxSizer *viewer3dSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText *viewer3dTitle =
      new wxStaticText(viewer3dPanel, wxID_ANY, _("3D Viewer"));
  wxFont viewer3dTitleFont = viewer3dTitle->GetFont();
  viewer3dTitleFont.MakeBold();
  viewer3dTitleFont.SetPointSize(viewer3dTitleFont.GetPointSize() + 2);
  viewer3dTitle->SetFont(viewer3dTitleFont);
  viewer3dSizer->Add(viewer3dTitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

  wxStaticText *viewer3dSubtitle =
      new wxStaticText(viewer3dPanel, wxID_ANY,
                       _("Customize camera interaction and visualize the "
                         "current navigation shortcuts."));
  viewer3dSubtitle->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dSubtitle->Wrap(760);
  viewer3dSizer->Add(viewer3dSubtitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                     6);
  viewer3dSizer->Add(new wxStaticLine(viewer3dPanel, wxID_ANY), 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *viewer3dNavigationSizer =
      new wxStaticBoxSizer(wxVERTICAL, viewer3dPanel, _("Navigation"));
  viewer3dInvertOrbitHorizontalCheck =
      new wxCheckBox(viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
                     _("Invert orbit horizontal direction"));
  const auto viewer3dInvertOrbitHorizontalValue =
      cfg.GetValue(std::string(
          user_navigation_preferences::kHorizontalOrbitInversionConfigKey));
  viewer3dInvertOrbitHorizontalCheck->SetValue(
      viewer3dInvertOrbitHorizontalValue &&
      *viewer3dInvertOrbitHorizontalValue == "1");
  viewer3dNavigationSizer->Add(viewer3dInvertOrbitHorizontalCheck, 0,
                               wxLEFT | wxRIGHT | wxTOP, 8);

  viewer3dInvertOrbitVerticalCheck =
      new wxCheckBox(viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
                     _("Invert orbit vertical direction"));
  const auto viewer3dInvertOrbitValue = cfg.GetValue(
      std::string(user_navigation_preferences::kVerticalOrbitInversionConfigKey));
  viewer3dInvertOrbitVerticalCheck->SetValue(viewer3dInvertOrbitValue &&
                                             *viewer3dInvertOrbitValue == "1");
  viewer3dNavigationSizer->Add(viewer3dInvertOrbitVerticalCheck, 0, wxALL, 8);

  wxStaticText *viewer3dNavigationHint = new wxStaticText(
      viewer3dNavigationSizer->GetStaticBox(), wxID_ANY,
      _("Disabled by default to preserve the current behavior."));
  viewer3dNavigationHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dNavigationHint->Wrap(740);
  viewer3dNavigationSizer->Add(viewer3dNavigationHint, 0,
                               wxLEFT | wxRIGHT | wxBOTTOM, 8);
  viewer3dSizer->Add(viewer3dNavigationSizer, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *viewer3dRenderSizer =
      new wxStaticBoxSizer(wxVERTICAL, viewer3dPanel, _("Render mode"));
  viewer3dStandardRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Standard"),
      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  viewer3dWhiteRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("White"));
  viewer3dWhiteModelRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Sketch mode"));
  viewer3dTexturedRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Textured"));
  viewer3dWireframeRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("Wireframe"));
  viewer3dByDeviceTypeRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By device type"));
  viewer3dByLayerRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By layer"));
  viewer3dByUniverseRenderRadio = new wxRadioButton(
      viewer3dRenderSizer->GetStaticBox(), wxID_ANY, _("By universe"));

  const Viewer3DRenderStyle renderStyle = ResolveViewer3DRenderStyle(cfg);
  viewer3dStandardRenderRadio->SetValue(renderStyle ==
                                        Viewer3DRenderStyle::Standard);
  viewer3dWhiteRenderRadio->SetValue(renderStyle == Viewer3DRenderStyle::White);
  viewer3dWhiteModelRenderRadio->SetValue(renderStyle ==
                                          Viewer3DRenderStyle::WhiteModel);
  viewer3dTexturedRenderRadio->SetValue(renderStyle ==
                                        Viewer3DRenderStyle::Textured);
  viewer3dWireframeRenderRadio->SetValue(renderStyle ==
                                         Viewer3DRenderStyle::Wireframe);
  viewer3dByDeviceTypeRenderRadio->SetValue(renderStyle ==
                                            Viewer3DRenderStyle::ByDeviceType);
  viewer3dByLayerRenderRadio->SetValue(renderStyle ==
                                       Viewer3DRenderStyle::ByLayer);
  viewer3dByUniverseRenderRadio->SetValue(renderStyle ==
                                          Viewer3DRenderStyle::ByUniverse);

  viewer3dRenderSizer->Add(viewer3dStandardRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 8);
  viewer3dRenderSizer->Add(viewer3dWhiteModelRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dTexturedRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dWireframeRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dWhiteRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByDeviceTypeRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByLayerRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP, 6);
  viewer3dRenderSizer->Add(viewer3dByUniverseRenderRadio, 0,
                           wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
  viewer3dSizer->Add(viewer3dRenderSizer, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

  wxStaticBoxSizer *viewer3dShortcutsSizer = new wxStaticBoxSizer(
      wxVERTICAL, viewer3dPanel, _("Current mouse and keyboard shortcuts"));
  wxStaticText *viewer3dShortcutsInfo = new wxStaticText(
      viewer3dShortcutsSizer->GetStaticBox(), wxID_ANY,
      _("Orbit: Left drag or Right drag.\n"
        "Pan: Middle drag, Shift + drag, or Shift + Left drag.\n"
        "Zoom: Mouse wheel.\n"
        "Selection rectangle: Ctrl + Left drag."));
  viewer3dShortcutsInfo->Wrap(740);
  viewer3dShortcutsSizer->Add(viewer3dShortcutsInfo, 0, wxALL, 8);

  wxStaticText *viewer3dShortcutsHint = new wxStaticText(
      viewer3dShortcutsSizer->GetStaticBox(), wxID_ANY,
      _("Informational only: shortcut remapping is not available yet."));
  viewer3dShortcutsHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  viewer3dShortcutsHint->Wrap(740);
  viewer3dShortcutsSizer->Add(viewer3dShortcutsHint, 0,
                              wxLEFT | wxRIGHT | wxBOTTOM, 8);
  viewer3dSizer->Add(viewer3dShortcutsSizer, 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 10);
  viewer3dPanel->SetSizer(viewer3dSizer);
  book->AddPage(viewer3dPanel, _("3D Viewer"));

  topSizer->Add(book, 1, wxEXPAND | wxALL, 5);
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL | wxAPPLY), 0,
                wxALL | wxEXPAND, 5);

  SetSizerAndFit(topSizer);

  Bind(wxEVT_BUTTON, &PreferencesDialog::OnApplyButton, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &PreferencesDialog::OnOkButton, this, wxID_OK);
}

// Applies the dialog changes without closing when requested.
void PreferencesDialog::OnApplyButton(wxCommandEvent &WXUNUSED(event)) {
  if (!ApplyPreferences())
    return;
  NotifyUnitsChanged();
  NotifyPreferencesApplied();
}

// Applies the dialog changes and closes the dialog on success.
void PreferencesDialog::OnOkButton(wxCommandEvent &WXUNUSED(event)) {
  if (!ApplyPreferences())
    return;
  NotifyUnitsChanged();
  NotifyPreferencesApplied();
  EndModal(wxID_OK);
}

// Writes all preference fields to persistent user configuration storage.
bool PreferencesDialog::ApplyPreferences() {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const ConfigManager::DirtyState originalDirtyState = cfg.CaptureDirtyState();
  const auto distanceUnitSystem =
      DistanceUnitSystemFromChoice(distanceUnitChoice);
  for (int i = 0; i < 6; ++i) {
    const auto heightMm = Units::ParseDistanceToMillimeters(
        std::string(lxHeightCtrls[i]->GetValue().ToUTF8()), distanceUnitSystem);
    const double v = heightMm.has_value() ? (*heightMm / 1000.0) : 0.0;
    cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_height",
                 static_cast<float>(v));
    const auto posMm = Units::ParseDistanceToMillimeters(
        std::string(lxPosCtrls[i]->GetValue().ToUTF8()), distanceUnitSystem);
    const double p = posMm.has_value() ? (*posMm / 1000.0) : 0.0;
    cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_pos",
                 static_cast<float>(p));
    const auto marginMm = Units::ParseDistanceToMillimeters(
        std::string(lxMarginCtrls[i]->GetValue().ToUTF8()), distanceUnitSystem);
    const double m = marginMm.has_value() ? (*marginMm / 1000.0) : 0.0;
    cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_margin",
                 static_cast<float>(m));
  }

  cfg.SetValue("rider_autopatch", autopatchCheck->GetValue() ? "1" : "0");
  cfg.SetValue("rider_layer_mode",
               layerTypeRadio->GetValue() ? "type" : "position");
  cfg.SetValue("ui_distance_unit_system",
               distanceUnitChoice->GetSelection() == 1 ? "imperial" : "metric");
  cfg.SetValue("ui_weight_unit_system",
               weightUnitChoice->GetSelection() == 1 ? "imperial" : "metric");
  localization::AppLanguage selectedLanguage =
      localization::DefaultAppLanguage();
  const auto &languageOptions = localization::SupportedAppLanguages();
  if (interfaceLanguageChoice) {
    const int selection = interfaceLanguageChoice->GetSelection();
    if (selection >= 0 &&
        static_cast<std::size_t>(selection) < languageOptions.size())
      selectedLanguage =
          languageOptions[static_cast<std::size_t>(selection)].language;
  }
  cfg.SetValue(localization::kUiLanguageConfigKey,
               std::string(localization::AppLanguageCode(selectedLanguage)));
  auto &preferences = GetDefaultGuiConfigServices().Preferences();
  if (updateCheckModeChoice && updateCheckModeChoice->GetSelection() == 1)
    gui::update::WriteStartupCheckMode(
        preferences, gui::update::StartupCheckMode::ManualOnly);
  else
    gui::update::WriteStartupCheckMode(
        preferences, gui::update::StartupCheckMode::StartupRecommended);
  Viewer3DRenderStyle renderStyle = Viewer3DRenderStyle::Standard;
  if (viewer3dWhiteRenderRadio && viewer3dWhiteRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::White;
  else if (viewer3dWhiteModelRenderRadio &&
           viewer3dWhiteModelRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::WhiteModel;
  else if (viewer3dTexturedRenderRadio &&
           viewer3dTexturedRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::Textured;
  else if (viewer3dWireframeRenderRadio &&
           viewer3dWireframeRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::Wireframe;
  else if (viewer3dByDeviceTypeRenderRadio &&
           viewer3dByDeviceTypeRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByDeviceType;
  else if (viewer3dByLayerRenderRadio && viewer3dByLayerRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByLayer;
  else if (viewer3dByUniverseRenderRadio &&
           viewer3dByUniverseRenderRadio->GetValue())
    renderStyle = Viewer3DRenderStyle::ByUniverse;
  cfg.SetValue("viewer3d_render_style", ToConfigValue(renderStyle));
  cfg.SetValue(std::string(user_navigation_preferences::kVerticalOrbitInversionConfigKey),
               viewer3dInvertOrbitVerticalCheck &&
                       viewer3dInvertOrbitVerticalCheck->GetValue()
                   ? "1"
                   : "0");
  cfg.SetValue(std::string(
                   user_navigation_preferences::kHorizontalOrbitInversionConfigKey),
               viewer3dInvertOrbitHorizontalCheck &&
                       viewer3dInvertOrbitHorizontalCheck->GetValue()
                   ? "1"
                   : "0");
  selection_movement_settings::SaveInteractiveTransformPolicy(
      cfg,
      {.promoteFixturesToGroup = groupMoveFixtureCheck->GetValue(),
       .promoteTrussesToGroup = groupMoveTrussCheck->GetValue(),
       .promoteSupportsToGroup = groupMoveSupportCheck->GetValue(),
       .promoteSceneObjectsToGroup = groupMoveSceneObjectCheck->GetValue()});
  cfg.SetValue(magnet_snap::kShowAnchorReferencesConfigKey,
               magnetAnchorReferencesCheck &&
                       magnetAnchorReferencesCheck->GetValue()
                   ? "1"
                   : "0");

  MvrExportOptions mvrExportOptions;
  mvrExportOptions.trussGeometryExportMode =
      mvrTrussGeometryExportModeChoice &&
              mvrTrussGeometryExportModeChoice->GetSelection() == 1
          ? MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols
          : MvrTrussGeometryExportMode::Standard;
  mvr::preferences::SaveExportOptions(cfg, mvrExportOptions);

  if (gdtfCredentialsPanel)
    gdtfCredentialsPanel->ApplyCredentials();

  const bool saved = cfg.SaveUserConfig();
  cfg.RestoreDirtyState(originalDirtyState);
  if (saved)
    ShowLanguageRestartNoticeIfNeeded(selectedLanguage);
  return saved;
}

// Refreshes Rider Import labels to show the selected distance unit suffix.
void PreferencesDialog::RefreshRiderImportDistanceLabels() {
  const auto unitSystem = DistanceUnitSystemFromChoice(distanceUnitChoice);
  const wxString unitSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(unitSystem));
  for (int i = 0; i < 6; ++i) {
    if (lxHeightLabels[i])
      lxHeightLabels[i]->SetLabel(
          wxString::Format(_("LX%d height (%s):"), i + 1, unitSuffix));
    if (lxPosLabels[i])
      lxPosLabels[i]->SetLabel(
          wxString::Format(_("LX%d position (%s):"), i + 1, unitSuffix));
    if (lxMarginLabels[i])
      lxMarginLabels[i]->SetLabel(
          wxString::Format(_("LX%d margin (%s):"), i + 1, unitSuffix));
  }
}

// Converts Rider Import distance fields when the displayed unit system changes.
void PreferencesDialog::ConvertRiderImportDistanceFields() {
  const auto targetUnit = DistanceUnitSystemFromChoice(distanceUnitChoice);
  const auto sourceUnit = targetUnit == Units::DistanceUnitSystem::Imperial
                              ? Units::DistanceUnitSystem::Metric
                              : Units::DistanceUnitSystem::Imperial;

  auto convertCtrl = [&](wxTextCtrl *ctrl) {
    if (!ctrl)
      return;
    const auto parsed = Units::ParseDistanceToMillimeters(
        std::string(ctrl->GetValue().ToUTF8()), sourceUnit);
    if (!parsed.has_value())
      return;
    ctrl->SetValue(wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        *parsed, targetUnit, Units::ValueFormatContext::Label)));
  };

  for (int i = 0; i < 6; ++i) {
    convertCtrl(lxHeightCtrls[i]);
    convertCtrl(lxPosCtrls[i]);
    convertCtrl(lxMarginCtrls[i]);
  }
}

// Notifies the main window when persisted unit selections have changed.
void PreferencesDialog::NotifyUnitsChanged() {
  const int currentDistanceUnitSelection = distanceUnitChoice->GetSelection();
  const int currentWeightUnitSelection = weightUnitChoice->GetSelection();
  if (currentDistanceUnitSelection == initialDistanceUnitSelection &&
      currentWeightUnitSelection == initialWeightUnitSelection) {
    return;
  }

  initialDistanceUnitSelection = currentDistanceUnitSelection;
  initialWeightUnitSelection = currentWeightUnitSelection;
  wxCommandEvent event(EVT_UI_UNITS_CHANGED);
  wxPostEvent(GetParent(), event);
}

// Notifies the main window that preferences were applied successfully.
void PreferencesDialog::NotifyPreferencesApplied() {
  wxCommandEvent event(EVT_UI_PREFERENCES_APPLIED);
  wxPostEvent(GetParent(), event);
}

// Shows the restart-required language notification once for each selected
// change.
void PreferencesDialog::ShowLanguageRestartNoticeIfNeeded(
    localization::AppLanguage selectedLanguage) {
  const localization::AppLanguage activeLanguage =
      localization::LocalizationManager::Get().ActiveLanguage();
  if (selectedLanguage == activeLanguage ||
      selectedLanguage == lastRestartNoticeLanguage)
    return;
  lastRestartNoticeLanguage = selectedLanguage;
  wxMessageBox(
      _("Language changes will be applied after restarting Perastage."),
               _("Restart required"), wxOK | wxICON_INFORMATION, this);
}
