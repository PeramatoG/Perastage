// Keep the UI regression isolated from the operating system credential vault
// and locale discovery. Production credential/locale implementations have
// separate integration tests; this test verifies their page coordination.
#include "preferences/gdtf_credentials_panel.h"
#include "localization/localization_manager.h"
#include <wx/intl.h>

int preferencesTestCredentialLoads = 0;
int preferencesTestCredentialApplies = 0;

GdtfCredentialsPanel::GdtfCredentialsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {}
void GdtfCredentialsPanel::LoadCredentials() { ++preferencesTestCredentialLoads; }
bool GdtfCredentialsPanel::ApplyCredentials() {
  ++preferencesTestCredentialApplies;
  return true;
}

namespace localization {
LocalizationManager &LocalizationManager::Get() {
  static LocalizationManager manager;
  return manager;
}
AppLanguage LocalizationManager::ActiveLanguage() const {
  return AppLanguage::English;
}
} // namespace localization
