#pragma once

#include "preferences/preferences_page.h"

class wxChoice;

class MvrPreferencesPage : public PreferencesPage {
public:
  explicit MvrPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  wxChoice *mvrTrussGeometryExportModeChoice = nullptr;
};
