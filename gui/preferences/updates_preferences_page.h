#pragma once

#include "preferences/preferences_page.h"

class wxChoice;

class UpdatesPreferencesPage : public PreferencesPage {
public:
  explicit UpdatesPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  wxChoice *updateCheckModeChoice = nullptr;
};
