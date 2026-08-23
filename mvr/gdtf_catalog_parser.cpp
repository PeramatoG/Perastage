#include "gdtf_catalog_parser.h"

#include "json.hpp"

#include <charconv>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <sstream>

namespace mvr::gdtf_catalog_parser {
namespace {
using Json = nlohmann::json;

// Converts a catalog scalar to text without changing numeric identifiers.
std::string JsonToString(const Json &value) {
  if (value.is_string())
    return value.get<std::string>();
  if (value.is_number())
    return value.dump();
  if (value.is_array()) {
    std::string result;
    for (const auto &element : value) {
      const std::string text = element.is_object() && element.contains("name")
                                   ? JsonToString(element["name"])
                                   : JsonToString(element);
      if (text.empty())
        continue;
      if (!result.empty())
        result += ", ";
      result += text;
    }
    return result;
  }
  if (value.is_object())
    return value.dump();
  return {};
}

// Locates official and historically cached catalog arrays recursively.
const Json *FindCatalogArray(const Json &node) {
  if (node.is_array())
    return &node;
  if (!node.is_object())
    return nullptr;
  static const std::array<const char *, 8> keys = {
      "data", "fixtures", "list", "results", "items", "docs", "rows", "catalog"};
  for (const char *key : keys) {
    const auto it = node.find(key);
    if (it == node.end())
      continue;
    if (const Json *array = FindCatalogArray(*it))
      return array;
  }
  return nullptr;
}

// Computes a stable non-cryptographic fingerprint for payload diagnostics.
std::string FingerprintPayload(const std::string &payload) {
  std::uint64_t hash = 14695981039346656037ull;
  for (unsigned char value : payload) {
    hash ^= value;
    hash *= 1099511628211ull;
  }
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << hash;
  return stream.str();
}

// Normalizes catalog search text without GUI dependencies.
std::string NormalizeSearchText(const std::string &text) {
  std::string normalized;
  for (unsigned char value : text) {
    if (std::isalnum(value))
      normalized.push_back(static_cast<char>(std::tolower(value)));
  }
  return normalized;
}

// Converts the API's numeric or string timestamps and footprints to integers.
long long JsonToInteger(const Json &value) {
  if (value.is_number_integer())
    return value.get<long long>();
  if (value.is_number())
    return static_cast<long long>(value.get<double>());
  if (!value.is_string())
    return 0;
  long long parsed = 0;
  const std::string &text = value.get_ref<const std::string &>();
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  return result.ec == std::errc{} ? parsed : 0;
}

// Converts the API's numeric or string rating to a floating-point value.
float JsonToFloat(const Json &value) {
  if (value.is_number())
    return static_cast<float>(value.get<double>());
  if (!value.is_string())
    return 0.0f;
  try {
    return std::stof(value.get<std::string>());
  } catch (...) {
    return 0.0f;
  }
}

// Returns the first present catalog field from a compatibility key list.
std::string GetText(const Json &item, std::initializer_list<const char *> keys) {
  for (const char *key : keys) {
    const auto it = item.find(key);
    if (it != item.end())
      return JsonToString(*it);
  }
  return {};
}

// Parses official modes/dmxfootprint fields, then legacy cached aliases.
std::vector<gdtf_catalog_matcher::GdtfCatalogModeCandidate>
ParseModes(const Json &item) {
  const Json *modeList = nullptr;
  if (item.contains("modes") && item["modes"].is_array())
    modeList = &item["modes"];
  else if (item.contains("dmxModes") && item["dmxModes"].is_array())
    modeList = &item["dmxModes"];
  if (!modeList)
    return {};

  std::vector<gdtf_catalog_matcher::GdtfCatalogModeCandidate> modes;
  for (const auto &mode : *modeList) {
    if (!mode.is_object())
      continue;
    gdtf_catalog_matcher::GdtfCatalogModeCandidate parsed;
    parsed.name = JsonToString(mode.value("name", Json{}));
    parsed.footprint = static_cast<int>(JsonToInteger(
        mode.contains("dmxfootprint") ? mode["dmxfootprint"]
                                      : mode.value("dmxFootprint", Json{})));
    if (!parsed.name.empty() || parsed.footprint > 0)
      modes.push_back(std::move(parsed));
  }
  return modes;
}
} // namespace

// Parses one catalog payload into displayable shared domain records.
GdtfCatalogParseResult ParseCatalog(const std::string &payload) {
  GdtfCatalogParseResult result;
  result.payloadBytes = payload.size();
  result.payloadFingerprint = FingerprintPayload(payload);
  Json list = Json::parse(payload, nullptr, false);
  if (list.is_discarded())
    return result;
  const Json *catalogArray = FindCatalogArray(list);
  if (!catalogArray)
    return result;

  for (const auto &item : *catalogArray) {
    if (!item.is_object())
      continue;
    gdtf_catalog_matcher::GdtfCatalogEntry entry;
    entry.rid = GetText(item, {"rid", "revisionId"});
    entry.manufacturer = GetText(item, {"manufacturer", "brand", "mfr"});
    entry.fixtureName = GetText(item, {"fixture", "name", "model"});
    entry.uuid = GetText(item, {"uuid"});
    entry.uploader = GetText(item, {"uploader"});
    entry.url = GetText(item, {"url", "download", "downloadUrl"});
    entry.creator = GetText(item, {"creator", "user", "userName"});
    entry.creationDate = GetText(item, {"creationDate"});
    entry.revision = GetText(item, {"revision"});
    entry.version = GetText(item, {"version"});
    entry.ratingText = GetText(item, {"rating"});
    entry.lastModifiedUnix = JsonToInteger(item.value("lastModified", Json{}));
    entry.rating = JsonToFloat(item.value("rating", Json{}));
    entry.modes = ParseModes(item);
    entry.downloadable = !entry.rid.empty();
    if (!entry.downloadable)
      entry.downloadabilityReason = "missing revision identifier";
    result.entries.push_back(std::move(entry));
  }
  return result;
}

// Filters shared catalog records for the search dialog and domain tests.
std::vector<std::size_t> FilterCatalogEntries(
    const std::vector<gdtf_catalog_matcher::GdtfCatalogEntry> &entries,
    const std::string &manufacturerQuery, const std::string &fixtureQuery) {
  const std::string manufacturer = NormalizeSearchText(manufacturerQuery);
  const std::string fixture = NormalizeSearchText(fixtureQuery);
  std::vector<std::size_t> matches;
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (!manufacturer.empty() &&
        NormalizeSearchText(entries[index].manufacturer).find(manufacturer) ==
            std::string::npos)
      continue;
    if (!fixture.empty() &&
        NormalizeSearchText(entries[index].fixtureName).find(fixture) ==
            std::string::npos)
      continue;
    matches.push_back(index);
  }
  return matches;
}

// Preserves the established parser API as a shared-domain convenience adapter.
std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
ParseCatalogEntries(const std::string &payload) {
  return ParseCatalog(payload).entries;
}

} // namespace mvr::gdtf_catalog_parser
