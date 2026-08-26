#include "gdtf_catalog_service.h"
#include "apppaths.h"
#include "logger.h"
#include "../mvr/gdtf_catalog_parser.h"

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
  fs::path dir = AppPaths::GetUserDataDir();
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    dir = AppPaths::GetUserDataTempFallbackDir();
  const fs::path target = dir / "gdtf_catalog_cache.json";
  const fs::path legacy =
      fs::path(wxStandardPaths::Get().GetUserDataDir().ToStdString()) /
      "gdtf_catalog_cache.json";
  if (target != legacy && !fs::exists(target, ec) && fs::exists(legacy, ec)) {
    ec.clear();
    fs::copy_file(legacy, target, fs::copy_options::skip_existing, ec);
    Logger::Instance().Log(
        ec ? Logger::Level::Warn : Logger::Level::Info,
        ec ? "Could not migrate GDTF catalog cache to local app data."
           : "Migrated GDTF catalog cache to local app data.");
  }
  return target;
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

// Loads the last validated catalog snapshot and optionally returns parsed data.
std::optional<GdtfCatalogSnapshot> LoadCacheSnapshot(
    mvr::gdtf_catalog_parser::GdtfCatalogParseResult *parsedOut = nullptr) {
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

  const auto parsed = mvr::gdtf_catalog_parser::ParseCatalog(snapshot.listData);
  if (!parsed.IsUsable()) {
    Logger::Instance().Log(
        Logger::Level::Warn,
        "Ignoring unusable GDTF catalog cache: bytes=" +
            std::to_string(parsed.payloadBytes) + " fingerprint=" +
            parsed.payloadFingerprint + " usable_entries=" +
            std::to_string(parsed.usableEntryCount));
    return std::nullopt;
  }
  if (parsedOut)
    *parsedOut = parsed;

  return snapshot;
}

fs::path MakeUniqueCacheSibling(const fs::path &cachePath,
                                const std::string &tag) {
  for (int attempt = 0; attempt < 64; ++attempt) {
    const fs::path candidate = cachePath.parent_path() /
        (cachePath.filename().string() + tag +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()) +
         "_" + std::to_string(attempt));
    std::error_code existsError;
    if (!fs::exists(candidate, existsError))
      return candidate;
  }
  return cachePath.parent_path() /
         (cachePath.filename().string() + tag + "fallback");
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

  const fs::path tmpPath = MakeUniqueCacheSibling(cachePath, ".tmp.");
  {
    std::ofstream output(tmpPath, std::ios::binary | std::ios::trunc);
    if (!output)
      return false;
    output << root.dump();
    output.flush();
    if (!output.good()) {
      std::error_code removeError;
      fs::remove(tmpPath, removeError);
      return false;
    }
  }

  std::error_code renameError;
  fs::rename(tmpPath, cachePath, renameError);
  if (!renameError)
    return true;

  const fs::path backupPath = MakeUniqueCacheSibling(cachePath, ".backup.");
  std::error_code backupError;
  if (fs::exists(cachePath, backupError)) {
    fs::rename(cachePath, backupPath, backupError);
    if (backupError) {
      std::error_code removeError;
      fs::remove(tmpPath, removeError);
      return false;
    }
  }

  std::error_code publishError;
  fs::rename(tmpPath, cachePath, publishError);
  if (publishError) {
    std::error_code restoreError;
    if (fs::exists(backupPath, restoreError))
      fs::rename(backupPath, cachePath, restoreError);
    std::error_code removeError;
    fs::remove(tmpPath, removeError);
    return false;
  }

  std::error_code removeBackupError;
  fs::remove(backupPath, removeBackupError);
  return true;
}

} // namespace

std::optional<GdtfCatalogSnapshot> GdtfCatalogService::GetCatalogSnapshot() const {
  return LoadCacheSnapshot();
}

// Loads and validates one reusable parsed catalog snapshot.
std::optional<GdtfParsedCatalogSnapshot>
GdtfCatalogService::GetParsedCatalogSnapshot() const {
  mvr::gdtf_catalog_parser::GdtfCatalogParseResult parsed;
  const auto snapshot = LoadCacheSnapshot(&parsed);
  if (!snapshot)
    return std::nullopt;
  return GdtfParsedCatalogSnapshot{*snapshot, std::move(parsed)};
}

GdtfCatalogRefreshResult GdtfCatalogService::RefreshCatalogIfStale(
    const RefreshCatalogFn &refreshCatalogFn, const std::string &nowUtcIso,
    long long refreshThresholdSeconds) const {
  GdtfCatalogRefreshResult result;
  mvr::gdtf_catalog_parser::GdtfCatalogParseResult cachedParsed;
  result.snapshot = LoadCacheSnapshot(&cachedParsed);
  if (result.snapshot)
    result.parsedCatalog = std::move(cachedParsed);
  result.metrics.cacheHit = result.snapshot.has_value();
  result.metrics.cacheMiss = !result.metrics.cacheHit;
  result.source = result.snapshot ? GdtfCatalogResultSource::Cache
                                  : GdtfCatalogResultSource::None;

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
    result.staleFallback = result.snapshot.has_value();
    result.failureMessage = "Online catalog refresh failed";
    return result;
  }

  const auto parsedRefresh =
      mvr::gdtf_catalog_parser::ParseCatalog(refreshedListData);
  if (!parsedRefresh.IsUsable()) {
    result.staleFallback = result.snapshot.has_value();
    result.failureMessage = "Online catalog refresh returned no usable entries";
    Logger::Instance().Log(
        Logger::Level::Warn,
        "Rejected unusable GDTF catalog refresh: bytes=" +
            std::to_string(parsedRefresh.payloadBytes) + " fingerprint=" +
            parsedRefresh.payloadFingerprint + " usable_entries=" +
            std::to_string(parsedRefresh.usableEntryCount));
    return result;
  }
  result.parsedCatalog = parsedRefresh;

  GdtfCatalogSnapshot refreshedSnapshot;
  refreshedSnapshot.listData = refreshedListData;
  refreshedSnapshot.updatedAt = nowUtcIso;
  refreshedSnapshot.lastSuccessfulRefreshAt = nowUtcIso;

  if (!SaveCacheSnapshot(refreshedSnapshot))
    result.failureMessage = "Catalog cache write failed";
  result.snapshot = refreshedSnapshot;
  result.source = GdtfCatalogResultSource::Online;

  result.metrics.refreshSucceeded = true;
  result.metrics.cacheHit = result.snapshot.has_value();
  result.metrics.cacheMiss = !result.metrics.cacheHit;
  result.metrics.cacheAgeSeconds = 0;

  return result;
}
