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

GdtfPreferencesPage::GdtfPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  auto *gdtfSizer = new wxBoxSizer(wxVERTICAL);
  gdtfCredentialsPanel = new GdtfCredentialsPanel(this);
  gdtfSizer->Add(gdtfCredentialsPanel, 0, wxEXPAND);
  SetSizer(gdtfSizer);

  PrepareLayout();
}

void GdtfPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  (void)cfg;
  gdtfCredentialsPanel->LoadCredentials();
}

void GdtfPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  (void)cfg;
  gdtfCredentialsPanel->ApplyCredentials();
}
