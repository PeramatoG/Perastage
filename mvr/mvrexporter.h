#pragma once

#include <string>

// Exports the current scene in ConfigManager into a standards compliant MVR archive
class MvrExporter
{
public:
    // Serialize the scene and write a .mvr archive at the given path
    bool ExportToFile(const std::string& filePath);
};
