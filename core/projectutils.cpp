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
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace ProjectUtils {

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
    // Prefer the library alongside the executable, regardless of where the
    // project lives on disk.
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path exeBase = fs::path(exe.GetPath().ToStdString());
    fs::path exeLib = exeBase / "library" / subdir;
    if (fs::exists(exeLib))
        return exeLib.string();

    // When running from the build tree, the current working directory may
    // still contain the checked-out resources. Fall back to that before
    // creating any folders.
    fs::path cwdLib = fs::current_path() / "library" / subdir;
    if (fs::exists(cwdLib))
        return cwdLib.string();

    // As a last resort, create the directory next to the executable so
    // callers still receive a usable absolute path.
    std::error_code ec;
    fs::create_directories(exeLib, ec);
    if (!ec)
        return exeLib.string();

    return {};
}

} // namespace ProjectUtils

