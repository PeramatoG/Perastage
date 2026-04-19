#include "mainwindow_gdtf_credentials.h"

#include "configmanager.h"
#include "simplecrypt.h"

namespace {

void ClearLegacyGdtfCredentials(ConfigManager &configManager) {
  configManager.SetValue("gdtf_username", "");
  configManager.SetValue("gdtf_password", "");
}

} // namespace

std::optional<CredentialStore::Credentials>
LoadGdtfCredentialsForGui(ConfigManager &configManager) {
  if (auto credentials = CredentialStore::Load()) {
    return credentials;
  }

  const std::string legacyUsername =
      configManager.GetValue("gdtf_username").value_or("");
  const std::string legacyPasswordEncoded =
      configManager.GetValue("gdtf_password").value_or("");
  if (legacyUsername.empty()) {
    return std::nullopt;
  }

  CredentialStore::Credentials migratedCredentials;
  migratedCredentials.username = legacyUsername;
  migratedCredentials.password = SimpleCrypt::Decode(legacyPasswordEncoded);

  if (CredentialStore::Save(migratedCredentials)) {
    ClearLegacyGdtfCredentials(configManager);
  }
  return migratedCredentials;
}

void PersistGdtfCredentialsForGui(const CredentialStore::Credentials &credentials,
                                  ConfigManager &configManager) {
  if (credentials.username.empty()) {
    CredentialStore::Clear();
    ClearLegacyGdtfCredentials(configManager);
    return;
  }

  CredentialStore::Save(credentials);
  ClearLegacyGdtfCredentials(configManager);
}

bool IsAuthenticationFailureHttpCode(const long httpCode) {
  return httpCode == 401 || httpCode == 403;
}
