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
#include "projectutils.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace fs = std::filesystem;

namespace ProjectUtils {

namespace {

void CopyLibrarySubdir(const fs::path& source, const fs::path& destination)
{
    std::error_code ec;
    bool destinationExists = fs::exists(destination, ec);
    ec.clear();
    bool destinationEmpty = destinationExists ? fs::is_empty(destination, ec) : true;

    if (!destinationExists)
        fs::create_directories(destination, ec);

    if ((destinationEmpty || !destinationExists) && fs::exists(source)) {
        ec.clear();
        fs::copy(source, destination,
                 fs::copy_options::recursive | fs::copy_options::update_existing, ec);
    }
}

} // namespace

fs::path GetBaseLibraryPath(const std::string& subdir)
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path baseLib = exeBase / "library" / subdir;

    if (!fs::exists(baseLib)) {
        fs::path cwdLib = fs::current_path() / "library" / subdir;
        if (fs::exists(cwdLib))
            baseLib = cwdLib;
    }

    return baseLib;
}

fs::path GetResourceRoot()
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path exeResources = exeBase / "resources";

    if (fs::exists(exeResources))
        return exeResources;

    fs::path cwdResources = fs::current_path() / "resources";
    if (fs::exists(cwdResources))
        return cwdResources;

    return exeResources;
}

std::string GetLastProjectPathFile()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    fs::path p = fs::path(dir.ToStdString());
    fs::create_directories(p);
    p /= "last_project.txt";
    return p.string();
}

bool SaveLastProjectPath(const std::string& path)
{
    std::ofstream out(GetLastProjectPathFile());
    if (!out.is_open())
        return false;
    out << path;
    return true;
}

std::optional<std::string> LoadLastProjectPath()
{
    std::ifstream in(GetLastProjectPathFile());
    if (!in.is_open())
        return std::nullopt;
    std::string path;
    std::getline(in, path);
    if (path.empty())
        return std::nullopt;
    return path;
}

std::string GetDefaultLibraryPath(const std::string& subdir)
{
    if (const char* envPath = std::getenv("PERASTAGE_LIBRARY_PATH")) {
        if (*envPath != '\0') {
            fs::path envBase = fs::u8path(envPath) / subdir;
            std::error_code ec;
            fs::create_directories(envBase, ec);
            if (!ec)
                return fs::absolute(envBase).string();
        }
    }

#ifdef NDEBUG
    fs::path baseLib = GetBaseLibraryPath(subdir);
    fs::path userLib = fs::path(wxStandardPaths::Get().GetUserDataDir().ToStdString()) /
                      "library" / subdir;

    CopyLibrarySubdir(baseLib, userLib);

    return userLib.string();
#else
    fs::path baseLib = GetBaseLibraryPath(subdir);
    if (fs::exists(baseLib))
        return baseLib.string();

    std::error_code ec;
    fs::create_directories(baseLib, ec);
    if (!ec)
        return baseLib.string();

    return {};
#endif
}

} // namespace ProjectUtils
