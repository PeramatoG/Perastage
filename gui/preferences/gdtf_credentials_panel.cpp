#include "preferences/gdtf_credentials_panel.h"

#include "configmanager.h"
#include "credentialstore.h"
#include "gdtfnet.h"
#include "guiconfigservices.h"
#include "mainwindow_gdtf_credentials.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

CredentialStore::Credentials ReadUiCredentials(const wxTextCtrl *usernameCtrl,
                                               const wxTextCtrl *passwordCtrl) {
  CredentialStore::Credentials credentials;
  credentials.username =
      std::string(usernameCtrl->GetValue().Trim(true).Trim(false).ToUTF8());
  credentials.password = std::string(passwordCtrl->GetValue().ToUTF8());
  return credentials;
}

} // namespace

GdtfCredentialsPanel::GdtfCredentialsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *credentialsGrid = new wxFlexGridSizer(2, 2, 8, 10);
  credentialsGrid->AddGrowableCol(1, 1);

  credentialsGrid->Add(new wxStaticText(this, wxID_ANY, "Username:"), 0,
                       wxALIGN_CENTER_VERTICAL);
  usernameCtrl = new wxTextCtrl(this, wxID_ANY);
  credentialsGrid->Add(usernameCtrl, 1, wxEXPAND);

  credentialsGrid->Add(new wxStaticText(this, wxID_ANY, "Password:"), 0,
                       wxALIGN_CENTER_VERTICAL);
  passwordCtrl =
      new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDefaultSize, wxTE_PASSWORD);
  credentialsGrid->Add(passwordCtrl, 1, wxEXPAND);

  topSizer->Add(credentialsGrid, 0, wxALL | wxEXPAND, 10);

  validateButton = new wxButton(this, wxID_ANY, "Validate credentials");
  topSizer->Add(validateButton, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  SetSizer(topSizer);

  validateButton->Bind(wxEVT_BUTTON,
                       &GdtfCredentialsPanel::OnValidateCredentials, this);
}

void GdtfCredentialsPanel::LoadCredentials() {
  ConfigManager &configManager =
      GetDefaultGuiConfigServices().LegacyConfigManager();
  if (const auto credentials = LoadGdtfCredentialsForGui(configManager)) {
    usernameCtrl->SetValue(wxString::FromUTF8(credentials->username));
    passwordCtrl->SetValue(wxString::FromUTF8(credentials->password));
  }
}

bool GdtfCredentialsPanel::ApplyCredentials() {
  ConfigManager &configManager =
      GetDefaultGuiConfigServices().LegacyConfigManager();
  const CredentialStore::Credentials credentials =
      ReadUiCredentials(usernameCtrl, passwordCtrl);
  const CredentialStore::Result result = credentials.username.empty()
      ? CredentialStore::ClearDetailed()
      : CredentialStore::Save(credentials);
  configManager.SetValue("gdtf_username", credentials.username);
  configManager.SetValue("gdtf_password", "");
  return result.Succeeded() ||
         result.status == CredentialStore::Status::SecureStoreUnavailable;
}

void GdtfCredentialsPanel::OnValidateCredentials(wxCommandEvent &WXUNUSED(event)) {
  const CredentialStore::Credentials credentials =
      ReadUiCredentials(usernameCtrl, passwordCtrl);
  if (credentials.username.empty() || credentials.password.empty()) {
    wxMessageBox("Please enter username and password first.",
                 "Validate credentials", wxOK | wxICON_INFORMATION, this);
    return;
  }

  GdtfShareClient client;
  const GdtfShareResult loginResult =
      client.Login(credentials.username, credentials.password);
  if (!loginResult.Succeeded()) {
    wxMessageBox(wxString::FromUTF8(
                     FormatGdtfShareUserMessage(loginResult, "login")),
                 "Validate credentials", wxOK | wxICON_WARNING, this);
    return;
  }

  const CredentialStore::Result saveResult = CredentialStore::Save(credentials);
  if (!saveResult.Succeeded()) {
    wxMessageBox("The credentials are valid, but secure storage is unavailable, "
                 "so the password was not saved.",
                 "Validate credentials", wxOK | wxICON_WARNING, this);
    return;
  }
  wxMessageBox("Credentials are valid and were saved.",
               "Validate credentials", wxOK | wxICON_INFORMATION, this);
}
