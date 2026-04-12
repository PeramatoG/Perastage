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
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace ProjectUtils {
    constexpr const char* PROJECT_EXTENSION = ".pstg";

    std::string GetLastProjectPathFile();
    bool SaveLastProjectPath(const std::string& path);
    std::optional<std::string> LoadLastProjectPath();

    // Path containing the built-in library shipped with the executable.
    std::filesystem::path GetBaseLibraryPath(const std::string& subdir);

    // Read-only library path shipped with the installation. Returns empty when unavailable.
    std::string GetInstalledLibraryPath(const std::string& subdir);

    // Writable library path used by editing flows.
    // Uses PERASTAGE_LIBRARY_PATH/<subdir> when writable, otherwise user-data/library/<subdir>.
    std::string GetWritableLibraryPath(const std::string& subdir);

    // Path containing the built-in resources shipped with the executable.
    std::filesystem::path GetResourceRoot();

    // Returns true when a directory exists (or can be created) and accepts writes.
    bool IsDirectoryWritable(const std::filesystem::path& dir);

    // Legacy default path resolver kept for compatibility with existing call sites.
    std::string GetDefaultLibraryPath(const std::string& subdir);
}
