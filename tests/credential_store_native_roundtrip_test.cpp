#include <wx/init.h>
#include <wx/secretstore.h>
#include <wx/string.h>
#include <cstdlib>
#include <ctime>
#include <utility>
#include <iostream>
#include <string>

namespace {

// Returns a unique service name that cannot overlap with production credentials.
std::string MakeServiceName() {
    const char* runId = std::getenv("GITHUB_RUN_ID");
    const char* attempt = std::getenv("GITHUB_RUN_ATTEMPT");
    if (runId && *runId) {
        return std::string("Perastage/Test/SecureStore/") + runId + "/" + (attempt && *attempt ? attempt : "0");
    }
    return std::string("Perastage/Test/SecureStore/local-") + std::to_string(static_cast<unsigned long long>(std::time(nullptr)));
}

// Deletes the test-only secret when the test exits.
class SecretCleanup final {
public:
    explicit SecretCleanup(std::string service) : service_(std::move(service)) {}
    ~SecretCleanup() {
#if defined(wxUSE_SECRETSTORE) && wxUSE_SECRETSTORE
        wxSecretStore store = wxSecretStore::GetDefault();
        if (store.IsOk()) {
            store.Delete(wxString::FromUTF8(service_));
        }
#endif
    }
private:
    std::string service_;
};

}

// Runs a test-only wxSecretStore native round trip without logging the password.
int main() {
#if !defined(wxUSE_SECRETSTORE) || !wxUSE_SECRETSTORE
    std::cout << "SKIP: wxSecretStore support is disabled in this wxWidgets build.\n";
    return 0;
#else
    wxInitializer initializer;
    if (!initializer.IsOk()) {
        std::cout << "SKIP: wxWidgets initialization failed in this environment.\n";
        return 0;
    }

    wxSecretStore store = wxSecretStore::GetDefault();
    if (!store.IsOk()) {
        std::cout << "SKIP: native secure credential store is unavailable in this environment.\n";
        return 0;
    }

    const std::string service = MakeServiceName();
    const std::string username = "perastage-native-store-test";
    const std::string password = "quote-\"-slash-\\-unicode-秘密";
    SecretCleanup cleanup(service);

    wxSecretValue secret(password.size(), password.data());
    if (!store.Save(wxString::FromUTF8(service), wxString::FromUTF8(username), secret)) {
        std::cout << "SKIP: native secure credential store rejected test save.\n";
        return 0;
    }

    wxString loadedUser;
    wxSecretValue loadedSecret;
    if (!store.Load(wxString::FromUTF8(service), loadedUser, loadedSecret)) {
        std::cerr << "Native secure credential store did not load the saved test entry.\n";
        return 1;
    }

    const auto userUtf8 = loadedUser.ToUTF8();
    const wxString loadedPasswordString = loadedSecret.GetAsString(wxMBConvUTF8());
    const auto passwordUtf8 = loadedPasswordString.ToUTF8();
    if ((userUtf8 ? std::string(userUtf8.data()) : std::string()) != username ||
        (passwordUtf8 ? std::string(passwordUtf8.data()) : std::string()) != password) {
        std::cerr << "Native secure credential store round trip changed the test credentials.\n";
        return 1;
    }

    if (!store.Delete(wxString::FromUTF8(service))) {
        std::cerr << "Native secure credential store did not delete the test entry.\n";
        return 1;
    }

    wxString afterDeleteUser;
    wxSecretValue afterDeleteSecret;
    if (store.Load(wxString::FromUTF8(service), afterDeleteUser, afterDeleteSecret)) {
        std::cerr << "Native secure credential store retained the deleted test entry.\n";
        return 1;
    }

    std::cout << "PASS: native secure credential store round trip completed.\n";
    return 0;
#endif
}
