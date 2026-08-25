#pragma once

#include <functional>
#include <optional>
#include <string>
#include "../mvr/gdtf_catalog_parser.h"

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

enum class GdtfCatalogResultSource {
  None,
  Cache,
  Online
};

struct GdtfCatalogRefreshResult {
  std::optional<GdtfCatalogSnapshot> snapshot;
  GdtfCatalogRefreshMetrics metrics;
  GdtfCatalogResultSource source = GdtfCatalogResultSource::None;
  bool staleFallback = false;
  std::string failureMessage;
  std::optional<mvr::gdtf_catalog_parser::GdtfCatalogParseResult> parsedCatalog;
};

struct GdtfParsedCatalogSnapshot {
  GdtfCatalogSnapshot snapshot;
  mvr::gdtf_catalog_parser::GdtfCatalogParseResult parsed;
};

class GdtfCatalogService {
public:
  using RefreshCatalogFn = std::function<bool(std::string &listData)>;

  std::optional<GdtfCatalogSnapshot> GetCatalogSnapshot() const;
  std::optional<GdtfParsedCatalogSnapshot> GetParsedCatalogSnapshot() const;

  GdtfCatalogRefreshResult
  RefreshCatalogIfStale(const RefreshCatalogFn &refreshCatalogFn,
                        const std::string &nowUtcIso,
                        long long refreshThresholdSeconds = 3600) const;
};
