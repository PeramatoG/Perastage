#pragma once

#include <functional>
#include <optional>
#include <string>

struct GdtfCatalogSnapshot {
  std::string listData;
  std::string updatedAt;
  std::string lastSuccessfulRefreshAt;
};

struct GdtfCatalogRefreshMetrics {
  bool cacheHit = false;
  bool cacheMiss = true;
  bool refreshAttempted = false;
  bool refreshSucceeded = false;
  long long cacheAgeSeconds = -1;
};

struct GdtfCatalogRefreshResult {
  std::optional<GdtfCatalogSnapshot> snapshot;
  GdtfCatalogRefreshMetrics metrics;
};

class GdtfCatalogService {
public:
  using RefreshCatalogFn = std::function<bool(std::string &listData)>;

  std::optional<GdtfCatalogSnapshot> GetCatalogSnapshot() const;

  GdtfCatalogRefreshResult
  RefreshCatalogIfStale(const RefreshCatalogFn &refreshCatalogFn,
                        const std::string &nowUtcIso,
                        long long refreshThresholdSeconds = 3600) const;
};
