#pragma once

#include "preferences/preferences_page.h"
#include "localization/app_language.h"

class wxChoice;

class LanguagePreferencesPage : public PreferencesPage {
public:
  explicit LanguagePreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;
  localization::AppLanguage SelectedLanguage() const;

private:
  wxChoice *interfaceLanguageChoice = nullptr;
};
