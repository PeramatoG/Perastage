// Keep the UI regression isolated from the operating system credential vault
// while exercising production locale discovery and catalog loading.
#include "preferences/gdtf_credentials_panel.h"

int preferencesTestCredentialLoads = 0;
int preferencesTestCredentialApplies = 0;

GdtfCredentialsPanel::GdtfCredentialsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {}
void GdtfCredentialsPanel::LoadCredentials() { ++preferencesTestCredentialLoads; }
bool GdtfCredentialsPanel::ApplyCredentials() {
  ++preferencesTestCredentialApplies;
  return true;
}
