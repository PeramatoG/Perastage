/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once
#include <memory>
#include <optional>
#include <string>

namespace CredentialStore {
struct Credentials {
    std::string username;
    std::string password;
};

enum class Status {
    Success,
    NotFound,
    SecureStoreUnavailable,
    SecureStoreAccessFailed,
    MetadataReadFailed,
    MetadataWriteFailed,
    LegacyDataInvalid,
    MigrationFailed
};

struct Result {
    Status status = Status::Success;
    std::string message;
    bool metadataWritten = false;
    bool secretWritten = false;
    bool Succeeded() const { return status == Status::Success; }
};

struct LoadResult : Result {
    std::optional<Credentials> credentials;
    std::optional<std::string> usernameHint;
    bool migrationAttempted = false;
    bool migrationSucceeded = false;
};

class CredentialBackend {
public:
    virtual ~CredentialBackend() = default;
    virtual std::string Name() const = 0;
    virtual bool IsAvailable(std::string& error) const = 0;
    virtual Result Save(const std::string& service, const Credentials& cred) = 0;
    virtual LoadResult Load(const std::string& service) = 0;
    virtual Result Clear(const std::string& service) = 0;
};

extern const char* kGdtfShareCredentialService;
extern const int kGdtfCredentialMetadataSchemaVersion;

void SetCredentialBackendForTesting(std::shared_ptr<CredentialBackend> backend);
void SetCredentialMetadataPathForTesting(const std::string& path);
std::string StatusName(Status status);
Result Save(const Credentials& cred);
LoadResult LoadDetailed();
std::optional<Credentials> Load();
Result ClearDetailed();
bool Clear();
}
