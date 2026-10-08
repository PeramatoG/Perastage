#pragma once

#include "../mvr/gdtf_catalog_matcher.h"
#include <optional>
#include <string>
#include <vector>

namespace gdtf_catalog_browser {

struct SearchQuery {
  std::string general;
  std::string manufacturer;
  std::string fixture;
};

// Index lifetime follows one parsed snapshot; searches normalize only queries.
class SearchIndex {
public:
  explicit SearchIndex(
      const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &entries = {});
  std::vector<std::size_t> Search(const SearchQuery &query) const;
  std::optional<std::size_t> PageForRid(
      const std::vector<std::size_t> &matches, const std::string &rid,
      std::size_t pageSize) const;

private:
  struct Row {
    std::string rid;
    std::string manufacturer;
    std::string fixture;
    std::string searchable;
  };
  std::vector<Row> rows;
};

std::string FormatFileSize(const std::optional<long long> &bytes);
// Missing, N/A and invalid ratings are absent, rather than synthetic zeroes.
std::optional<std::string> MeaningfulRating(const std::string &rating);

} // namespace gdtf_catalog_browser
