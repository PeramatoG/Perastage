/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 */
#include "credentialstore.h"
#include "apppaths.h"
#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "json.hpp"
#include "simplecrypt.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#if __has_include(<wx/secretstore.h>)
#include <wx/secretstore.h>
#endif
#include <wx/stdpaths.h>

namespace fs = std::filesystem;

namespace CredentialStore {
const char* kGdtfShareCredentialService = "Perastage/GDTF Share/gdtf-share.com";
const int kGdtfCredentialMetadataSchemaVersion = 2;

namespace {
std::shared_ptr<CredentialBackend> g_backendOverride;
std::string g_metadataPathOverride;

// Returns the active metadata file path and migrates the old path when present.
std::string GetCredFile() {
    if (!g_metadataPathOverride.empty()) return g_metadataPathOverride;
    fs::path p = AppPaths::GetUserDataDir();
    std::error_code ec; fs::create_directories(p, ec);
    if (ec) { ec.clear(); p = AppPaths::GetUserDataTempFallbackDir(); fs::create_directories(p, ec); if (ec) return {}; }
    const fs::path target = p / "gdtf_credentials.json";
    const fs::path legacy = fs::path(wxStandardPaths::Get().GetUserDataDir().ToStdString()) / "gdtf_credentials.json";
    if (target != legacy && !fs::exists(target, ec) && fs::exists(legacy, ec)) fs::copy_file(legacy, target, fs::copy_options::skip_existing, ec);
    return target.string();
}

// Returns a unique sibling path for credential metadata transactions.
fs::path MakeUniqueMetadataSibling(const fs::path& file, const std::string& tag) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        fs::path candidate = file.parent_path() / (file.filename().string() + tag +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "_" + std::to_string(attempt));
        std::error_code existsEc;
        if (!fs::exists(candidate, existsEc))
            return candidate;
    }
    return file.parent_path() / (file.filename().string() + tag + "fallback");
}

// Writes non-secret credential metadata atomically.
Result WriteMetadata(const std::string& username) {
    const std::string fileText = GetCredFile();
    if (fileText.empty()) return {Status::MetadataWriteFailed, "Credential metadata path unavailable"};
    const fs::path file(fileText);
    std::error_code mkdirEc;
    fs::create_directories(file.parent_path(), mkdirEc);
    if (mkdirEc) return {Status::MetadataWriteFailed, "Could not create credential metadata directory"};
    nlohmann::json j{{"schema_version", kGdtfCredentialMetadataSchemaVersion}, {"backend", "wx_secret_store"}, {"service", kGdtfShareCredentialService}, {"username", username}};
    const fs::path tmp = MakeUniqueMetadataSibling(file, ".tmp.");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return {Status::MetadataWriteFailed, "Could not write credential metadata"};
        out << j.dump(4);
        out.flush();
        if (!out.good()) { std::error_code removeEc; fs::remove(tmp, removeEc); return {Status::MetadataWriteFailed, "Could not flush credential metadata"}; }
    }
#ifndef _WIN32
    std::error_code permEc; fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, permEc);
#endif
    std::error_code renameEc;
    fs::rename(tmp, file, renameEc);
    if (!renameEc) { Result result; result.metadataWritten = true; return result; }

    const fs::path backup = MakeUniqueMetadataSibling(file, ".backup.");
    std::error_code backupEc;
    if (fs::exists(file, backupEc)) {
        fs::rename(file, backup, backupEc);
        if (backupEc) { std::error_code removeEc; fs::remove(tmp, removeEc); return {Status::MetadataWriteFailed, "Could not preserve previous credential metadata"}; }
    }
    std::error_code publishEc;
    fs::rename(tmp, file, publishEc);
    if (publishEc) {
        std::error_code restoreEc;
        if (fs::exists(backup, restoreEc))
            fs::rename(backup, file, restoreEc);
        std::error_code removeEc;
        fs::remove(tmp, removeEc);
        return {Status::MetadataWriteFailed, restoreEc ? "Could not publish credential metadata; previous metadata backup was retained" : "Could not publish credential metadata"};
    }
    std::error_code cleanupEc;
    fs::remove(backup, cleanupEc);
    Result result; result.metadataWritten = true; return result;
}

// Reads credential metadata without treating corruption as secure-store deletion permission.
std::optional<nlohmann::json> ReadMetadata(Status& status) {
    const std::string file = GetCredFile();
    if (file.empty()) { status = Status::MetadataReadFailed; return std::nullopt; }
    std::ifstream in(file);
    if (!in.is_open()) { status = Status::NotFound; return std::nullopt; }
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { status = Status::MetadataReadFailed; return std::nullopt; }
    status = Status::Success; return j;
}

// Loads and decodes the legacy JSON credential format in memory only.
std::optional<Credentials> LoadLegacyJson(Status& status) {
    auto j = ReadMetadata(status);
    if (!j || j->value("schema_version", 1) >= kGdtfCredentialMetadataSchemaVersion) return std::nullopt;
    if (!j->contains("password") || !(*j)["password"].is_string()) return std::nullopt;
    const auto decoded = SimpleCrypt::DecodeStrict((*j)["password"].get<std::string>());
    if (!decoded.success) { status = Status::LegacyDataInvalid; return std::nullopt; }
    Credentials c{j->value("username", ""), decoded.value};
    if (c.username.empty() || c.password.empty()) { status = Status::LegacyDataInvalid; return std::nullopt; }
    return c;
}

// Loads and decodes the older ConfigManager credential keys in memory only.
std::optional<Credentials> LoadLegacyConfig() {
    ConfigManager &cfg = ConfigManager::Get();
    const auto decoded = SimpleCrypt::DecodeStrict(cfg.GetValue("gdtf_password").value_or(""));
    if (!decoded.success) return std::nullopt;
    Credentials c{cfg.GetValue("gdtf_username").value_or(""), decoded.value};
    if (c.username.empty() || c.password.empty()) return std::nullopt;
    return c;
}

class WxSecretCredentialBackend final : public CredentialBackend {
public:
    // Returns a descriptive backend name for diagnostics.
    std::string Name() const override { return "wx_secret_store"; }
    // Reports whether wxSecretStore can use an operating-system credential store.
    bool IsAvailable(std::string& error) const override {
#if defined(wxUSE_SECRETSTORE) && wxUSE_SECRETSTORE
        wxSecretStore store = wxSecretStore::GetDefault();
        if (store.IsOk()) return true;
        error = "wxSecretStore is not available"; return false;
#else
        error = "wxSecretStore support is disabled in this wxWidgets build"; return false;
#endif
    }
    // Saves credentials to the operating-system credential store through wxWidgets.
    Result Save(const std::string& service, const Credentials& cred) override {
#if defined(wxUSE_SECRETSTORE) && wxUSE_SECRETSTORE
        std::string error; if (!IsAvailable(error)) return {Status::SecureStoreUnavailable, error};
        wxSecretStore store = wxSecretStore::GetDefault();
        wxSecretValue secret(cred.password.size(), cred.password.data());
        if (!store.Save(wxString::FromUTF8(service), wxString::FromUTF8(cred.username), secret))
            return {Status::SecureStoreAccessFailed, "wxSecretStore save failed"};
        Result result; result.secretWritten = true; return result;
#else
        (void)service; (void)cred; return {Status::SecureStoreUnavailable, "wxSecretStore support is disabled in this wxWidgets build"};
#endif
    }
    // Loads credentials from the operating-system credential store through wxWidgets.
    LoadResult Load(const std::string& service) override {
        LoadResult r;
#if defined(wxUSE_SECRETSTORE) && wxUSE_SECRETSTORE
        std::string error; if (!IsAvailable(error)) { r.status = Status::SecureStoreUnavailable; r.message = error; return r; }
        wxSecretStore store = wxSecretStore::GetDefault(); wxString user; wxSecretValue secret;
        if (!store.Load(wxString::FromUTF8(service), user, secret)) { r.status = Status::NotFound; return r; }
        r.credentials = Credentials{user.ToStdString(), secret.GetAsString(wxMBConvUTF8()).ToStdString()}; return r;
#else
        (void)service; r.status = Status::SecureStoreUnavailable; r.message = "wxSecretStore support is disabled in this wxWidgets build"; return r;
#endif
    }
    // Clears credentials from the operating-system credential store through wxWidgets.
    Result Clear(const std::string& service) override {
#if defined(wxUSE_SECRETSTORE) && wxUSE_SECRETSTORE
        std::string error; if (!IsAvailable(error)) return {Status::SecureStoreUnavailable, error};
        wxSecretStore store = wxSecretStore::GetDefault();
        if (!store.Delete(wxString::FromUTF8(service)))
            return {Status::SecureStoreAccessFailed, "wxSecretStore delete could not be confirmed"};
        return Result{};
#else
        (void)service; return {Status::SecureStoreUnavailable, "wxSecretStore support is disabled in this wxWidgets build"};
#endif
    }
};

// Returns the active credential backend.
std::shared_ptr<CredentialBackend> Backend() { static std::shared_ptr<CredentialBackend> b = std::make_shared<WxSecretCredentialBackend>(); return g_backendOverride ? g_backendOverride : b; }
}

// Installs a fake backend for unit tests.
void SetCredentialBackendForTesting(std::shared_ptr<CredentialBackend> backend) { g_backendOverride = std::move(backend); }

// Overrides the metadata path for unit tests.
void SetCredentialMetadataPathForTesting(const std::string& path) { g_metadataPathOverride = path; }

// Converts credential store status to stable diagnostics text.
std::string StatusName(Status status) { switch (status) { case Status::Success: return "Success"; case Status::NotFound: return "NotFound"; case Status::SecureStoreUnavailable: return "SecureStoreUnavailable"; case Status::SecureStoreAccessFailed: return "SecureStoreAccessFailed"; case Status::MetadataReadFailed: return "MetadataReadFailed"; case Status::MetadataWriteFailed: return "MetadataWriteFailed"; case Status::LegacyDataInvalid: return "LegacyDataInvalid"; case Status::MigrationFailed: return "MigrationFailed"; } return "Unknown"; }

// Saves GDTF Share credentials without writing the password to JSON metadata.
Result Save(const Credentials& cred) {
    auto b = Backend();
    std::string error;
    if (!b->IsAvailable(error)) {
        Result mr = WriteMetadata(cred.username);
        return {mr.Succeeded() ? Status::SecureStoreUnavailable : mr.status, error, mr.metadataWritten || mr.Succeeded(), false};
    }
    Result sr = b->Save(kGdtfShareCredentialService, cred);
    if (!sr.Succeeded())
        return sr;
    Result mr = WriteMetadata(cred.username);
    mr.secretWritten = true;
    mr.metadataWritten = mr.Succeeded();
    diagnostics::DiagnosticLogger::Info("GDTF credential save: backend=" + b->Name() + " status=" + StatusName(mr.status));
    return mr;
}

// Saves only non-secret username metadata for credential prompts.
Result SaveUsernameMetadataOnly(const std::string& username) { return WriteMetadata(username); }

// Loads credentials and safely migrates legacy recoverable values when needed.
LoadResult LoadDetailed() {
    Status metadataStatus = Status::Success;
    std::optional<nlohmann::json> metadata = ReadMetadata(metadataStatus);
    std::optional<std::string> usernameHint;
    if (metadata && metadata->value("schema_version", 1) >= kGdtfCredentialMetadataSchemaVersion && metadata->contains("username") && (*metadata)["username"].is_string())
        usernameHint = (*metadata)["username"].get<std::string>();

    auto b = Backend();
    LoadResult secure = b->Load(kGdtfShareCredentialService);
    secure.usernameHint = usernameHint;
    if (secure.credentials) {
        if (usernameHint && secure.credentials->username != *usernameHint)
            secure.usernameHint = secure.credentials->username;
        return secure;
    }

    Status legacyStatus = Status::Success;
    std::optional<Credentials> legacy = LoadLegacyJson(legacyStatus);
    if (!legacy)
        legacy = LoadLegacyConfig();
    if (!legacy) {
        secure.status = secure.status == Status::Success ? Status::NotFound : secure.status;
        if (legacyStatus == Status::LegacyDataInvalid)
            secure.status = Status::LegacyDataInvalid;
        return secure;
    }

    secure.migrationAttempted = true;
    Result backendSave = b->Save(kGdtfShareCredentialService, *legacy);
    if (!backendSave.Succeeded()) { secure.status = Status::MigrationFailed; secure.message = backendSave.message; return secure; }
    LoadResult verify = b->Load(kGdtfShareCredentialService);
    if (!verify.credentials || verify.credentials->username != legacy->username || verify.credentials->password != legacy->password) {
        b->Clear(kGdtfShareCredentialService);
        secure.status = Status::MigrationFailed;
        secure.message = "Secure-store verification failed";
        return secure;
    }
    Result metadataWrite = WriteMetadata(legacy->username);
    if (!metadataWrite.Succeeded()) {
        b->Clear(kGdtfShareCredentialService);
        secure.status = Status::MigrationFailed;
        secure.message = metadataWrite.message;
        return secure;
    }
    ConfigManager &cfg = ConfigManager::Get();
    cfg.SetValue("gdtf_password", "");
    verify.usernameHint = legacy->username;
    verify.migrationAttempted = true;
    verify.migrationSucceeded = true;
    diagnostics::DiagnosticLogger::Info("GDTF credential migration succeeded.");
    return verify;
}

// Loads credentials using the compatibility optional interface.
std::optional<Credentials> Load() { return LoadDetailed().credentials; }

// Clears secure credentials, metadata, and legacy password values centrally.
Result ClearDetailed() {
    Result br = Backend()->Clear(kGdtfShareCredentialService);
    if (!br.Succeeded() && br.status != Status::NotFound) {
        ConfigManager &cfg = ConfigManager::Get();
        cfg.SetValue("gdtf_password", "");
        diagnostics::DiagnosticLogger::Info("GDTF credential clear: status=" + StatusName(br.status));
        return br;
    }
    const std::string file = GetCredFile();
    std::error_code ec;
    if (!file.empty())
        fs::remove(file, ec);
    if (ec)
        return {Status::MetadataWriteFailed, "Could not remove credential metadata"};
    ConfigManager &cfg = ConfigManager::Get();
    cfg.SetValue("gdtf_username", "");
    cfg.SetValue("gdtf_password", "");
    diagnostics::DiagnosticLogger::Info("GDTF credential clear: status=" + StatusName(br.status));
    return {};
}

// Clears credentials through the compatibility boolean interface.
bool Clear() { return ClearDetailed().Succeeded(); }
}
