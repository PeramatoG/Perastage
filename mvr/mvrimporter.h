#pragma once

#include <string>

// Responsible for importing .mvr files into the application's internal data model
class MvrImporter
{
public:
    // Imports and parses a .mvr file and stores the data into ConfigManager
    // Set promptConflicts=false to skip showing the dictionary conflict dialog
    bool ImportFromFile(const std::string& filePath, bool promptConflicts = true);

    // Static interface for use outside the import module (e.g. GUI)
    // Allows the caller to decide whether dictionary conflicts should prompt
    static bool ImportAndRegister(const std::string& filePath,
                                  bool promptConflicts = true);

private:
    // Creates a temporary directory for extracting the contents of the MVR archive
    std::string CreateTemporaryDirectory();

    // Extracts the .mvr (ZIP) contents into the given destination directory
    bool ExtractMvrZip(const std::string& mvrPath, const std::string& destDir);

    // Parses the GeneralSceneDescription.xml file and updates the scene model
    // When promptConflicts is true, the user is asked to resolve GDTF conflicts
    bool ParseSceneXml(const std::string& sceneXmlPath, bool promptConflicts);
};
