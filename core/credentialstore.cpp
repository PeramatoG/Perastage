#include "credentialstore.h"
#include "simplecrypt.h"
#include "../external/json.hpp"
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace CredentialStore {

static std::string GetCredFile()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    fs::path p = fs::path(dir.ToStdString());
    fs::create_directories(p);
    p /= "gdtf_credentials.json";
    return p.string();
}

bool Save(const Credentials& cred)
{
    nlohmann::json j;
    j["username"] = cred.username;
    j["password"] = SimpleCrypt::Encode(cred.password);
    std::ofstream out(GetCredFile());
    if (!out.is_open())
        return false;
    out << j.dump(4);
    return true;
}

std::optional<Credentials> Load()
{
    std::ifstream in(GetCredFile());
    if (!in.is_open())
        return std::nullopt;
    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return std::nullopt;
    }
    Credentials c;
    c.username = j.value("username", "");
    c.password = SimpleCrypt::Decode(j.value("password", ""));
    if (c.username.empty())
        return std::nullopt;
    return c;
}

} // namespace CredentialStore

