#include "apppaths.h"
#include "gdtf_catalog_service.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <wx/init.h>
#include <wx/app.h>

namespace {
const auto testDirectory = std::filesystem::temp_directory_path() /
    ("perastage-catalog-cache-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
const std::string catalog =
    R"({"list":[{"rid":"123","manufacturer":"Acme","fixture":"Spot"}]})";
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
  int refreshes = 0;
  const auto refresh = [&](std::string &data) {
    ++refreshes;
    data = catalog;
    return true;
  };
  const auto missing = service.RefreshCatalogIfStale(refresh, "2026-10-08 10:00:00");
  assert(missing.snapshot && missing.metrics.refreshSucceeded && refreshes == 1);
  const auto fresh = service.RefreshCatalogIfStale(refresh, "2026-10-08 10:59:59");
  assert(fresh.snapshot && !fresh.metrics.refreshAttempted && refreshes == 1);
  assert(fresh.source == GdtfCatalogResultSource::Cache && !fresh.staleFallback);
  const auto stale = service.RefreshCatalogIfStale(refresh, "2026-10-08 11:00:00");
  assert(stale.snapshot && stale.metrics.refreshSucceeded && refreshes == 2);
  const auto failing = [&](std::string &) { ++refreshes; return false; };
  const auto fallback = service.RefreshCatalogIfStale(failing, "2026-10-08 12:00:00");
  assert(fallback.snapshot && fallback.staleFallback && refreshes == 3);
  assert(fallback.source == GdtfCatalogResultSource::Cache);
  const auto forced = service.RefreshCatalogIfStale(refresh, "2026-10-08 11:00:01", 0);
  assert(forced.metrics.refreshSucceeded && refreshes == 4);
  std::filesystem::remove_all(testDirectory);
  const auto noCache = service.RefreshCatalogIfStale(failing, "2026-10-08 12:00:00");
  assert(!noCache.snapshot && !noCache.staleFallback && refreshes == 5);
  std::filesystem::remove_all(testDirectory);
}
