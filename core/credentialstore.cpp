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
#include "apppaths.h"
#include "logger.h"
#include "simplecrypt.h"
#include "json.hpp"
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace CredentialStore {

static std::string GetCredFile()
{
    fs::path p = AppPaths::GetUserDataDir();
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec) {
        ec.clear();
        p = AppPaths::GetUserDataTempFallbackDir();
        fs::create_directories(p, ec);
        if (ec)
            return {};
    }
    const fs::path target = p / "gdtf_credentials.json";
    const fs::path legacy =
        fs::path(wxStandardPaths::Get().GetUserDataDir().ToStdString()) /
        "gdtf_credentials.json";
    if (target != legacy && !fs::exists(target, ec) && fs::exists(legacy, ec)) {
        ec.clear();
        fs::copy_file(legacy, target, fs::copy_options::skip_existing, ec);
        Logger::Instance().Log(
            ec ? Logger::Level::Warn : Logger::Level::Info,
            ec ? "Could not migrate GDTF credentials to local app data."
               : "Migrated GDTF credentials to local app data.");
    }
    return target.string();
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


bool Clear()
{
    const std::string credFile = GetCredFile();
    if (credFile.empty())
        return false;
    std::error_code ec;
    if (!fs::exists(credFile, ec))
        return true;
    fs::remove(credFile, ec);
    return !ec;
}

} // namespace CredentialStore
