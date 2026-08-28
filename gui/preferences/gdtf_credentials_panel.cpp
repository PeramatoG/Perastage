#include "preferences/gdtf_credentials_panel.h"
#include "gdtf_share_message_formatter.h"

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

  credentialsGrid->Add(new wxStaticText(this, wxID_ANY, _("Username:")), 0,
                       wxALIGN_CENTER_VERTICAL);
  usernameCtrl = new wxTextCtrl(this, wxID_ANY);
  credentialsGrid->Add(usernameCtrl, 1, wxEXPAND);

  credentialsGrid->Add(new wxStaticText(this, wxID_ANY, _("Password:")), 0,
                       wxALIGN_CENTER_VERTICAL);
  passwordCtrl =
      new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDefaultSize, wxTE_PASSWORD);
  credentialsGrid->Add(passwordCtrl, 1, wxEXPAND);

  topSizer->Add(credentialsGrid, 0, wxALL | wxEXPAND, 10);

  validateButton = new wxButton(this, wxID_ANY, _("Validate credentials"));
  topSizer->Add(validateButton, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  SetSizer(topSizer);

  validateButton->Bind(wxEVT_BUTTON,
                       &GdtfCredentialsPanel::OnValidateCredentials, this);
}

void GdtfCredentialsPanel::LoadCredentials() {
  ConfigManager &configManager =
      GetDefaultGuiConfigServices().LegacyConfigManager();
  const CredentialStore::LoadResult loaded =
      LoadGdtfCredentialsForGuiDetailed(configManager);
  if (loaded.credentials) {
    usernameCtrl->SetValue(wxString::FromUTF8(loaded.credentials->username));
    passwordCtrl->SetValue(wxString::FromUTF8(loaded.credentials->password));
  } else if (loaded.usernameHint) {
    usernameCtrl->SetValue(wxString::FromUTF8(*loaded.usernameHint));
    passwordCtrl->Clear();
  }
}

bool GdtfCredentialsPanel::ApplyCredentials() {
  ConfigManager &configManager =
      GetDefaultGuiConfigServices().LegacyConfigManager();
  const CredentialStore::Credentials credentials =
      ReadUiCredentials(usernameCtrl, passwordCtrl);
  CredentialStore::Result result;
  if (credentials.username.empty()) {
    result = CredentialStore::ClearDetailed();
  } else if (credentials.password.empty()) {
    result = CredentialStore::SaveUsernameMetadataOnly(credentials.username);
    if (result.Succeeded()) {
      wxMessageBox(_("Only the username was saved. Enter a password to update the stored GDTF Share credentials."),
                   _("GDTF Share credentials"), wxOK | wxICON_WARNING, this);
    }
  } else {
    result = CredentialStore::Save(credentials);
  }
  configManager.SetValue("gdtf_username", credentials.username);
  configManager.SetValue("gdtf_password", "");
  if (result.Succeeded())
    return true;
  if (result.status == CredentialStore::Status::SecureStoreUnavailable) {
    wxMessageBox(_("The username was saved, but secure password storage is unavailable. The password must be entered again after restart."),
                 _("GDTF Share credentials"), wxOK | wxICON_WARNING, this);
    return true;
  }
  wxMessageBox(wxString::Format(_("GDTF Share credentials were not saved (%s)."),
                                wxString::FromUTF8(CredentialStore::StatusName(result.status))),
               _("GDTF Share credentials"), wxOK | wxICON_WARNING, this);
  return false;
}

void GdtfCredentialsPanel::OnValidateCredentials(wxCommandEvent &WXUNUSED(event)) {
  const CredentialStore::Credentials credentials =
      ReadUiCredentials(usernameCtrl, passwordCtrl);
  if (credentials.username.empty() || credentials.password.empty()) {
    wxMessageBox(_("Please enter username and password first."),
                 _("Validate credentials"), wxOK | wxICON_INFORMATION, this);
    return;
  }

  GdtfShareClient client;
  const GdtfShareResult loginResult =
      client.Login(credentials.username, credentials.password);
  if (!loginResult.Succeeded()) {
    wxMessageBox(FormatLocalizedGdtfShareUserMessage(
                     loginResult, GdtfShareGuiOperation::Login),
                 _("Validate credentials"), wxOK | wxICON_WARNING, this);
    return;
  }

  const CredentialStore::Result saveResult = CredentialStore::Save(credentials);
  if (!saveResult.Succeeded()) {
    wxMessageBox(saveResult.status == CredentialStore::Status::SecureStoreUnavailable
                     ? _("The credentials are valid, but secure storage is unavailable, so the password was not saved.")
                     : wxString::Format(_("The credentials are valid, but they were not saved (%s)."),
                                        wxString::FromUTF8(CredentialStore::StatusName(saveResult.status))),
                 _("Validate credentials"), wxOK | wxICON_WARNING, this);
    return;
  }
  wxMessageBox(_("Credentials are valid and were saved."),
               _("Validate credentials"), wxOK | wxICON_INFORMATION, this);
}
