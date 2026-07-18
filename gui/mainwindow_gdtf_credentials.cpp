#include "mainwindow_gdtf_credentials.h"

#include "configmanager.h"
#include "credentialstore.h"

// Clears legacy GUI credential keys after centralized credential removal.
namespace {
void ClearLegacyCredentialValues(ConfigManager &configManager) {
  configManager.SetValue("gdtf_username", "");
  configManager.SetValue("gdtf_password", "");
}
} // namespace

// Loads detailed GDTF credential state for GUI workflows through the centralized store.
CredentialStore::LoadResult LoadGdtfCredentialsForGuiDetailed(ConfigManager &configManager) {
  (void)configManager;
  return CredentialStore::LoadDetailed();
}

// Loads complete GDTF credentials for GUI workflows through the centralized store.
std::optional<CredentialStore::Credentials>
LoadGdtfCredentialsForGui(ConfigManager &configManager) {
  return LoadGdtfCredentialsForGuiDetailed(configManager).credentials;
}

// Persists or clears GDTF credentials for GUI workflows through the centralized store.
CredentialStore::Result PersistGdtfCredentialsForGui(const CredentialStore::Credentials &credentials,
                                  ConfigManager &configManager) {
  if (credentials.username.empty()) {
    CredentialStore::ClearDetailed();
    ClearLegacyCredentialValues(configManager);
    return {};
  }
  CredentialStore::Result result = CredentialStore::Save(credentials);
  configManager.SetValue("gdtf_username", credentials.username);
  configManager.SetValue("gdtf_password", "");
  return result;
}
