#include "apppaths.h"
#include "cache_service.h"
#include "gdtf_catalog_service.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <wx/init.h>
#include <wx/app.h>
#include <fstream>
#include "json.hpp"

namespace {
const auto testDirectory = std::filesystem::temp_directory_path() /
    ("perastage-catalog-cache-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
const std::string catalog =
    R"({"timestamp":1700000000,"list":[{"rid":"123","manufacturer":"Acme","fixture":"Spot","filesize":2048}]})";
}

// Keep catalog storage isolated from existing user settings and cached catalogs.
namespace AppPaths {
std::filesystem::path GetUserDataDir() { return testDirectory; }
std::filesystem::path GetUserDataTempFallbackDir() { return testDirectory; }
}

int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  wxTheApp->SetAppName(testDirectory.filename().string());
  GdtfCatalogService service;
  const auto cachePath = AppPaths::GetGdtfCatalogCachePath();
  const auto legacyPath = testDirectory / "gdtf_catalog_cache.json";
  assert(CacheService::EnsureDirectories());
  std::filesystem::create_directories(testDirectory);
  std::ofstream(legacyPath) << nlohmann::json{{"list_data", "[]"}}.dump();
  assert(!service.GetCatalogSnapshot());
  assert(std::filesystem::exists(legacyPath) && !std::filesystem::exists(cachePath));
  // A failed migration retains the valid original for a later retry.
  std::filesystem::remove_all(cachePath.parent_path());
  std::ofstream(cachePath.parent_path()) << "directory blocked";
  std::ofstream(legacyPath) << nlohmann::json{{"list_data", catalog}}.dump();
  assert(!service.GetCatalogSnapshot());
  assert(std::filesystem::exists(legacyPath));
  std::filesystem::remove(cachePath.parent_path());
  assert(CacheService::EnsureDirectories());
  for (int version : {1, 2}) {
    const auto oldPayload = version == 1
        ? std::string(R"([{"revisionId":123,"model":"Legacy Spot"}])") : catalog;
    std::filesystem::remove(cachePath);
    std::filesystem::remove(AppPaths::GetCatalogCacheMigrationMarker());
    std::ofstream(legacyPath) << nlohmann::json{
        {"version", version}, {"updated_at", "2026-10-08 09:00:00"},
        {"list_data", oldPayload}}.dump();
    const auto old = service.GetParsedCatalogSnapshot();
    assert(old && old->parsed.IsUsable());
    assert(std::filesystem::exists(cachePath));
    assert(!std::filesystem::exists(legacyPath));
    assert(old->snapshot.catalogTimestamp == (version == 1 ? "" : "1700000000"));
    if (version == 1)
      assert(!old->parsed.entries[0].fileSizeBytes);
    assert(old->snapshot.lastSuccessfulRefreshAt == old->snapshot.updatedAt);
    assert(!GdtfCatalogService::IsCatalogStale(old->snapshot, "2026-10-08 09:59:59"));
    assert(GdtfCatalogService::IsCatalogStale(old->snapshot, "2026-10-08 10:00:00"));
  }
  // A valid centralized catalog takes precedence over a different legacy one.
  std::ofstream(legacyPath) << nlohmann::json{{"list_data", R"([{"revisionId":999,"model":"Other"}])"}}.dump();
  assert(service.GetParsedCatalogSnapshot()->parsed.entries[0].fixtureName == "Spot");
  assert(std::filesystem::exists(legacyPath));
  assert(CacheService::Clear().Succeeded());
  // A remaining valid legacy copy must not undo an explicit cache discard.
  std::ofstream(legacyPath) << nlohmann::json{{"list_data", catalog}}.dump();
  assert(!service.GetCatalogSnapshot());
  std::filesystem::remove(legacyPath);
  int refreshes = 0;
  const auto refresh = [&](std::string &data) {
    ++refreshes;
    data = catalog;
    return true;
  };
  const auto missing = service.RefreshCatalogIfStale(refresh, "2026-10-08 10:00:00");
  assert(missing.snapshot && missing.metrics.refreshSucceeded && refreshes == 1);
  assert(missing.parsedCatalog && missing.parsedCatalog->entries[0].fileSizeBytes == 2048);
  assert(missing.snapshot->catalogTimestamp == "1700000000");
  nlohmann::json persisted;
  std::ifstream(cachePath) >> persisted;
  assert(persisted["catalog_timestamp"] == "1700000000" && persisted["version"] == 2);
  const auto fresh = service.RefreshCatalogIfStale(refresh, "2026-10-08 10:59:59");
  assert(fresh.snapshot && !fresh.metrics.refreshAttempted && refreshes == 1);
  assert(fresh.source == GdtfCatalogResultSource::Cache && !fresh.staleFallback);
  const auto stale = service.RefreshCatalogIfStale(refresh, "2026-10-08 11:00:00");
  assert(stale.snapshot && stale.metrics.refreshSucceeded && refreshes == 2);
  assert(stale.metrics.catalogUnchanged && stale.snapshot->listData == catalog);
  const auto failing = [&](std::string &) { ++refreshes; return false; };
  const auto fallback = service.RefreshCatalogIfStale(failing, "2026-10-08 12:00:00");
  assert(fallback.snapshot && fallback.staleFallback && refreshes == 3);
  assert(fallback.source == GdtfCatalogResultSource::Cache);
  assert(fallback.parsedCatalog && fallback.snapshot->catalogTimestamp == "1700000000");
  const auto forced = service.RefreshCatalogIfStale(refresh, "2026-10-08 11:00:01", 0);
  assert(forced.metrics.refreshSucceeded && refreshes == 4);
  const auto reformatted = service.RefreshCatalogIfStale([&](std::string &data) {
    data = R"({ "list": [{"filesize":"2048","fixture":"Spot","manufacturer":"Acme","rid":123}], "timestamp":"1700000001" })";
    return true;
  }, "2026-10-08 12:01:00", 0);
  assert(reformatted.metrics.catalogUnchanged && reformatted.snapshot->listData == catalog);
  assert(reformatted.snapshot->catalogTimestamp == "1700000001");
  assert(reformatted.parsedCatalog->catalogTimestamp == "1700000001");
  assert(service.GetParsedCatalogSnapshot()->snapshot.catalogTimestamp == "1700000001");
  const auto afterUnchanged = service.RefreshCatalogIfStale(refresh, "2026-10-08 12:59:00");
  assert(!afterUnchanged.metrics.refreshAttempted && refreshes == 4);
  const auto invalidRefresh = service.RefreshCatalogIfStale([](std::string &data) {
    data = R"({"timestamp":999,"list":[]})";
    return true;
  }, "2026-10-08 13:01:00");
  assert(invalidRefresh.staleFallback && invalidRefresh.parsedCatalog->IsUsable());
  assert(service.GetCatalogSnapshot()->catalogTimestamp == "1700000001");
  const auto changed = service.RefreshCatalogIfStale([](std::string &data) {
    data = R"({"timestamp":1700000002,"list":[{"rid":123,"fixture":"Changed"}]})";
    return true;
  }, "2026-10-08 13:01:01");
  assert(changed.metrics.refreshSucceeded && !changed.metrics.catalogUnchanged);
  assert(changed.parsedCatalog->entries[0].fixtureName == "Changed");
  assert(CacheService::Clear().Succeeded());
  const auto noCache = service.RefreshCatalogIfStale(failing, "2026-10-08 12:00:00");
  assert(!noCache.snapshot && !noCache.staleFallback && refreshes == 5);
  std::filesystem::remove_all(testDirectory);
}
