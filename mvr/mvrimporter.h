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
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class wxInputStream;

#include "mvrscene.h"

enum class MvrImportMode {
    ReplaceProject,
    ParseOnly,
};

enum class MvrImportSourceKind {
    ExternalImport,
    ProjectRestore,
    MergeImport,
};

struct MvrImportOptions {
    bool promptConflicts = true;
    bool applyDictionary = true;
    bool preserveMvrGdtfReferences = true;
    bool allowDummyFallback = true;
    MvrImportSourceKind sourceKind = MvrImportSourceKind::ExternalImport;
};

struct MvrImportDiagnostic {
    std::string code;
    std::string message;
};

struct MvrImportResult {
    MvrScene scene;
    std::unordered_map<std::string, std::string> fixtureUuidRemap;
    std::vector<MvrImportDiagnostic> diagnostics;
};

// Responsible for importing .mvr files into the application's internal data model
class MvrImporter
{
public:
    struct ProgressState {
        std::string stage;
        int completed = 0;
        int total = 0;

        bool HasCount() const { return total > 0; }
    };

    // Imports and parses a .mvr file and stores the data into ConfigManager
    // Set promptConflicts=false to skip showing the dictionary conflict dialog
    // Set applyDictionary=true to resolve GDTF conflicts using the dictionary
    using ProgressCallback = std::function<void(const ProgressState& state)>;

    bool ImportFromFile(const std::string& filePath,
                        bool promptConflicts = true,
                        bool applyDictionary = true,
                        ProgressCallback progressCallback = {});

    // Imports owned MVR archive bytes through the authoritative extraction path.
    bool ImportFromBuffer(const std::vector<std::uint8_t>& bytes,
                          MvrImportResult& importResult,
                          MvrImportMode mode,
                          const MvrImportOptions& options,
                          ProgressCallback progressCallback = {});

    // Imports and parses a .mvr file into an import result using the requested import mode.
    bool ImportFromFile(const std::string& filePath,
                        MvrImportResult& importResult,
                        MvrImportMode mode = MvrImportMode::ReplaceProject,
                        bool promptConflicts = true,
                        bool applyDictionary = true,
                        ProgressCallback progressCallback = {});

    // Imports and parses a .mvr file into an import result using explicit restore/import options.
    bool ImportFromFile(const std::string& filePath,
                        MvrImportResult& importResult,
                        MvrImportMode mode,
                        const MvrImportOptions& options,
                        ProgressCallback progressCallback = {});

    // Imports and parses a .mvr file into the provided scene without resetting ConfigManager.
    bool ImportSceneFromFile(const std::string& filePath,
                             MvrScene& targetScene,
                             bool promptConflicts = true,
                             bool applyDictionary = true,
                             ProgressCallback progressCallback = {});

    // Imports and parses a .mvr file into the provided scene using explicit restore/import options.
    bool ImportSceneFromFile(const std::string& filePath,
                             MvrScene& targetScene,
                             const MvrImportOptions& options,
                             ProgressCallback progressCallback = {});

    // Static interface for use outside the import module (e.g. GUI)
    // Allows the caller to decide whether dictionary conflicts should prompt
    // or whether the dictionary should be applied at all
    static bool ImportAndRegister(const std::string& filePath,
                                  bool promptConflicts = true,
                                  bool applyDictionary = true,
                                  ProgressCallback progressCallback = {});

    // Static interface for project restore imports that must preserve serialized MVR resources.
    static bool ImportAndRegister(const std::string& filePath,
                                  const MvrImportOptions& options,
                                  ProgressCallback progressCallback = {});

    // Imports and registers owned MVR archive bytes for project restore.
    static bool ImportAndRegisterFromBuffer(
        const std::vector<std::uint8_t>& bytes,
        const MvrImportOptions& options,
        ProgressCallback progressCallback = {});

private:
    std::unordered_map<std::string, std::string> pathRemap;
    std::unordered_map<std::string, std::string> fixtureUuidRemap;

    // Creates a temporary directory for extracting the contents of the MVR archive

    // Extracts the .mvr (ZIP) contents into the given destination directory
    bool ExtractMvrZip(const std::string& mvrPath, const std::string& destDir,
                       std::vector<MvrImportDiagnostic>& diagnostics);
    bool ExtractMvrZip(wxInputStream& input, const std::string& destDir,
                       std::vector<MvrImportDiagnostic>& diagnostics);

    bool ImportFromStreamIntoResult(wxInputStream& input,
                                    MvrImportResult& importResult,
                                    MvrImportMode mode,
                                    const MvrImportOptions& options,
                                    ProgressCallback progressCallback);

    // Extracts and parses a .mvr file into an import result payload.
    bool ImportFromFileIntoResult(const std::string& filePath,
                                  MvrImportResult& importResult,
                                  MvrImportMode mode,
                                  const MvrImportOptions& options,
                                  ProgressCallback progressCallback);

    // Parses the GeneralSceneDescription.xml file and updates the import result payload.
    bool ParseSceneXml(const std::string& sceneXmlPath,
                       MvrImportResult& importResult,
                       const MvrImportOptions& options,
                       ProgressCallback progressCallback);

    std::string NormalizeArchivePath(const std::string& archivePath) const;
    std::string RemapArchivePathIfNeeded(const std::string& archivePath) const;
};
