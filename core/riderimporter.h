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

#include <functional>
#include <string>
#include <vector>

// Parses simple rider files (.txt/.pdf) to create dummy fixtures and trusses
class RiderImporter {
public:
    struct FixtureTypeRequest {
        std::string typeName;
        std::string normalizedTypeName;
        int quantity = 0;
        std::vector<std::string> positions;
    };
    struct ProgressState {
        std::string stage;
        int completed = 0;
        int total = 0;

        bool HasCount() const { return total > 0; }
    };

    using ProgressCallback = std::function<void(const ProgressState&)>;

    // Import rider located at path. Returns true on success.
    static bool Import(const std::string& path,
                       ProgressCallback progressCallback = {});
    // Load rider file into text. Returns empty string on failure.
    static std::string LoadText(const std::string& path);
    // Build a filtered text preview that keeps only fixture lines that would be
    // considered during text-to-scene import.
    static std::string BuildFixtureFilterPreview(const std::string& text);
    // Analyzes unique fixture requests without changing scene or dictionary state.
    static std::vector<FixtureTypeRequest>
    AnalyzeFixtureTypes(const std::string& text);
    // Import from raw rider text. Returns true on success.
    static bool ImportText(const std::string& text,
                           ProgressCallback progressCallback = {},
                           bool skipFixtureFilterPreview = false);
};
