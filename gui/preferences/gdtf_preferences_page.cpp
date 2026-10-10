#include "preferences/gdtf_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "preferences/gdtf_credentials_panel.h"
#include "preferences/gdtf_mutation_policy_panel.h"

GdtfPreferencesPage::GdtfPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  auto *gdtfSizer = new wxBoxSizer(wxVERTICAL);
  gdtfMutationPolicyPanel = new GdtfMutationPolicyPanel(this);
  gdtfSizer->Add(gdtfMutationPolicyPanel, 0, wxEXPAND);
  gdtfCredentialsPanel = new GdtfCredentialsPanel(this);
  gdtfSizer->Add(gdtfCredentialsPanel, 0, wxEXPAND);
  SetSizer(gdtfSizer);

  PrepareLayout();
}

void GdtfPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  gdtfMutationPolicyPanel->LoadPreferences(
      cfg);
  gdtfCredentialsPanel->LoadCredentials();
}

void GdtfPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  gdtfMutationPolicyPanel->ApplyPreferences(cfg);
  gdtfCredentialsPanel->ApplyCredentials();
}
