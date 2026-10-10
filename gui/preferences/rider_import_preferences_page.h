#pragma once

#include "preferences/preferences_page.h"
#include <array>
#include "units/units.h"

class wxCheckBox;
class wxRadioButton;
class wxTextCtrl;

class RiderImportPreferencesPage : public PreferencesPage {
public:
  explicit RiderImportPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;
  void SetDistanceUnit(Units::DistanceUnitSystem unit);

private:
  void RefreshDistanceLabels();
  std::array<wxTextCtrl *, 6> lxHeightCtrls{};
  std::array<wxTextCtrl *, 6> lxPosCtrls{};
  std::array<wxTextCtrl *, 6> lxMarginCtrls{};
  std::array<wxStaticText *, 6> lxHeightLabels{};
  std::array<wxStaticText *, 6> lxPosLabels{};
  std::array<wxStaticText *, 6> lxMarginLabels{};
  wxCheckBox *autopatchCheck = nullptr;
  wxRadioButton *layerPosRadio = nullptr;
  wxRadioButton *layerTypeRadio = nullptr;
  Units::DistanceUnitSystem displayedDistanceUnit = Units::DistanceUnitSystem::Metric;
};
