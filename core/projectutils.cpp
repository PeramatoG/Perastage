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
#include <optional>
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

std::optional<fs::path> FindExistingPath(const fs::path& start,
                                         const fs::path& suffix,
                                         int maxDepth = 3)
{
    std::error_code ec;
    fs::path current = start;
    for (int depth = 0; depth <= maxDepth; ++depth) {
        fs::path candidate = current / suffix;
        if (fs::exists(candidate, ec))
            return candidate;
        ec.clear();
        if (!current.has_parent_path())
            break;
        current = current.parent_path();
    }
    return std::nullopt;
}

} // namespace

fs::path GetBaseLibraryPath(const std::string& subdir)
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path suffix = fs::path("library") / subdir;
    if (auto found = FindExistingPath(exeBase, suffix))
        return *found;
    if (auto found = FindExistingPath(fs::current_path(), suffix))
        return *found;
    return exeBase / suffix;
}

fs::path GetResourceRoot()
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path suffix = fs::path("resources");
    if (auto found = FindExistingPath(exeBase, suffix))
        return *found;
    if (auto found = FindExistingPath(fs::current_path(), suffix))
        return *found;
    return exeBase / suffix;
}

std::string GetLastProjectPathFile()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    if (dir.empty())
        return {};
    fs::path p = fs::path(dir.ToStdString());
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return {};
    p /= "last_project.txt";
    return p.string();
}

bool SaveLastProjectPath(const std::string& path)
{
    const std::string pathFile = GetLastProjectPathFile();
    if (pathFile.empty())
        return false;
    std::ofstream out(pathFile);
    if (!out.is_open())
        return false;
    if (path.empty()) {
        out << path;
        return true;
    }
    std::error_code ec;
    fs::path resolved = fs::u8path(path);
    resolved = fs::absolute(resolved, ec);
    if (ec)
        out << path;
    else
        out << resolved.u8string();
    return true;
}

std::optional<std::string> LoadLastProjectPath()
{
    const std::string pathFile = GetLastProjectPathFile();
    if (pathFile.empty())
        return std::nullopt;
    std::ifstream in(pathFile);
    if (!in.is_open())
        return std::nullopt;
    std::string rawPath;
    std::getline(in, rawPath);
    if (rawPath.empty())
        return std::nullopt;
    fs::path candidate = fs::u8path(rawPath);
    if (candidate.is_absolute())
        return candidate.u8string();
    std::error_code ec;
    fs::path currentCandidate = fs::absolute(candidate, ec);
    if (!ec && fs::exists(currentCandidate, ec))
        return currentCandidate.u8string();
    ec.clear();
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path exeCandidate = exeBase / candidate;
    if (fs::exists(exeCandidate, ec))
        return fs::absolute(exeCandidate, ec).u8string();
    return candidate.u8string();
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
