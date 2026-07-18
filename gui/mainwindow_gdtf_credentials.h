#pragma once

#include "credentialstore.h"

#include <optional>

class ConfigManager;

CredentialStore::LoadResult LoadGdtfCredentialsForGuiDetailed(ConfigManager &configManager);
std::optional<CredentialStore::Credentials>
LoadGdtfCredentialsForGui(ConfigManager &configManager);

CredentialStore::Result PersistGdtfCredentialsForGui(const CredentialStore::Credentials &credentials,
                                  ConfigManager &configManager);

bool IsAuthenticationFailureHttpCode(long httpCode);
