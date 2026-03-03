/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "credentialstore.h"
#include "simplecrypt.h"
#include "json.hpp"
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace CredentialStore {

static std::string GetCredFile()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    fs::path p = fs::path(dir.ToStdString());
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec) {
        ec.clear();
        p = fs::path(wxStandardPaths::Get().GetTempDir().ToStdString()) / "Perastage";
        fs::create_directories(p, ec);
        if (ec)
            return {};
    }
    p /= "gdtf_credentials.json";
    return p.string();
}

bool Save(const Credentials& cred)
{
    nlohmann::json j;
    j["username"] = cred.username;
    j["password"] = SimpleCrypt::Encode(cred.password);
    const std::string credFile = GetCredFile();
    if (credFile.empty())
        return false;
    std::ofstream out(credFile);
    if (!out.is_open())
        return false;
    out << j.dump(4);
    return true;
}

std::optional<Credentials> Load()
{
    const std::string credFile = GetCredFile();
    if (credFile.empty())
        return std::nullopt;
    std::ifstream in(credFile);
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
