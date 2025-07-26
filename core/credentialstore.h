#pragma once
#include <string>
#include <optional>

namespace CredentialStore {
    struct Credentials {
        std::string username;
        std::string password; // decoded
    };

    bool Save(const Credentials& cred);
    std::optional<Credentials> Load();
}
