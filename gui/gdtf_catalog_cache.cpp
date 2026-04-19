#include "gdtf_catalog_cache.h"

#include <filesystem>
#include <fstream>

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "json.hpp"

namespace {

namespace fs = std::filesystem;

fs::path GetCachePath() {
  const wxString userDataDir = wxStandardPaths::Get().GetUserDataDir();
  const fs::path rootPath = userDataDir.ToStdString();
  return rootPath / "gdtf_catalog_cache.json";
}

} // namespace

std::optional<GdtfCatalogCacheEntry> LoadGdtfCatalogCache() {
  const fs::path cachePath = GetCachePath();
  if (!fs::exists(cachePath))
    return std::nullopt;

  std::ifstream input(cachePath, std::ios::binary);
  if (!input)
    return std::nullopt;

  std::string payload((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
  if (payload.empty())
    return std::nullopt;

  nlohmann::json root = nlohmann::json::parse(payload, nullptr, false);
  if (root.is_discarded() || !root.is_object())
    return std::nullopt;

  GdtfCatalogCacheEntry entry;
  if (root.contains("list_data") && root["list_data"].is_string())
    entry.listData = root["list_data"].get<std::string>();
  if (root.contains("updated_at") && root["updated_at"].is_string())
    entry.updatedAt = root["updated_at"].get<std::string>();
  if (root.contains("compressed") && root["compressed"].is_boolean())
    entry.compressed = root["compressed"].get<bool>();

  if (entry.listData.empty())
    return std::nullopt;

  return entry;
}

bool SaveGdtfCatalogCache(const GdtfCatalogCacheEntry &entry) {
  const fs::path cachePath = GetCachePath();
  std::error_code mkdirError;
  fs::create_directories(cachePath.parent_path(), mkdirError);

  nlohmann::json root;
  root["version"] = 1;
  root["updated_at"] = entry.updatedAt;
  root["compressed"] = entry.compressed;
  root["list_data"] = entry.listData;

  std::ofstream output(cachePath, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;

  output << root.dump();
  return output.good();
}
