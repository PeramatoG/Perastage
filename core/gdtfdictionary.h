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

#include "dictionary_import.h"

#include <cstddef>
#include <string>
#include <optional>
#include <unordered_map>

namespace GdtfDictionary {
    struct LoadStatus {
        bool usedDefaultDictionary = false;
        std::string error;
    };

    struct Entry {
        std::string path; // optional absolute path inside fixtures library
        std::string mode;
        std::string category;
        std::string color;
        std::string importedAt; // optional UTC ISO-8601 timestamp
        std::string sha256; // optional file hash for diagnostics
    };

    // Loads the dictionary file into a map of type -> {gdtf path in library, default mode}
    std::optional<std::unordered_map<std::string, Entry>> Load();
    LoadStatus GetLastLoadStatus();

    std::string GetActiveDictionaryFilePath();
    std::string GetActiveDictionaryFileName();
    bool SetActiveDictionaryFilePath(const std::string &path,
                                     std::string *errorOut = nullptr);
    // Saves the dictionary map back to disk.
    // Returns false and optionally fills errorOut when writing fails.
    bool Save(const std::unordered_map<std::string, Entry>& dict,
              std::string* errorOut = nullptr);
    // Returns the stored entry for a given type if it exists and file exists.
    // If the file is missing, the entry is removed and std::nullopt returned.
    std::optional<Entry> Get(const std::string& type);
    // Looks up a type in an already loaded dictionary without reloading from disk.
    std::optional<Entry> FindInLoadedDictionary(
        const std::unordered_map<std::string, Entry>& dict,
        const std::string& type, bool validateExistingPath = true);
    // Returns the default color for a fixture when present in dictionary.
    // Lookup priority:
    // 1) Explicit fixture type entry.
    // 2) Any entry sharing the same GDTF file + mode fixture family.
    std::optional<std::string> GetDefaultColorForFixture(
        const std::string& type, const std::string& gdtfPath,
        const std::string& mode);
    // Copies a user-selected GDTF into the fixtures library and updates the dictionary.
    void Update(const std::string& type, const std::string& gdtfPath, const std::string& mode = {}, const std::string& category = {});
    // Creates or overwrites the stable @Perastage derivative for a library fixture and updates the dictionary.
    std::optional<Entry> CreateOrUpdatePerastageLibraryDerivative(
        const std::string& type, const std::string& gdtfPath,
        const std::string& mode = {}, const std::string& category = {});
    // Updates dictionary metadata without copying fixture files into the library.
    void UpdateDictionaryEntry(const std::string& type, const Entry& entry);
    void UpdateCategory(const std::string& type, const std::string& category);
    void UpdateCategoryForFile(const std::string& type, const std::string& gdtfPath,
                               const std::string& category);
    void UpdateCategoriesBulk(
        const std::unordered_map<std::string, std::string>& categoriesByType);
    void UpdateColor(const std::string& type, const std::string& color);
    void UpdateColorForFile(const std::string& type, const std::string& gdtfPath,
                            const std::string& mode,
                            const std::string& color);
    DictionaryImportSummary PreviewImportFromFile(
        const std::string &filePath, DictionaryImportPolicy policy);
    DictionaryImportSummary ApplyImportFromFile(
        const std::string &filePath, DictionaryImportPolicy policy);

    size_t GetSaveCallCountForTesting();
    void ResetSaveCallCountForTesting();
}
