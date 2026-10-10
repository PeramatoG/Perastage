#include "preferences/rider_import_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "units/units.h"

RiderImportPreferencesPage::RiderImportPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  autopatchCheck =
      new wxCheckBox(this, wxID_ANY, _("Auto patch after import"));
  riderSizer->Add(autopatchCheck, 0, wxALL, 10);
  layerPosRadio = new wxRadioButton(
      this, wxID_ANY, _("Auto-create layers by position"),
      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  layerTypeRadio = new wxRadioButton(this, wxID_ANY,
                                     _("Auto-create layers by fixture type"));
  riderSizer->Add(layerPosRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  riderSizer->Add(layerTypeRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 5, 10);
  grid->AddGrowableCol(1, 1);

  for (int i = 0; i < 6; ++i) {
    lxHeightLabels[i] = new wxStaticText(
        this, wxID_ANY, wxString::Format(_("LX%d height:"), i + 1));
    grid->Add(lxHeightLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    lxHeightCtrls[i] = new wxTextCtrl(this, wxID_ANY);
    grid->Add(lxHeightCtrls[i], 1, wxEXPAND);

    lxPosLabels[i] = new wxStaticText(
        this, wxID_ANY, wxString::Format(_("LX%d position:"), i + 1));
    grid->Add(lxPosLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    lxPosCtrls[i] = new wxTextCtrl(this, wxID_ANY);
    grid->Add(lxPosCtrls[i], 1, wxEXPAND);

    lxMarginLabels[i] = new wxStaticText(
        this, wxID_ANY, wxString::Format(_("LX%d margin:"), i + 1));
    grid->Add(lxMarginLabels[i], 0, wxALIGN_CENTER_VERTICAL);
    lxMarginCtrls[i] = new wxTextCtrl(this, wxID_ANY);
    grid->Add(lxMarginCtrls[i], 1, wxEXPAND);
  }
  riderSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
  SetSizer(riderSizer);

  PrepareLayout();
}

void RiderImportPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  auto autoVal = cfg.GetValue("rider_autopatch");
  autopatchCheck->SetValue(!autoVal || *autoVal != "0");
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool byType = modeVal && *modeVal == "type";
  layerTypeRadio->SetValue(byType);
  layerPosRadio->SetValue(!byType);
  const auto riderDistanceUnit =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  displayedDistanceUnit = riderDistanceUnit;
  for (int i = 0; i < 6; ++i) {
    lxHeightCtrls[i]->SetValue(wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_height") * 1000.0,
        displayedDistanceUnit, Units::ValueFormatContext::Label)));
    lxPosCtrls[i]->SetValue(wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_pos") * 1000.0,
        displayedDistanceUnit, Units::ValueFormatContext::Label)));
    lxMarginCtrls[i]->SetValue(wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
        cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_margin") * 1000.0,
        displayedDistanceUnit, Units::ValueFormatContext::Label)));
  }
  RefreshDistanceLabels();
}

void RiderImportPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  const auto distanceUnitSystem =
      displayedDistanceUnit;
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
}

void RiderImportPreferencesPage::RefreshDistanceLabels() {
  const auto unitSystem = displayedDistanceUnit;
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

void RiderImportPreferencesPage::SetDistanceUnit(Units::DistanceUnitSystem unit) {
  if (unit == displayedDistanceUnit)
    return;
  auto convert = [&](wxTextCtrl *control) {
    const auto parsed = Units::ParseDistanceToMillimeters(
        std::string(control->GetValue().ToUTF8()), displayedDistanceUnit);
    if (parsed)
      control->ChangeValue(wxString::FromUTF8(Units::FormatDistanceFromMillimeters(
          *parsed, unit, Units::ValueFormatContext::Label)));
  };
  for (int i = 0; i < 6; ++i) {
    convert(lxHeightCtrls[i]);
    convert(lxPosCtrls[i]);
    convert(lxMarginCtrls[i]);
  }
  displayedDistanceUnit = unit;
  RefreshDistanceLabels();
}
