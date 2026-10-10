#include "preferences/units_preferences_page.h"
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

UnitsPreferencesPage::UnitsPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *unitsSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *unitsGrid = new wxFlexGridSizer(2, 2, 10, 10);
  unitsGrid->AddGrowableCol(1, 1);

  unitsGrid->Add(new wxStaticText(this, wxID_ANY, _("Distance system:")),
                 0, wxALIGN_CENTER_VERTICAL);
  distanceUnitChoice = new wxChoice(this, wxID_ANY);
  distanceUnitChoice->Append(_("Metric"));
  distanceUnitChoice->Append(_("Imperial"));
  unitsGrid->Add(distanceUnitChoice, 1, wxEXPAND);

  unitsGrid->Add(new wxStaticText(this, wxID_ANY, _("Weight system:")), 0,
                 wxALIGN_CENTER_VERTICAL);
  weightUnitChoice = new wxChoice(this, wxID_ANY);
  weightUnitChoice->Append(_("Metric"));
  weightUnitChoice->Append(_("Imperial"));
  unitsGrid->Add(weightUnitChoice, 1, wxEXPAND);

  unitsSizer->Add(unitsGrid, 0, wxALL | wxEXPAND, 10);
  SetSizer(unitsSizer);

  PrepareLayout();
}

void UnitsPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  auto distanceUnitValue = cfg.GetValue("ui_distance_unit_system");
  const bool hasImperialDistance =
      distanceUnitValue && *distanceUnitValue == "imperial";
  distanceUnitChoice->SetSelection(hasImperialDistance ? 1 : 0);
  initialDistanceUnitSelection = distanceUnitChoice->GetSelection();
  auto weightUnitValue = cfg.GetValue("ui_weight_unit_system");
  const bool hasImperialWeight =
      weightUnitValue && *weightUnitValue == "imperial";
  weightUnitChoice->SetSelection(hasImperialWeight ? 1 : 0);
  initialWeightUnitSelection = weightUnitChoice->GetSelection();
}

void UnitsPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  cfg.SetValue("ui_distance_unit_system",
               distanceUnitChoice->GetSelection() == 1 ? "imperial" : "metric");
  cfg.SetValue("ui_weight_unit_system",
               weightUnitChoice->GetSelection() == 1 ? "imperial" : "metric");
}

Units::DistanceUnitSystem UnitsPreferencesPage::DistanceUnit() const {
  return distanceUnitChoice->GetSelection() == 1
             ? Units::DistanceUnitSystem::Imperial
             : Units::DistanceUnitSystem::Metric;
}

bool UnitsPreferencesPage::UnitsChanged() const {
  return distanceUnitChoice->GetSelection() != initialDistanceUnitSelection ||
         weightUnitChoice->GetSelection() != initialWeightUnitSelection;
}

void UnitsPreferencesPage::MarkApplied() {
  initialDistanceUnitSelection = distanceUnitChoice->GetSelection();
  initialWeightUnitSelection = weightUnitChoice->GetSelection();
}

void UnitsPreferencesPage::BindDistanceChanged(
    const std::function<void(Units::DistanceUnitSystem)> &callback) {
  distanceUnitChoice->Bind(wxEVT_CHOICE, [this, callback](wxCommandEvent &) {
    callback(DistanceUnit());
  });
}
