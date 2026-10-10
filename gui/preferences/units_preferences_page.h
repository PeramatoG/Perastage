#pragma once

#include "preferences/preferences_page.h"
#include <functional>
#include "units/units.h"

class wxChoice;

class UnitsPreferencesPage : public PreferencesPage {
public:
  explicit UnitsPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;
  Units::DistanceUnitSystem DistanceUnit() const;
  bool UnitsChanged() const;
  void MarkApplied();
  void BindDistanceChanged(const std::function<void(Units::DistanceUnitSystem)> &callback);

private:
  wxChoice *distanceUnitChoice = nullptr;
  wxChoice *weightUnitChoice = nullptr;
  int initialDistanceUnitSelection = wxNOT_FOUND;
  int initialWeightUnitSelection = wxNOT_FOUND;
};
