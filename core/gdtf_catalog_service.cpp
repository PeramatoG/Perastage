#include "gdtf_catalog_service.h"
#include "apppaths.h"
#include "cache_service.h"
#include "filesystem_path_utils.h"
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
std::optional<GdtfCatalogSnapshot> ReadCacheSnapshot(
    const fs::path &cachePath,
    mvr::gdtf_catalog_parser::GdtfCatalogParseResult *parsedOut = nullptr) {
  std::error_code ec;
  if (!CacheService::IsSafeDirectory(cachePath.parent_path()) ||
      !fs::is_regular_file(fs::symlink_status(cachePath, ec)) || ec)
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
  if (root.contains("catalog_timestamp") &&
      (root["catalog_timestamp"].is_string() || root["catalog_timestamp"].is_number()))
    snapshot.catalogTimestamp = root["catalog_timestamp"].is_string()
        ? root["catalog_timestamp"].get<std::string>()
        : root["catalog_timestamp"].dump();

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
  if (snapshot.catalogTimestamp.empty())
    snapshot.catalogTimestamp = parsed.catalogTimestamp;
  if (parsedOut) {
    *parsedOut = parsed;
    parsedOut->catalogTimestamp = snapshot.catalogTimestamp;
  }

  return snapshot;
}

fs::path MakeUniqueCacheSibling(const fs::path &, const std::string &);

// Migrate only validated snapshots, publish without overwriting a current cache,
// and retain legacy data whenever publication fails.
std::optional<GdtfCatalogSnapshot> LoadCacheSnapshot(
    mvr::gdtf_catalog_parser::GdtfCatalogParseResult *parsedOut = nullptr) {
  const auto target = AppPaths::GetGdtfCatalogCachePath();
  if (auto cached = ReadCacheSnapshot(target, parsedOut))
    return cached;
  std::error_code ec;
  if (fs::exists(target, ec) || ec ||
      fs::exists(AppPaths::GetCatalogCacheMigrationMarker(), ec) || ec)
    return std::nullopt;
  const auto standardDir = PathUtils::PathFromUtf8(
      std::string(wxStandardPaths::Get().GetUserDataDir().ToUTF8()));
  for (const auto &dir : {AppPaths::GetUserDataDir(), standardDir,
                          AppPaths::GetUserDataTempFallbackDir()}) {
    const auto legacy = dir / "gdtf_catalog_cache.json";
    if (!ReadCacheSnapshot(legacy)) continue;
    if (!CacheService::EnsureDirectories()) return std::nullopt;
    const auto temporary = MakeUniqueCacheSibling(target, ".tmp.");
    fs::copy_file(legacy, temporary, fs::copy_options::none, ec);
    if (!ec && !ReadCacheSnapshot(temporary)) {
      fs::remove(temporary, ec);
      return std::nullopt;
    }
    if (!ec) fs::rename(temporary, target, ec);
    if (ec) {
      Logger::Instance().Log(Logger::Level::Warn, "Could not migrate GDTF catalog cache: " + ec.message());
      fs::remove(temporary, ec);
      return std::nullopt;
    }
    if (!CacheService::MarkCatalogMigrationComplete()) {
      fs::remove(target, ec);
      return std::nullopt;
    }
    fs::remove(legacy, ec);
    if (ec)
      Logger::Instance().Log(Logger::Level::Warn, "Could not remove migrated catalog cache: " + ec.message());
    return ReadCacheSnapshot(target, parsedOut);
  }
  return std::nullopt;
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
  const fs::path cachePath = AppPaths::GetGdtfCatalogCachePath();
  if (!CacheService::EnsureDirectories() ||
      !CacheService::IsSafeCacheFile(cachePath))
    return false;

  nlohmann::json root;
  root["version"] = 2;
  root["updated_at"] = snapshot.updatedAt;
  root["last_successful_refresh_at"] = snapshot.lastSuccessfulRefreshAt;
  root["catalog_timestamp"] = snapshot.catalogTimestamp;
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

bool GdtfCatalogService::IsCatalogStale(const GdtfCatalogSnapshot &snapshot,
                                      const std::string &nowUtcIso,
                                      long long refreshThresholdSeconds) {
  const auto age = CacheAgeSeconds(snapshot, nowUtcIso);
  return age < 0 || age >= refreshThresholdSeconds;
}

long long GdtfCatalogService::CacheAgeSeconds(const GdtfCatalogSnapshot &snapshot,
                                            const std::string &nowUtcIso) {
  return ComputeAgeSeconds(snapshot.updatedAt, nowUtcIso).value_or(-1);
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

  if (result.snapshot)
    result.metrics.cacheAgeSeconds = CacheAgeSeconds(*result.snapshot, nowUtcIso);

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
  GdtfCatalogSnapshot refreshedSnapshot;
  result.metrics.catalogUnchanged = result.parsedCatalog &&
      mvr::gdtf_catalog_parser::CatalogsEquivalent(*result.parsedCatalog, parsedRefresh);
  refreshedSnapshot.listData = result.metrics.catalogUnchanged
      ? result.snapshot->listData : std::move(refreshedListData);
  if (!result.metrics.catalogUnchanged)
    result.parsedCatalog = parsedRefresh;
  refreshedSnapshot.updatedAt = nowUtcIso;
  refreshedSnapshot.lastSuccessfulRefreshAt = nowUtcIso;
  refreshedSnapshot.catalogTimestamp = parsedRefresh.catalogTimestamp;
  result.parsedCatalog->catalogTimestamp = refreshedSnapshot.catalogTimestamp;

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
