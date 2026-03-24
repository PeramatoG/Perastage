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
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace fs = std::filesystem;

namespace ProjectUtils {

namespace {

std::string ToUtf8String(const fs::path& path)
{
    std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}


fs::path WxStringToPath(const wxString& value)
{
    const wxScopedCharBuffer utf8 = value.ToUTF8();
    if (utf8)
        return fs::u8path(std::string(utf8.data(), utf8.length()));

    return fs::path(value.ToStdString());
}

void CopyLibrarySubdir(const fs::path& source, const fs::path& destination)
{
    std::error_code ec;
    bool sourceExists = fs::exists(source, ec);
    if (ec || !sourceExists)
        return;

    bool destinationExists = fs::exists(destination, ec);
    if (ec || !ProjectUtils::IsDirectoryWritable(destination))
        return;

    ec.clear();
    bool destinationEmpty = destinationExists ? fs::is_empty(destination, ec) : true;
    if (ec)
        return;

    if (destinationEmpty || !destinationExists) {
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

std::optional<fs::path> ResolveWritableUserDataDir()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    fs::path candidate;
    if (!dir.empty())
        candidate = WxStringToPath(dir);

    std::error_code ec;
    if (!candidate.empty()) {
        fs::create_directories(candidate, ec);
        if (!ec)
            return fs::absolute(candidate, ec);
    }

    ec.clear();
    wxString tempDir = wxStandardPaths::Get().GetTempDir();
    fs::path fallback = WxStringToPath(tempDir) / "Perastage";
    fs::create_directories(fallback, ec);
    if (ec)
        return std::nullopt;
    return fs::absolute(fallback, ec);
}

} // namespace

bool IsDirectoryWritable(const fs::path& dir)
{
    if (dir.empty())
        return false;

    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
        return false;

    const fs::path probePath = dir / ".perastage_write_test.tmp";
    {
        std::ofstream probe(probePath, std::ios::out | std::ios::trunc);
        if (!probe.is_open())
            return false;
    }

    fs::remove(probePath, ec);
    return true;
}

fs::path GetBaseLibraryPath(const std::string& subdir)
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = WxStringToPath(exe.GetPath());
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
    fs::path exeBase = WxStringToPath(exe.GetPath());
    fs::path suffix = fs::path("resources");
    if (auto found = FindExistingPath(exeBase, suffix))
        return *found;
    if (auto found = FindExistingPath(fs::current_path(), suffix))
        return *found;
    const wxString resourcesDir = wxStandardPaths::Get().GetResourcesDir();
    if (!resourcesDir.empty()) {
        fs::path resourcesPath = WxStringToPath(resourcesDir);
        std::error_code ec;
        if (fs::exists(resourcesPath, ec))
            return resourcesPath;
    }
    return {};
}

std::string GetLastProjectPathFile()
{
    const auto dataDir = ResolveWritableUserDataDir();
    if (!dataDir)
        return {};
    fs::path p = *dataDir;
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
    fs::path resolved = fs::absolute(fs::u8path(path), ec);
    if (ec)
        out << path;
    else
        out << ToUtf8String(resolved);
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
    fs::path candidate;
    try {
        candidate = fs::u8path(rawPath);
    } catch (...) {
        return std::nullopt;
    }
    auto isValidFile = [](const fs::path& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_regular_file(path, ec);
    };
    if (candidate.is_absolute()) {
        if (isValidFile(candidate))
            return ToUtf8String(candidate);
        return std::nullopt;
    }
    std::error_code ec;
    fs::path currentCandidate = fs::absolute(candidate, ec);
    if (!ec && isValidFile(currentCandidate))
        return ToUtf8String(currentCandidate);
    ec.clear();
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = WxStringToPath(exe.GetPath());
    fs::path exeCandidate = fs::absolute(exeBase / candidate, ec);
    if (!ec && isValidFile(exeCandidate))
        return ToUtf8String(exeCandidate);
    return std::nullopt;
}

std::string GetDefaultLibraryPath(const std::string& subdir)
{
    auto absoluteUtf8 = [](const fs::path& path) -> std::string {
        std::error_code ec;
        fs::path absolutePath = fs::absolute(path, ec);
        if (ec)
            return {};
        return ToUtf8String(absolutePath);
    };

    if (const char* envPath = std::getenv("PERASTAGE_LIBRARY_PATH")) {
        if (*envPath != '\0') {
            fs::path envRoot = fs::u8path(envPath);
            std::error_code ec;
            const bool envRootExists = fs::exists(envRoot, ec);
            const bool envRootIsDir = !ec && fs::is_directory(envRoot, ec);
            if (!ec && envRootExists && envRootIsDir) {
                fs::path envLibraryPath = envRoot / subdir;
                if (IsDirectoryWritable(envLibraryPath))
                    return absoluteUtf8(envLibraryPath);
            }
        }
    }

    fs::path baseLib = GetBaseLibraryPath(subdir);
    std::error_code ec;
    const bool baseExists = fs::exists(baseLib, ec);
    const bool baseIsDir = !ec && fs::is_directory(baseLib, ec);
    const bool baseWritable = baseExists && baseIsDir && IsDirectoryWritable(baseLib);
    if (!ec && baseWritable)
        return absoluteUtf8(baseLib);

    if (const auto dataDir = ResolveWritableUserDataDir()) {
        fs::path userLib = *dataDir / "library" / subdir;
        if (IsDirectoryWritable(userLib)) {
            CopyLibrarySubdir(baseLib, userLib);
            return absoluteUtf8(userLib);
        }
    }

    std::cerr << "ProjectUtils::GetDefaultLibraryPath failed for subdir '" << subdir
              << "'. Checked PERASTAGE_LIBRARY_PATH, writable base library path, and "
                 "user data library fallback."
              << std::endl;
    return {};
}

} // namespace ProjectUtils
