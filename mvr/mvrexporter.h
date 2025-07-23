#pragma once

#include <string>

// Simple exporter that packages the current scene directory into an MVR (ZIP) file
class MvrExporter
{
public:
    // Export ConfigManager::Get().GetScene() into a .mvr archive at the given path
    bool ExportToFile(const std::string& filePath);
};
