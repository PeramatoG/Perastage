#include "credentialstore.h"
#include "configmanager.h"
#include "gdtfnet.h"
#include "gdtf_share_workflow.h"
#include "json.hpp"
#include "simplecrypt.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace {

class ScopedTestDirectory {
public:
    // Creates a unique temporary directory owned by this test scope.
    explicit ScopedTestDirectory(const std::string& prefix) {
        namespace fs = std::filesystem;
        std::error_code error;
        const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned int attempt = 0; attempt < 100; ++attempt) {
            path_ = fs::temp_directory_path(error) /
                    (prefix + "_" + std::to_string(seed) + "_" + std::to_string(attempt));
            if (error) break;
            if (fs::create_directory(path_, error)) return;
            if (error != std::errc::file_exists) break;
            error.clear();
        }
        error_ = "Unable to create temporary directory '" + path_.string() + "': " + error.message();
    }

    // Removes the owned directory without throwing during scope unwinding.
    ~ScopedTestDirectory() {
        if (!cleaned_ && !Cleanup()) std::cerr << error_ << '\n';
    }

    ScopedTestDirectory(const ScopedTestDirectory&) = delete;
    ScopedTestDirectory& operator=(const ScopedTestDirectory&) = delete;

    // Returns the unique directory path created for the test.
    const std::filesystem::path& Path() const { return path_; }

    // Reports whether temporary-directory setup completed successfully.
    bool IsValid() const { return error_.empty(); }

    // Returns the most recent setup or cleanup diagnostic.
    const std::string& Error() const { return error_; }

    // Removes the directory tree and records an actionable failure diagnostic.
    bool Cleanup() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        cleaned_ = !error;
        if (error) {
            error_ = "Unable to remove temporary directory '" + path_.string() + "': " + error.message();
        }
        return cleaned_;
    }

private:
    std::filesystem::path path_;
    std::string error_;
    bool cleaned_ = false;
};

class ScopedCredentialStoreOverrides {
public:
    // Restores credential-store overrides and lightweight configuration values.
    ~ScopedCredentialStoreOverrides() { Reset(); }

    // Restores all test process state changed by credential-store scenarios.
    void Reset() {
        if (reset_) return;
        CredentialStore::SetCredentialBackendForTesting(nullptr);
        CredentialStore::SetCredentialMetadataPathForTesting("");
        ConfigManager::Get().ClearValues();
        reset_ = true;
    }

private:
    bool reset_ = false;
};

// Writes complete test content while keeping the output stream in a narrow scope.
bool WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        std::cerr << "Unable to open test file for writing: " << path << '\n';
        return false;
    }
    output << content;
    output.close();
    if (!output) std::cerr << "Unable to complete test file write: " << path << '\n';
    return output.good();
}

// Reads complete test content while keeping the input stream in a narrow scope.
bool ReadTextFile(const std::filesystem::path& path, std::string& content) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "Unable to open test file for reading: " << path << '\n';
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        std::cerr << "Unable to complete test file read: " << path << '\n';
        return false;
    }
    content = buffer.str();
    return true;
}

// Reports and asserts temporary-directory setup failures at their call site.
void RequireValidDirectory(const ScopedTestDirectory& directory) {
    if (!directory.IsValid()) std::cerr << directory.Error() << '\n';
    assert(directory.IsValid());
}

// Reports and asserts temporary-directory cleanup failures at their call site.
void RequireDirectoryCleanup(ScopedTestDirectory& directory) {
    const bool cleaned = directory.Cleanup();
    if (!cleaned) std::cerr << directory.Error() << '\n';
    assert(cleaned);
}

} // namespace

// Returns true when any JSON string value equals the provided sentinel.
bool ContainsJsonStringValue(const nlohmann::json& value, const std::string& sentinel) {
    if (value.is_string()) return value.get<std::string>() == sentinel;
    if (value.is_array()) {
        for (const auto& item : value) { if (ContainsJsonStringValue(item, sentinel)) return true; }
        return false;
    }
    if (value.is_object()) {
        for (const auto& item : value.items()) { if (ContainsJsonStringValue(item.value(), sentinel)) return true; }
    }
    return false;
}

// Returns true when any JSON object key equals the provided field name.
bool ContainsJsonObjectKey(const nlohmann::json& value, const std::string& fieldName) {
    if (value.is_array()) {
        for (const auto& item : value) { if (ContainsJsonObjectKey(item, fieldName)) return true; }
        return false;
    }
    if (value.is_object()) {
        for (const auto& item : value.items()) {
            if (item.key() == fieldName || ContainsJsonObjectKey(item.value(), fieldName)) return true;
        }
    }
    return false;
}

class FakeBackend final : public CredentialStore::CredentialBackend {
public:
    bool available = true;
    bool failSave = false;
    bool failLoad = false;
    bool failClear = false;
    std::optional<CredentialStore::Credentials> stored;
    std::string Name() const override { return "fake"; }
    bool IsAvailable(std::string& error) const override { if (available) return true; error = "unavailable"; return false; }
    CredentialStore::Result Save(const std::string&, const CredentialStore::Credentials& cred) override { if (!available) return {CredentialStore::Status::SecureStoreUnavailable, "unavailable"}; if (failSave) return {CredentialStore::Status::SecureStoreAccessFailed, "save failed"}; stored = cred; CredentialStore::Result r; r.secretWritten = true; return r; }
    CredentialStore::LoadResult Load(const std::string&) override { CredentialStore::LoadResult r; if (!available) { r.status = CredentialStore::Status::SecureStoreUnavailable; return r; } if (failLoad) { r.status = CredentialStore::Status::SecureStoreAccessFailed; return r; } if (!stored) { r.status = CredentialStore::Status::NotFound; return r; } r.credentials = stored; return r; }
    CredentialStore::Result Clear(const std::string&) override { if (!available) return {CredentialStore::Status::SecureStoreUnavailable, "unavailable"}; if (failClear) return {CredentialStore::Status::SecureStoreAccessFailed, "clear failed"}; stored.reset(); return {}; }
};

// Verifies login JSON preserves special characters exactly.
void TestLoginJson() {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"ordinary", "password"}, {"quote\"user", "pass\"word"}, {"slash\\user", "slash\\pass"},
        {"unicode-é-用户", "päss-秘密"}, {"space user", "apostrophe' amp& punct!"}, {"ctrl", std::string("a\nb\tc", 5)}};
    for (const auto& [u, p] : cases) {
        const auto body = BuildGdtfLoginRequestBody(u, p);
        const auto json = nlohmann::json::parse(body);
        assert(json.at("user") == u);
        assert(json.at("password") == p);
    }
}

// Verifies API response mapping distinguishes major failure modes.
void TestResponseMapping() {
    assert(MapGdtfShareResponse(200, 0, "", R"({"result":true,"notice":"ok"})", "application/json", 1).category == GdtfShareResultCategory::Success);
    assert(MapGdtfShareResponse(200, 0, "", R"({"result":false,"error":"bad"})", "application/json", 1).category == GdtfShareResultCategory::ApiRejected);
    assert(MapGdtfShareResponse(400, 0, "", R"({"result":false,"error":"malformed"})", "application/json", 1).category == GdtfShareResultCategory::ApiRejected);
    assert(MapGdtfShareResponse(401, 0, "", R"({"result":false,"error":"No valid user"})", "application/json", 1).category == GdtfShareResultCategory::AuthenticationRejected);
    assert(MapGdtfShareResponse(500, 0, "", R"({"result":false,"error":"server"})", "application/json", 1).category == GdtfShareResultCategory::HttpError);
    assert(MapGdtfShareResponse(200, 0, "", "not-json", "text/plain", 1).category == GdtfShareResultCategory::InvalidJsonResponse);
    assert(MapGdtfShareResponse(200, 0, "", R"({"notice":"missing"})", "application/json", 1).category == GdtfShareResultCategory::InvalidResponseSchema);
    assert(MakeGdtfShareTransportResult(7, "connect", 1).category == GdtfShareResultCategory::TransportError);
    assert(MakeGdtfShareTimeoutResult(28, "timeout", 1).category == GdtfShareResultCategory::Timeout);
}


// Verifies session cookie path ownership and move semantics.
void TestCookieOwnership() {
    namespace fs = std::filesystem;
    ScopedTestDirectory directory("perastage_cookie_security_test");
    RequireValidDirectory(directory);
    fs::path ownedPath;
    {
        GdtfShareClient client;
        ownedPath = client.CookiePathForTesting();
        assert(client.OwnsCookieForTesting());
        assert(WriteTextFile(ownedPath, "cookie"));
        assert(fs::exists(ownedPath));
    }
    assert(!fs::exists(ownedPath));

    const fs::path externalPath = directory.Path() / "external_cookie.txt";
    assert(WriteTextFile(externalPath, "cookie"));
    {
        GdtfShareClient client(externalPath.string());
        assert(!client.OwnsCookieForTesting());
        assert(client.CookiePathForTesting() == externalPath);
    }
    assert(fs::exists(externalPath));
    std::error_code removeError;
    assert(fs::remove(externalPath, removeError));
    assert(!removeError);

    GdtfShareClient first;
    GdtfShareClient second;
    assert(first.CookiePathForTesting() != second.CookiePathForTesting());

    fs::path movedPath;
    {
        GdtfShareClient source;
        movedPath = source.CookiePathForTesting();
        assert(WriteTextFile(movedPath, "cookie"));
        GdtfShareClient moved(std::move(source));
        assert(moved.OwnsCookieForTesting());
        assert(moved.CookiePathForTesting() == movedPath);
    }
    assert(!fs::exists(movedPath));
    RequireDirectoryCleanup(directory);
}

// Verifies strict legacy decoding rejects malformed migration inputs.
void TestStrictLegacyDecode() {
    const auto valid = SimpleCrypt::DecodeStrict(SimpleCrypt::Encode("päss-秘密"));
    assert(valid.success);
    assert(valid.value == "päss-秘密");
    assert(!SimpleCrypt::DecodeStrict("abc").success);
    assert(!SimpleCrypt::DecodeStrict("zz").success);
    assert(!SimpleCrypt::DecodeStrict("5aXX").success);
    assert(!SimpleCrypt::DecodeStrict("").success);
}


// Verifies credential prompt decisions do not treat transport failures as bad credentials.
void TestWorkflowPromptDecisions() {
    using namespace gdtf_share_workflow;
    CredentialStore::LoadResult none;
    assert(DetermineCredentialAvailability(none) == CredentialAvailability::None);
    none.usernameHint = "operator";
    assert(DetermineCredentialAvailability(none) == CredentialAvailability::UsernameHintOnly);
    none.credentials = CredentialStore::Credentials{"operator", "secret"};
    assert(DetermineCredentialAvailability(none) == CredentialAvailability::Complete);

    assert(PromptReasonForAuthenticationResult(false, std::nullopt) ==
           CredentialPromptReason::MissingCredentials);
    GdtfShareResult transport;
    transport.category = GdtfShareResultCategory::TransportError;
    assert(PromptReasonForAuthenticationResult(true, transport) ==
           CredentialPromptReason::None);
    GdtfShareResult rejected;
    rejected.category = GdtfShareResultCategory::AuthenticationRejected;
    assert(PromptReasonForAuthenticationResult(true, rejected) ==
           CredentialPromptReason::RejectedCredentials);
    assert(PromptReasonForAuthenticationResult(true, transport, true) ==
           CredentialPromptReason::ExpiredSession);

    assert(DetermineCatalogAccessAction(false, CredentialAvailability::None,
                                        std::nullopt, false) ==
           CatalogAccessAction::RequestCredentials);
    assert(DetermineCatalogAccessAction(true, CredentialAvailability::None,
                                        std::nullopt, false) ==
           CatalogAccessAction::OpenCachedCatalog);
    assert(DetermineCatalogAccessAction(false, CredentialAvailability::Complete,
                                        rejected, false) ==
           CatalogAccessAction::RequestCredentials);
    assert(DetermineCatalogAccessAction(true, CredentialAvailability::Complete,
                                        rejected, false) ==
           CatalogAccessAction::OpenCachedCatalog);
    assert(DetermineCatalogAccessAction(false, CredentialAvailability::Complete,
                                        std::nullopt, true) ==
           CatalogAccessAction::OpenOnlineCatalog);
    assert(DetermineCatalogAccessAction(false, CredentialAvailability::Complete,
                                        transport, false) ==
           CatalogAccessAction::ReportCatalogFailure);
    assert(DetermineCatalogAccessAction(false, CredentialAvailability::None,
                                        std::nullopt, false, true) ==
           CatalogAccessAction::Cancel);
    assert(DetermineCatalogAccessAction(true, CredentialAvailability::Complete,
                                        transport, false) ==
           CatalogAccessAction::OpenCachedCatalog);
}

// Verifies credential storage orchestration and metadata omit passwords.
void TestCredentialStorage() {
    namespace fs = std::filesystem;
    ScopedTestDirectory directory("perastage_credential_security_test");
    RequireValidDirectory(directory);
    ScopedCredentialStoreOverrides overrides;
    const fs::path dir = directory.Path();
    CredentialStore::SetCredentialMetadataPathForTesting((dir / "gdtf_credentials.json").string());
    auto backend = std::make_shared<FakeBackend>();
    CredentialStore::SetCredentialBackendForTesting(backend);
    const std::string username = "perastage-test-user";
    const std::string password = "perastage-test-password-gdtf-share-security-sentinel";
    CredentialStore::Credentials cred{username, password};
    CredentialStore::Result save = CredentialStore::Save(cred);
    assert(save.Succeeded());
    assert(save.metadataWritten);
    assert(save.secretWritten);
    auto loaded = CredentialStore::LoadDetailed();
    assert(loaded.credentials && loaded.credentials->password == password);
    backend->stored.reset();
    auto usernameOnly = CredentialStore::LoadDetailed();
    assert(!usernameOnly.credentials);
    assert(usernameOnly.usernameHint && *usernameOnly.usernameHint == username);
    backend->stored = cred;
    {
        std::ifstream in(dir / "gdtf_credentials.json");
        assert(in.is_open());
        const auto metadata = nlohmann::json::parse(in);
        assert(!in.bad());
        assert(metadata.is_object());
        assert(metadata.size() == 4);
        assert(metadata.at("schema_version") == CredentialStore::kGdtfCredentialMetadataSchemaVersion);
        assert(metadata.at("backend") == "wx_secret_store");
        assert(metadata.at("service") == CredentialStore::kGdtfShareCredentialService);
        assert(metadata.at("username") == username);
        assert(!ContainsJsonObjectKey(metadata, "password"));
        assert(!ContainsJsonObjectKey(metadata, "secret"));
        assert(!ContainsJsonStringValue(metadata, password));
    }
    const CredentialStore::Result clearResult = CredentialStore::ClearDetailed();
    if (!clearResult.Succeeded()) {
        std::cerr << "Credential metadata clear failed: status="
                  << static_cast<int>(clearResult.status)
                  << " message=" << clearResult.message << '\n';
    }
    assert(clearResult.Succeeded());
    assert(!fs::exists(dir / "gdtf_credentials.json"));
    assert(!CredentialStore::LoadDetailed().credentials);
    backend->available = false;
    CredentialStore::Result unavailableSave = CredentialStore::Save(cred);
    assert(unavailableSave.status == CredentialStore::Status::SecureStoreUnavailable);
    assert(unavailableSave.metadataWritten);
    assert(!unavailableSave.secretWritten);
    auto hintOnly = CredentialStore::LoadDetailed();
    assert(!hintOnly.credentials);
    assert(hintOnly.usernameHint && *hintOnly.usernameHint == username);
    overrides.Reset();
    RequireDirectoryCleanup(directory);
}

// Verifies legacy JSON migration saves first, verifies, then removes the password field.
void TestLegacyMigration() {
    ScopedTestDirectory directory("perastage_credential_migration_test");
    RequireValidDirectory(directory);
    ScopedCredentialStoreOverrides overrides;
    const std::filesystem::path file = directory.Path() / "gdtf_credentials.json";
    CredentialStore::SetCredentialMetadataPathForTesting(file.string());
    assert(WriteTextFile(file, nlohmann::json{{"username","legacy"},{"password",SimpleCrypt::Encode("legacy-secret")}}.dump(4)));
    auto backend = std::make_shared<FakeBackend>();
    CredentialStore::SetCredentialBackendForTesting(backend);
    auto loaded = CredentialStore::LoadDetailed();
    assert(loaded.credentials && loaded.credentials->password == "legacy-secret");
    assert(loaded.migrationSucceeded);
    std::string meta;
    assert(ReadTextFile(file, meta));
    assert(meta.find("password") == std::string::npos);
    backend->stored.reset(); backend->failSave = true;
    assert(WriteTextFile(file, nlohmann::json{{"username","keep"},{"password",SimpleCrypt::Encode("keep-secret")}}.dump(4)));
    auto failed = CredentialStore::LoadDetailed();
    assert(!failed.credentials);
    std::string retainedMeta;
    assert(ReadTextFile(file, retainedMeta));
    assert(retainedMeta.find("password") != std::string::npos);
    backend->failSave = false;
    assert(WriteTextFile(file, nlohmann::json{{"username","bad"},{"password","not-hex"}}.dump(4)));
    auto invalid = CredentialStore::LoadDetailed();
    assert(!invalid.credentials);
    assert(invalid.status == CredentialStore::Status::LegacyDataInvalid);
    overrides.Reset();
    RequireDirectoryCleanup(directory);
}

// Verifies diagnostic redaction helpers do not expose secrets accidentally.
void TestRedaction() {
    assert(MaskGdtfShareUsernameForDiagnostics("person@example.com").find("example") == std::string::npos);
    assert(MaskGdtfShareUsernameForDiagnostics("operator").find("operator") == std::string::npos);
    const std::string sanitized = SanitizeGdtfShareApiMessage(std::string("bad\nsecret\t") + std::string(400, 'x'), 20);
    assert(sanitized.find('\n') == std::string::npos);
    assert(sanitized.size() <= 23);
}

// Runs focused GDTF Share security tests.
int main() {
    TestLoginJson();
    TestResponseMapping();
    TestCookieOwnership();
    TestStrictLegacyDecode();
    TestWorkflowPromptDecisions();
    TestCredentialStorage();
    TestLegacyMigration();
    TestRedaction();
    return 0;
}
