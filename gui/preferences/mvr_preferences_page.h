#pragma once

#include "preferences/preferences_page.h"

class wxChoice;
class wxCheckBox;

class MvrPreferencesPage : public PreferencesPage {
public:
  explicit MvrPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  wxCheckBox *autoCloseGdtfDownloadWindow = nullptr;
  wxChoice *mvrTrussGeometryExportModeChoice = nullptr;
};
