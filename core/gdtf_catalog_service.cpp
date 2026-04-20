#include "gdtf_catalog_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <wx/datetime.h>
#include <wx/stdpaths.h>

#include "json.hpp"

namespace {

namespace fs = std::filesystem;

fs::path GetCatalogCachePath() {
  const wxString userDataDir = wxStandardPaths::Get().GetUserDataDir();
  return fs::path(userDataDir.ToStdString()) / "gdtf_catalog_cache.json";
}

bool ParseIsoDateTimeUtc(const std::string &text, wxDateTime &out) {
  if (text.empty())
    return false;

  wxDateTime parsed;
  if (parsed.ParseISOCombined(text.c_str(), ' ')) {
    out = parsed;
    return out.IsValid();
  }

  if (parsed.ParseISOCombined(text.c_str(), 'T')) {
    out = parsed;
    return out.IsValid();
  }

  return false;
}

std::optional<long long> ComputeAgeSeconds(const std::string &updatedAt,
                                           const std::string &nowUtcIso) {
  wxDateTime updatedAtDt;
  wxDateTime nowDt;
  if (!ParseIsoDateTimeUtc(updatedAt, updatedAtDt) ||
      !ParseIsoDateTimeUtc(nowUtcIso, nowDt)) {
    return std::nullopt;
  }

  const wxTimeSpan ageSpan = nowDt - updatedAtDt;
  const wxLongLong ageMillis = ageSpan.GetMilliseconds();
  if (ageMillis.GetValue() < 0)
    return 0;

  return static_cast<long long>(ageMillis.GetValue() / 1000);
}

std::optional<GdtfCatalogSnapshot> LoadCacheSnapshot() {
  const fs::path cachePath = GetCatalogCachePath();
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

  GdtfCatalogSnapshot snapshot;
  if (root.contains("list_data") && root["list_data"].is_string())
    snapshot.listData = root["list_data"].get<std::string>();
  if (root.contains("updated_at") && root["updated_at"].is_string())
    snapshot.updatedAt = root["updated_at"].get<std::string>();
  if (root.contains("last_successful_refresh_at") &&
      root["last_successful_refresh_at"].is_string()) {
    snapshot.lastSuccessfulRefreshAt =
        root["last_successful_refresh_at"].get<std::string>();
  }

  if (snapshot.lastSuccessfulRefreshAt.empty())
    snapshot.lastSuccessfulRefreshAt = snapshot.updatedAt;

  if (snapshot.listData.empty())
    return std::nullopt;

  return snapshot;
}

bool SaveCacheSnapshot(const GdtfCatalogSnapshot &snapshot) {
  const fs::path cachePath = GetCatalogCachePath();
  std::error_code mkdirError;
  fs::create_directories(cachePath.parent_path(), mkdirError);

  nlohmann::json root;
  root["version"] = 2;
  root["updated_at"] = snapshot.updatedAt;
  root["last_successful_refresh_at"] = snapshot.lastSuccessfulRefreshAt;
  root["list_data"] = snapshot.listData;

  std::ofstream output(cachePath, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;

  output << root.dump();
  return output.good();
}

} // namespace

std::optional<GdtfCatalogSnapshot> GdtfCatalogService::GetCatalogSnapshot() const {
  return LoadCacheSnapshot();
}

GdtfCatalogRefreshResult GdtfCatalogService::RefreshCatalogIfStale(
    const RefreshCatalogFn &refreshCatalogFn, const std::string &nowUtcIso,
    long long refreshThresholdSeconds) const {
  GdtfCatalogRefreshResult result;
  result.snapshot = LoadCacheSnapshot();
  result.metrics.cacheHit = result.snapshot.has_value();
  result.metrics.cacheMiss = !result.metrics.cacheHit;

  if (result.snapshot) {
    const std::optional<long long> ageSeconds =
        ComputeAgeSeconds(result.snapshot->updatedAt, nowUtcIso);
    if (ageSeconds)
      result.metrics.cacheAgeSeconds = *ageSeconds;
  }

  const bool hasFreshCache =
      result.snapshot && result.metrics.cacheAgeSeconds >= 0 &&
      result.metrics.cacheAgeSeconds < refreshThresholdSeconds;
  if (hasFreshCache)
    return result;

  result.metrics.refreshAttempted = true;

  std::string refreshedListData;
  if (!refreshCatalogFn || !refreshCatalogFn(refreshedListData) ||
      refreshedListData.empty()) {
    return result;
  }

  GdtfCatalogSnapshot refreshedSnapshot;
  refreshedSnapshot.listData = refreshedListData;
  refreshedSnapshot.updatedAt = nowUtcIso;
  refreshedSnapshot.lastSuccessfulRefreshAt = nowUtcIso;

  SaveCacheSnapshot(refreshedSnapshot);
  result.snapshot = refreshedSnapshot;

  result.metrics.refreshSucceeded = true;
  result.metrics.cacheHit = result.snapshot.has_value();
  result.metrics.cacheMiss = !result.metrics.cacheHit;
  result.metrics.cacheAgeSeconds = 0;

  return result;
}
