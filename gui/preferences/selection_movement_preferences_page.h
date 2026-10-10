#pragma once

#include "preferences/preferences_page.h"

class wxCheckBox;

class SelectionMovementPreferencesPage : public PreferencesPage {
public:
  explicit SelectionMovementPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  wxCheckBox *groupMoveFixtureCheck = nullptr;
  wxCheckBox *groupMoveTrussCheck = nullptr;
  wxCheckBox *groupMoveSupportCheck = nullptr;
  wxCheckBox *groupMoveSceneObjectCheck = nullptr;
  wxCheckBox *magnetAnchorReferencesCheck = nullptr;
};
