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
#include <optional>
#include <string>
#include <unordered_map>

namespace GdtfDictionary {
    enum class LoadOutcome {
        LoadedActiveDictionary,
        ActiveDictionaryMissing,
        ActiveDictionaryInvalid,
        TemporaryFallbackUsed,
        ManagedDefaultRecreated
    };

    struct LoadStatus {
        LoadOutcome outcome = LoadOutcome::LoadedActiveDictionary;
        bool loadedActiveDictionary = false;
        bool activeDictionaryMissing = false;
        bool activeDictionaryInvalid = false;
        bool temporaryFallbackUsed = false;
        bool managedDefaultRecreated = false;
        bool usedDefaultDictionary = false;
        std::string activePath;
        std::string fallbackPath;
        std::string error;
    };

    struct Entry {
        std::string path; // optional absolute path inside fixtures library
        std::string mode;
        std::string category;
        std::string visualColorHex;
        std::string importedAt; // optional UTC ISO-8601 timestamp
        std::string sha256; // optional file hash for diagnostics
    };

    // Loads the dictionary file into a map of type -> {gdtf path in library, default mode}
    std::optional<std::unordered_map<std::string, Entry>> Load();
    LoadStatus GetLastLoadStatus();

    std::string GetActiveDictionaryFilePath();
    std::string GetActiveDictionaryFileName();
    bool ValidateDictionaryFile(const std::string &path,
                                std::string *errorOut = nullptr);
    bool CreateEmptyDictionaryFile(const std::string &path,
                                   std::string *errorOut = nullptr);
    bool CreateDictionaryFileFromDefaults(const std::string &path,
                                          std::string *errorOut = nullptr);
    bool SetActiveDictionaryFilePath(const std::string &path,
                                     std::string *errorOut = nullptr);
    // Saves the dictionary map back to disk.
    // Returns false and optionally fills errorOut when writing fails.
    bool Save(const std::unordered_map<std::string, Entry>& dict,
              std::string* errorOut = nullptr);
    // Returns the stored entry for a given type when the referenced file exists.
    // Missing referenced files return std::nullopt without mutating the dictionary.
    std::optional<Entry> Get(const std::string& type);
    // Normalizes a fixture alias with the same rules used by dictionary lookup.
    std::string NormalizeTypeKey(const std::string& type);
    // Looks up a type in an already loaded dictionary without reloading from disk.
    std::optional<Entry> FindInLoadedDictionary(
        const std::unordered_map<std::string, Entry>& dict,
        const std::string& type, bool validateExistingPath = true);
    // Returns the default color for a fixture when present in dictionary.
    // Lookup priority:
    // 1) Explicit fixture type entry.
    // 2) Any entry sharing the same GDTF file + mode fixture family.
    std::optional<std::string> GetDefaultVisualColorForFixture(
        const std::string& type, const std::string& gdtfPath,
        const std::string& mode);
    // Copies a user-selected GDTF into the fixtures library and updates the dictionary.
    void Update(const std::string& type, const std::string& gdtfPath, const std::string& mode = {}, const std::string& category = {});
    // Builds the stable @Perastage derivative filename for a GDTF path.
    std::string BuildPerastageCanonicalGdtfFileName(const std::string& gdtfPath);
    // Builds the stable @Perastage derivative filename from explicit identity values.
    std::string BuildPerastageCanonicalGdtfFileName(
        const std::string& manufacturer, const std::string& model,
        const std::string& fallbackStem = "");
    // Returns true when a GDTF filename already uses Perastage derivative naming.
    bool IsPerastageNamedGdtfFile(const std::string& gdtfPath);
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
    void UpdateVisualColor(const std::string& type, const std::string& color);
    void UpdateVisualColorForFile(const std::string& type, const std::string& gdtfPath,
                            const std::string& mode,
                            const std::string& color);
    DictionaryImportSummary PreviewImportFromFile(
        const std::string &filePath, DictionaryImportPolicy policy);
    DictionaryImportSummary ApplyImportFromFile(
        const std::string &filePath, DictionaryImportPolicy policy);

    size_t GetSaveCallCountForTesting();
    void ResetSaveCallCountForTesting();
}
