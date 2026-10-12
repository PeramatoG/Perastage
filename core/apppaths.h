#pragma once

#include <filesystem>

namespace AppPaths {

// Returns the primary writable per-user application-data directory.
std::filesystem::path GetUserDataDir();

// Returns the per-user fallback application-data directory inside the system temp folder.
std::filesystem::path GetUserDataTempFallbackDir();

// Canonical regenerable application cache paths. No source assets belong here.
std::filesystem::path GetCacheRoot();
std::filesystem::path GetViewer3DGdtfCacheDir();
std::filesystem::path GetViewer3DMeshCacheDir();
std::filesystem::path GetGdtfCatalogCachePath();
std::filesystem::path GetCatalogCacheMigrationMarker();

// Returns the full path to the per-user user_config.json file.
std::filesystem::path GetUserConfigFilePath();

// Returns the full path to the per-user application log file.
std::filesystem::path GetLogFilePath();

// Ensures the directory exists and reports whether it is available for use.
bool EnsureDirectory(const std::filesystem::path &dir);

} // namespace AppPaths
