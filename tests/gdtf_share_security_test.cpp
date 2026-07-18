#include "credentialstore.h"
#include "gdtfnet.h"
#include "gdtf_share_workflow.h"
#include "json.hpp"
#include "simplecrypt.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

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
    fs::path ownedPath;
    {
        GdtfShareClient client;
        ownedPath = client.CookiePathForTesting();
        assert(client.OwnsCookieForTesting());
        { std::ofstream out(ownedPath); out << "cookie"; }
        assert(fs::exists(ownedPath));
    }
    assert(!fs::exists(ownedPath));

    const fs::path externalPath = fs::temp_directory_path() / "perastage_external_cookie_test.txt";
    { std::ofstream out(externalPath); out << "cookie"; }
    {
        GdtfShareClient client(externalPath.string());
        assert(!client.OwnsCookieForTesting());
        assert(client.CookiePathForTesting() == externalPath);
    }
    assert(fs::exists(externalPath));
    fs::remove(externalPath);

    GdtfShareClient first;
    GdtfShareClient second;
    assert(first.CookiePathForTesting() != second.CookiePathForTesting());

    fs::path movedPath;
    {
        GdtfShareClient source;
        movedPath = source.CookiePathForTesting();
        { std::ofstream out(movedPath); out << "cookie"; }
        GdtfShareClient moved(std::move(source));
        assert(moved.OwnsCookieForTesting());
        assert(moved.CookiePathForTesting() == movedPath);
    }
    assert(!fs::exists(movedPath));
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
}

// Verifies credential storage orchestration and metadata omit passwords.
void TestCredentialStorage() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "perastage_credential_test";
    fs::remove_all(dir); fs::create_directories(dir);
    CredentialStore::SetCredentialMetadataPathForTesting((dir / "gdtf_credentials.json").string());
    auto backend = std::make_shared<FakeBackend>();
    CredentialStore::SetCredentialBackendForTesting(backend);
    CredentialStore::Credentials cred{"user", "secret"};
    CredentialStore::Result save = CredentialStore::Save(cred);
    assert(save.Succeeded());
    assert(save.metadataWritten);
    assert(save.secretWritten);
    auto loaded = CredentialStore::LoadDetailed();
    assert(loaded.credentials && loaded.credentials->password == "secret");
    backend->stored.reset();
    auto usernameOnly = CredentialStore::LoadDetailed();
    assert(!usernameOnly.credentials);
    assert(usernameOnly.usernameHint && *usernameOnly.usernameHint == "user");
    backend->stored = cred;
    std::ifstream in(dir / "gdtf_credentials.json");
    std::string meta((std::istreambuf_iterator<char>(in)), {});
    assert(meta.find("secret") == std::string::npos);
    assert(meta.find("password") == std::string::npos);
    assert(CredentialStore::ClearDetailed().Succeeded());
    assert(!CredentialStore::LoadDetailed().credentials);
    backend->available = false;
    CredentialStore::Result unavailableSave = CredentialStore::Save(cred);
    assert(unavailableSave.status == CredentialStore::Status::SecureStoreUnavailable);
    assert(unavailableSave.metadataWritten);
    assert(!unavailableSave.secretWritten);
    auto hintOnly = CredentialStore::LoadDetailed();
    assert(!hintOnly.credentials);
    assert(hintOnly.usernameHint && *hintOnly.usernameHint == "user");
    CredentialStore::SetCredentialBackendForTesting(nullptr);
    CredentialStore::SetCredentialMetadataPathForTesting("");
    fs::remove_all(dir);
}

// Verifies legacy JSON migration saves first, verifies, then removes the password field.
void TestLegacyMigration() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "perastage_migration_test";
    fs::remove_all(dir); fs::create_directories(dir);
    const fs::path file = dir / "gdtf_credentials.json";
    CredentialStore::SetCredentialMetadataPathForTesting(file.string());
    { std::ofstream out(file); out << nlohmann::json{{"username","legacy"},{"password",SimpleCrypt::Encode("legacy-secret")}}.dump(4); }
    auto backend = std::make_shared<FakeBackend>();
    CredentialStore::SetCredentialBackendForTesting(backend);
    auto loaded = CredentialStore::LoadDetailed();
    assert(loaded.credentials && loaded.credentials->password == "legacy-secret");
    assert(loaded.migrationSucceeded);
    std::ifstream in(file); std::string meta((std::istreambuf_iterator<char>(in)), {});
    assert(meta.find("password") == std::string::npos);
    backend->stored.reset(); backend->failSave = true;
    { std::ofstream out(file); out << nlohmann::json{{"username","keep"},{"password",SimpleCrypt::Encode("keep-secret")}}.dump(4); }
    auto failed = CredentialStore::LoadDetailed();
    assert(!failed.credentials);
    std::ifstream retained(file); std::string retainedMeta((std::istreambuf_iterator<char>(retained)), {});
    assert(retainedMeta.find("password") != std::string::npos);
    backend->failSave = false;
    { std::ofstream out(file); out << nlohmann::json{{"username","bad"},{"password","not-hex"}}.dump(4); }
    auto invalid = CredentialStore::LoadDetailed();
    assert(!invalid.credentials);
    assert(invalid.status == CredentialStore::Status::LegacyDataInvalid);
    CredentialStore::SetCredentialBackendForTesting(nullptr);
    CredentialStore::SetCredentialMetadataPathForTesting("");
    fs::remove_all(dir);
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
