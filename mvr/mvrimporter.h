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

#include <string>

// Responsible for importing .mvr files into the application's internal data model
class MvrImporter
{
public:
    // Imports and parses a .mvr file and stores the data into ConfigManager
    // Set promptConflicts=false to skip showing the dictionary conflict dialog
    // Set applyDictionary=true to resolve GDTF conflicts using the dictionary
    bool ImportFromFile(const std::string& filePath,
                        bool promptConflicts = true,
                        bool applyDictionary = true);

    // Static interface for use outside the import module (e.g. GUI)
    // Allows the caller to decide whether dictionary conflicts should prompt
    // or whether the dictionary should be applied at all
    static bool ImportAndRegister(const std::string& filePath,
                                  bool promptConflicts = true,
                                  bool applyDictionary = true);

private:
    // Creates a temporary directory for extracting the contents of the MVR archive
    std::string CreateTemporaryDirectory();

    // Extracts the .mvr (ZIP) contents into the given destination directory
    bool ExtractMvrZip(const std::string& mvrPath, const std::string& destDir);

    // Parses the GeneralSceneDescription.xml file and updates the scene model
    // When promptConflicts is true, the user is asked to resolve GDTF conflicts
    // If applyDictionary is false, existing GDTF assignments are kept intact
    bool ParseSceneXml(const std::string& sceneXmlPath, bool promptConflicts,
                       bool applyDictionary);
};
