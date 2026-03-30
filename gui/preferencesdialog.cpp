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
#include "configmanager.h"
#include "guiconfigservices.h"
#include "units/units.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/radiobut.h>

wxDEFINE_EVENT(EVT_UI_UNITS_CHANGED, wxCommandEvent);

namespace {

Units::DistanceUnitSystem DistanceUnitSystemFromChoice(const wxChoice *choice) {
  if (choice && choice->GetSelection() == 1)
    return Units::DistanceUnitSystem::Imperial;
  return Units::DistanceUnitSystem::Metric;
}

} // namespace

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition,
               wxDefaultSize) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxNotebook *book = new wxNotebook(this, wxID_ANY);

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();

  // Rider Import page
  wxPanel *riderPanel = new wxPanel(book);
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  autopatchCheck = new wxCheckBox(riderPanel, wxID_ANY,
                                  "Auto patch after import");
  auto autoVal = cfg.GetValue("rider_autopatch");
  autopatchCheck->SetValue(!autoVal || *autoVal != "0");
  riderSizer->Add(autopatchCheck, 0, wxALL, 10);
  layerPosRadio =
      new wxRadioButton(riderPanel, wxID_ANY,
                        "Auto-create layers by position", wxDefaultPosition,
                        wxDefaultSize, wxRB_GROUP);
  layerTypeRadio = new wxRadioButton(riderPanel, wxID_ANY,
                                     "Auto-create layers by fixture type");
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
    lxHeightLabels[i] =
        new wxStaticText(riderPanel, wxID_ANY, wxString::Format("LX%d height:", i + 1));
    grid->Add(lxHeightLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valH = static_cast<double>(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_height"));
    lxHeightCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                          valH * 1000.0, riderDistanceUnit,
                                          Units::ValueFormatContext::Label)));
    grid->Add(lxHeightCtrls[i], 1, wxEXPAND);

    lxPosLabels[i] =
        new wxStaticText(riderPanel, wxID_ANY, wxString::Format("LX%d position:", i + 1));
    grid->Add(lxPosLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valP =
        static_cast<double>(cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_pos"));
    lxPosCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                   wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                       valP * 1000.0, riderDistanceUnit,
                                       Units::ValueFormatContext::Label)));
    grid->Add(lxPosCtrls[i], 1, wxEXPAND);

    lxMarginLabels[i] =
        new wxStaticText(riderPanel, wxID_ANY, wxString::Format("LX%d margin:", i + 1));
    grid->Add(lxMarginLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    const double valM = static_cast<double>(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_margin"));
    lxMarginCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
                                          valM * 1000.0, riderDistanceUnit,
                                          Units::ValueFormatContext::Label)));
    grid->Add(lxMarginCtrls[i], 1, wxEXPAND);
  }
  riderSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
  riderPanel->SetSizer(riderSizer);
  book->AddPage(riderPanel, "Rider Import");

  // Units page
  wxPanel *unitsPanel = new wxPanel(book);
  wxBoxSizer *unitsSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *unitsGrid = new wxFlexGridSizer(2, 2, 10, 10);
  unitsGrid->AddGrowableCol(1, 1);

  unitsGrid->Add(new wxStaticText(unitsPanel, wxID_ANY, "Distance system:"), 0,
                 wxALIGN_CENTER_VERTICAL);
  distanceUnitChoice = new wxChoice(unitsPanel, wxID_ANY);
  distanceUnitChoice->Append("Metric");
  distanceUnitChoice->Append("Imperial");
  auto distanceUnitValue = cfg.GetValue("ui_distance_unit_system");
  const bool hasImperialDistance =
      distanceUnitValue && *distanceUnitValue == "imperial";
  distanceUnitChoice->SetSelection(hasImperialDistance ? 1 : 0);
  RefreshRiderImportDistanceLabels();
  distanceUnitChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
    ConvertRiderImportDistanceFields();
    RefreshRiderImportDistanceLabels();
  });
  initialDistanceUnit = distanceUnitChoice->GetStringSelection();
  unitsGrid->Add(distanceUnitChoice, 1, wxEXPAND);

  unitsGrid->Add(new wxStaticText(unitsPanel, wxID_ANY, "Weight system:"), 0,
                 wxALIGN_CENTER_VERTICAL);
  weightUnitChoice = new wxChoice(unitsPanel, wxID_ANY);
  weightUnitChoice->Append("Metric");
  weightUnitChoice->Append("Imperial");
  auto weightUnitValue = cfg.GetValue("ui_weight_unit_system");
  const bool hasImperialWeight = weightUnitValue && *weightUnitValue == "imperial";
  weightUnitChoice->SetSelection(hasImperialWeight ? 1 : 0);
  initialWeightUnit = weightUnitChoice->GetStringSelection();
  unitsGrid->Add(weightUnitChoice, 1, wxEXPAND);

  unitsSizer->Add(unitsGrid, 0, wxALL | wxEXPAND, 10);
  unitsPanel->SetSizer(unitsSizer);
  book->AddPage(unitsPanel, "Units");

  // 3D Viewer page
  wxPanel *viewer3dPanel = new wxPanel(book);
  wxBoxSizer *viewer3dSizer = new wxBoxSizer(wxVERTICAL);
  viewer3dSizer->Add(new wxStaticText(viewer3dPanel, wxID_ANY, "Render mode:"),
                     0, wxLEFT | wxRIGHT | wxTOP, 10);
  viewer3dStandardRenderRadio = new wxRadioButton(
      viewer3dPanel, wxID_ANY, "Standard (current)", wxDefaultPosition,
      wxDefaultSize, wxRB_GROUP);
  viewer3dWhiteModelRenderRadio = new wxRadioButton(
      viewer3dPanel, wxID_ANY, "White Model style");

  auto viewer3dRenderStyle = cfg.GetValue("viewer3d_render_style");
  const bool useWhiteModelStyle =
      viewer3dRenderStyle && *viewer3dRenderStyle == "white_model";
  viewer3dStandardRenderRadio->SetValue(!useWhiteModelStyle);
  viewer3dWhiteModelRenderRadio->SetValue(useWhiteModelStyle);

  viewer3dSizer->Add(viewer3dStandardRenderRadio, 0,
                     wxLEFT | wxRIGHT | wxTOP, 10);
  viewer3dSizer->Add(viewer3dWhiteModelRenderRadio, 0,
                     wxLEFT | wxRIGHT | wxBOTTOM, 10);
  viewer3dPanel->SetSizer(viewer3dSizer);
  book->AddPage(viewer3dPanel, "3D Viewer");

  topSizer->Add(book, 1, wxEXPAND | wxALL, 5);
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL | wxAPPLY), 0,
                wxALL | wxEXPAND, 5);

  SetSizerAndFit(topSizer);

  Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
    if (evt.GetId() == wxID_OK || evt.GetId() == wxID_APPLY) {
      if (ApplyPreferences()) {
        NotifyUnitsChanged();
      }
    }
    evt.Skip();
  });
}

bool PreferencesDialog::ApplyPreferences() {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto distanceUnitSystem = DistanceUnitSystemFromChoice(distanceUnitChoice);
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
  cfg.SetValue("rider_layer_mode", layerTypeRadio->GetValue() ? "type"
                                                               : "position");
  cfg.SetValue("ui_distance_unit_system",
               distanceUnitChoice->GetSelection() == 1 ? "imperial"
                                                       : "metric");
  cfg.SetValue("ui_weight_unit_system",
               weightUnitChoice->GetSelection() == 1 ? "imperial" : "metric");
  cfg.SetValue("viewer3d_render_style",
               viewer3dWhiteModelRenderRadio &&
                       viewer3dWhiteModelRenderRadio->GetValue()
                   ? "white_model"
                   : "standard");
  return cfg.SaveUserConfig();
}

void PreferencesDialog::RefreshRiderImportDistanceLabels() {
  const auto unitSystem = DistanceUnitSystemFromChoice(distanceUnitChoice);
  const wxString unitSuffix = wxString::FromUTF8(Units::DistanceUnitSuffix(unitSystem));
  for (int i = 0; i < 6; ++i) {
    if (lxHeightLabels[i])
      lxHeightLabels[i]->SetLabel(wxString::Format("LX%d height (%s):", i + 1, unitSuffix));
    if (lxPosLabels[i])
      lxPosLabels[i]->SetLabel(wxString::Format("LX%d position (%s):", i + 1, unitSuffix));
    if (lxMarginLabels[i])
      lxMarginLabels[i]->SetLabel(wxString::Format("LX%d margin (%s):", i + 1, unitSuffix));
  }
}

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

void PreferencesDialog::NotifyUnitsChanged() {
  const wxString currentDistanceUnit = distanceUnitChoice->GetStringSelection();
  const wxString currentWeightUnit = weightUnitChoice->GetStringSelection();
  if (currentDistanceUnit == initialDistanceUnit &&
      currentWeightUnit == initialWeightUnit) {
    return;
  }

  initialDistanceUnit = currentDistanceUnit;
  initialWeightUnit = currentWeightUnit;
  wxCommandEvent event(EVT_UI_UNITS_CHANGED);
  wxPostEvent(GetParent(), event);
}
