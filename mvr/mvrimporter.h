#pragma once

#include <string>

// Responsible for importing .mvr files into the application's internal data model
class MvrImporter
{
public:
    // Imports and parses a .mvr file and stores the data into ConfigManager
    bool ImportFromFile(const std::string& filePath);

    // Static interface for use outside the import module (e.g. GUI)
    static bool ImportAndRegister(const std::string& filePath);

private:
    // Creates a temporary directory for extracting the contents of the MVR archive
    std::string CreateTemporaryDirectory();

    // Extracts the .mvr (ZIP) contents into the given destination directory
    bool ExtractMvrZip(const std::string& mvrPath, const std::string& destDir);

    // Parses the GeneralSceneDescription.xml file and updates the scene model
    bool ParseSceneXml(const std::string& sceneXmlPath);
};
