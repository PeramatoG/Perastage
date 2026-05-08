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
#include "library/library_bootstrap.h"
#include "logger.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace fs = std::filesystem;

namespace ProjectUtils {

namespace {

// Converts a filesystem path to a UTF-8 encoded std::string.
std::string ToUtf8String(const fs::path& path)
{
    std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

// Converts a wxString value into a filesystem path preserving UTF-8 characters.
fs::path WxStringToPath(const wxString& value)
{
    const wxScopedCharBuffer utf8 = value.ToUTF8();
    if (utf8)
        return fs::u8path(std::string(utf8.data(), utf8.length()));

    return fs::path(value.ToStdString());
}

// Searches for an existing suffix path while walking parent directories up to maxDepth.
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

// Returns a candidate directory if it exists and is a directory.
std::optional<fs::path> ExistingDirectory(const fs::path& candidate)
{
    std::error_code ec;
    if (fs::exists(candidate, ec) && !ec && fs::is_directory(candidate, ec) && !ec)
        return candidate;
    return std::nullopt;
}

// Resolves a writable user data directory with a temp-directory fallback.
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

// Returns an absolute UTF-8 path string when path canonicalization succeeds.
std::string ToAbsoluteUtf8(const fs::path& path)
{
    std::error_code ec;
    fs::path absolutePath = fs::absolute(path, ec);
    if (ec)
        return {};
    return ToUtf8String(absolutePath);
}

// Returns the discovered root folder for installed library content.
fs::path GetBaseLibraryRoot()
{
    return GetBaseLibraryPath("");
}

// Logs a summary of library bootstrap migration results and collected errors.
void LogBootstrapSummary(const LibraryBootstrap::BootstrapResult& result,
                         const fs::path& installedRoot,
                         const fs::path& userDataDir)
{
    std::ostringstream summary;
    summary << "Library bootstrap migration (installed='" << installedRoot.string()
            << "', userData='" << userDataDir.string()
            << "') completed=" << (result.completed ? "true" : "false")
            << ", copied=" << result.filesCopied
            << ", skipped_existing=" << result.filesSkippedExisting
            << ", dirs_created=" << result.directoriesCreated
            << ", errors=" << result.errors.size();
    Logger::Instance().Log(Logger::Level::Info, summary.str());

    for (const std::string& error : result.errors) {
        Logger::Instance().Log(Logger::Level::Error, "Library bootstrap migration error: " + error);
    }
}

} // namespace

// Checks whether a directory can be created and written to by creating a probe file.
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

// Resolves the base library directory by searching executable, working, and platform resource locations.
fs::path GetBaseLibraryPath(const std::string& subdir)
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = WxStringToPath(exe.GetPath());
    fs::path suffix = fs::path("library") / subdir;
    if (auto found = FindExistingPath(exeBase, suffix))
        return *found;
    if (auto found = FindExistingPath(fs::current_path(), suffix))
        return *found;
    const wxString resourcesDir = wxStandardPaths::Get().GetResourcesDir();
    if (!resourcesDir.empty()) {
        fs::path resourcesPath = WxStringToPath(resourcesDir);
        const fs::path bundleResources = exeBase.parent_path() / "Resources";
        const std::array<fs::path, 3> candidates = {
            resourcesPath / "library" / subdir,
            bundleResources / "library" / subdir,
            resourcesPath.parent_path() / "Resources" / "library" / subdir
        };
        for (const fs::path& candidate : candidates) {
            if (auto existing = ExistingDirectory(candidate))
                return *existing;
        }
    }
    return exeBase / suffix;
}

// Resolves the runtime resources root by checking executable-relative paths and platform bundle locations.
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
        const fs::path bundleResources = exeBase.parent_path() / "Resources";
        const std::array<fs::path, 5> candidates = {
            resourcesPath / "resources",
            resourcesPath,
            bundleResources / "resources",
            bundleResources,
            resourcesPath.parent_path() / "Resources"
        };
        for (const fs::path& candidate : candidates) {
            if (auto existing = ExistingDirectory(candidate))
                return *existing;
        }
    }
    return {};
}

// Returns the persistent file path used to store the last opened project path.
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

// Stores the last opened project path as an absolute UTF-8 path when possible.
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

// Loads the last opened project path and validates that the referenced file still exists.
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

// Returns the installed library subdirectory path when it exists on disk.
std::string GetInstalledLibraryPath(const std::string& subdir)
{
    const fs::path installedPath = GetBaseLibraryPath(subdir);
    std::error_code ec;
    if (fs::exists(installedPath, ec) && !ec && fs::is_directory(installedPath, ec) && !ec)
        return ToAbsoluteUtf8(installedPath);
    return {};
}

// Resolves a writable library subdirectory preferring environment override then user-data bootstrap.
std::string GetWritableLibraryPath(const std::string& subdir)
{
    if (const char* envPath = std::getenv("PERASTAGE_LIBRARY_PATH")) {
        if (*envPath != '\0') {
            fs::path envRoot = fs::u8path(envPath);
            std::error_code ec;
            const bool envRootExists = fs::exists(envRoot, ec);
            const bool envRootIsDir = !ec && fs::is_directory(envRoot, ec);
            if (!ec && envRootExists && envRootIsDir) {
                fs::path envLibraryPath = envRoot / subdir;
                if (IsDirectoryWritable(envLibraryPath))
                    return ToAbsoluteUtf8(envLibraryPath);
                std::cerr << "ProjectUtils::GetWritableLibraryPath could not use "
                             "PERASTAGE_LIBRARY_PATH for subdir '"
                          << subdir << "' because path is not writable: "
                          << envLibraryPath.string() << std::endl;
            }
        }
    }

    if (const auto dataDir = ResolveWritableUserDataDir()) {
        LibraryBootstrap::BootstrapUserLibrary(GetBaseLibraryRoot(), *dataDir);
        fs::path userLib = *dataDir / "library" / subdir;
        if (IsDirectoryWritable(userLib)) {
            return ToAbsoluteUtf8(userLib);
        }
    }

    std::cerr << "ProjectUtils::GetWritableLibraryPath failed for subdir '" << subdir
              << "'. Checked PERASTAGE_LIBRARY_PATH and user-data library fallback."
              << std::endl;
    return {};
}

// Returns the best available library path using installed, writable, then executable-relative fallback.
std::string GetDefaultLibraryPath(const std::string& subdir)
{
    if (std::string installedPath = GetInstalledLibraryPath(subdir); !installedPath.empty()) {
        const fs::path installed = fs::u8path(installedPath);
        if (IsDirectoryWritable(installed))
            return installedPath;
    }

    if (std::string writablePath = GetWritableLibraryPath(subdir); !writablePath.empty())
        return writablePath;

    const fs::path fallbackPath = GetBaseLibraryPath(subdir);
    std::error_code ec;
    fs::path absolutePath = fs::absolute(fallbackPath, ec);
    if (ec) {
        std::cerr << "ProjectUtils::GetDefaultLibraryPath failed for subdir '" << subdir
                  << "'." << std::endl;
        return {};
    }
    return ToUtf8String(absolutePath);
}

// Runs startup migration to bootstrap user library content from installed assets.
void RunStartupLibraryBootstrap()
{
    const auto dataDir = ResolveWritableUserDataDir();
    if (!dataDir) {
        Logger::Instance().Log(Logger::Level::Warn,
                               "Library bootstrap migration skipped: no writable user-data directory.");
        return;
    }

    const fs::path installedRoot = GetBaseLibraryRoot();
    const LibraryBootstrap::BootstrapResult result =
        LibraryBootstrap::BootstrapUserLibrary(installedRoot, *dataDir);
    LogBootstrapSummary(result, installedRoot, *dataDir);
}

} // namespace ProjectUtils
