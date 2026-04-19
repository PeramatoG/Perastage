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
#include "gdtf_catalog_cache.h"

#include <filesystem>
#include <fstream>

#include "json.hpp"
#include "projectutils.h"

using json = nlohmann::json;

namespace {

std::filesystem::path GetCacheFilePath() {
  const std::string fixturesPath = ProjectUtils::GetWritableLibraryPath("fixtures");
  if (fixturesPath.empty())
    return {};

  const std::filesystem::path fixturesDir = std::filesystem::u8path(fixturesPath);
  return fixturesDir.parent_path() / "gdtf_catalog_cache.json";
}

} // namespace

std::optional<GdtfCatalogCacheData> LoadGdtfCatalogCache() {
  const std::filesystem::path cachePath = GetCacheFilePath();
  if (cachePath.empty())
    return std::nullopt;

  std::ifstream in(cachePath);
  if (!in.is_open())
    return std::nullopt;

  json j = json::parse(in, nullptr, false);
  if (j.is_discarded() || !j.is_object())
    return std::nullopt;

  GdtfCatalogCacheData cache;
  cache.listData = j.value("list_data", std::string());
  cache.lastUpdate = j.value("last_update", std::string());
  cache.source = j.value("source", std::string());
  cache.version = j.value("version", std::string());
  return cache;
}

bool SaveGdtfCatalogCache(const GdtfCatalogCacheData &cache) {
  const std::filesystem::path cachePath = GetCacheFilePath();
  if (cachePath.empty())
    return false;

  std::error_code ec;
  std::filesystem::create_directories(cachePath.parent_path(), ec);
  if (ec)
    return false;

  json j;
  j["list_data"] = cache.listData;
  j["last_update"] = cache.lastUpdate;
  j["source"] = cache.source;
  j["version"] = cache.version;

  std::ofstream out(cachePath, std::ios::trunc);
  if (!out.is_open())
    return false;

  out << j.dump(2);
  return true;
}
