#pragma once

#include "preferences/preferences_page.h"

class GdtfCredentialsPanel;
class GdtfMutationPolicyPanel;

class GdtfPreferencesPage : public PreferencesPage {
public:
  explicit GdtfPreferencesPage(wxWindow *parent);
  void LoadPreferences(const IGuiPreferencesService &preferences) override;
  void ApplyPreferences(IGuiPreferencesService &preferences) override;

private:
  GdtfMutationPolicyPanel *gdtfMutationPolicyPanel = nullptr;
  GdtfCredentialsPanel *gdtfCredentialsPanel = nullptr;
};
