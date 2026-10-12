#pragma once
#include "preferences/preferences_page.h"

class CachePreferencesPage : public PreferencesPage {
public:
  explicit CachePreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &) override;
  void ApplyPreferences(IGuiPreferencesService &) override;
private:
  void RefreshSize();
  void ClearCache();
  void OpenFolder();
  wxStaticText *sizeLabel = nullptr;
};
