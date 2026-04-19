#pragma once

#include <optional>
#include <string>

struct GdtfCatalogCacheEntry {
  std::string listData;
  std::string updatedAt;
  bool compressed = false;
};

std::optional<GdtfCatalogCacheEntry> LoadGdtfCatalogCache();
bool SaveGdtfCatalogCache(const GdtfCatalogCacheEntry &entry);
