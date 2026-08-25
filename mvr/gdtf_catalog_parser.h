#pragma once

#include "gdtf_catalog_matcher.h"

#include <string>
#include <vector>

namespace mvr::gdtf_catalog_parser {

struct GdtfCatalogParseResult {
  std::vector<gdtf_catalog_matcher::GdtfCatalogEntry> entries;
  std::size_t payloadBytes = 0;
  std::string payloadFingerprint;
  std::size_t usableEntryCount = 0;
  bool schemaRecognized = false;
  long long parseMs = 0;

  // Reports whether parsing found a recognized catalog with usable entries.
  bool IsUsable() const {
    return schemaRecognized && usableEntryCount > 0;
  }
};

GdtfCatalogParseResult ParseCatalog(const std::string &payload);

std::vector<std::size_t> FilterCatalogEntries(
    const std::vector<gdtf_catalog_matcher::GdtfCatalogEntry> &entries,
    const std::string &manufacturerQuery, const std::string &fixtureQuery);

// Parses a GDTF Share getList.php response while retaining legacy cache aliases.
std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
ParseCatalogEntries(const std::string &payload);

} // namespace mvr::gdtf_catalog_parser
