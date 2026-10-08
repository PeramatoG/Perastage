#include "gdtf_catalog_browser_model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace gdtf_catalog_browser {
namespace {
std::string Normalize(const std::string &text) {
  std::string normalized;
  normalized.reserve(text.size());
  for (unsigned char value : text) {
    // Preserve UTF-8 bytes; ASCII case and punctuation follow existing filters.
    if (value >= 128 || (value >= 'a' && value <= 'z') ||
        (value >= '0' && value <= '9'))
      normalized += static_cast<char>(value);
    else if (value >= 'A' && value <= 'Z')
      normalized += static_cast<char>(value - 'A' + 'a');
  }
  return normalized;
}

std::vector<std::string> Tokens(const std::string &query) {
  std::istringstream stream(query);
  stream.imbue(std::locale::classic());
  std::vector<std::string> tokens;
  for (std::string token; stream >> token;) {
    token = Normalize(token);
    if (!token.empty())
      tokens.push_back(std::move(token));
  }
  return tokens;
}
} // namespace

SearchIndex::SearchIndex(
    const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &entries) {
  rows.reserve(entries.size());
  for (const auto &entry : entries) {
    Row row{entry.rid, Normalize(entry.manufacturer),
            Normalize(entry.fixtureName), {}};
    row.searchable = row.manufacturer + " " + row.fixture + " " +
                     Normalize(entry.uuid) + " " + Normalize(entry.revision);
    for (const auto &mode : entry.modes)
      row.searchable += " " + Normalize(mode.name);
    rows.push_back(std::move(row));
  }
}

std::vector<std::size_t> SearchIndex::Search(const SearchQuery &query) const {
  const auto tokens = Tokens(query.general);
  const auto manufacturer = Normalize(query.manufacturer);
  const auto fixture = Normalize(query.fixture);
  std::vector<std::size_t> matches;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto &row = rows[index];
    if (row.manufacturer.find(manufacturer) == std::string::npos ||
        row.fixture.find(fixture) == std::string::npos)
      continue;
    if (std::all_of(tokens.begin(), tokens.end(), [&](const auto &token) {
          return row.searchable.find(token) != std::string::npos;
        }))
      matches.push_back(index);
  }
  return matches;
}

std::optional<std::size_t> SearchIndex::PageForRid(
    const std::vector<std::size_t> &matches, const std::string &rid,
    std::size_t pageSize) const {
  if (rid.empty() || pageSize == 0)
    return std::nullopt;
  for (std::size_t position = 0; position < matches.size(); ++position)
    if (matches[position] < rows.size() && rows[matches[position]].rid == rid)
      return position / pageSize;
  return std::nullopt;
}

std::string FormatFileSize(const std::optional<long long> &bytes) {
  if (!bytes || *bytes <= 0)
    return {};
  static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
  double amount = static_cast<double>(*bytes);
  std::size_t unit = 0;
  while (amount >= 1024 && unit < 6) {
    amount /= 1024;
    ++unit;
  }
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  if (unit == 0)
    stream << *bytes;
  else
    stream << std::fixed << std::setprecision(1) << amount;
  stream << ' ' << units[unit];
  return stream.str();
}

std::optional<std::string> MeaningfulRating(const std::string &rating) {
  std::istringstream stream(rating);
  stream.imbue(std::locale::classic());
  double number = 0;
  if (!(stream >> number) || !std::isfinite(number) || number < 0)
    return std::nullopt;
  stream >> std::ws;
  return stream.eof() ? std::optional<std::string>(rating) : std::nullopt;
}
} // namespace gdtf_catalog_browser
