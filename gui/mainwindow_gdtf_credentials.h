#pragma once

#include "credentialstore.h"

#include <optional>

class ConfigManager;

std::optional<CredentialStore::Credentials>
LoadGdtfCredentialsForGui(ConfigManager &configManager);

void PersistGdtfCredentialsForGui(const CredentialStore::Credentials &credentials,
                                  ConfigManager &configManager);

bool IsAuthenticationFailureHttpCode(long httpCode);
