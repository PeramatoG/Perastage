#include "gdtf_catalog_parser.h"

#include "json.hpp"

#include <charconv>
#include <initializer_list>

namespace mvr::gdtf_catalog_parser {
namespace {
using Json = nlohmann::json;

// Converts a catalog scalar to text without changing numeric identifiers.
std::string JsonToString(const Json &value) {
  if (value.is_string())
    return value.get<std::string>();
  if (value.is_number())
    return value.dump();
  return {};
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

// Parses a GDTF Share getList.php response while retaining legacy cache aliases.
std::vector<gdtf_catalog_matcher::GdtfCatalogEntry>
ParseCatalogEntries(const std::string &payload) {
  Json list = Json::parse(payload, nullptr, false);
  if (list.is_discarded())
    return {};
  if (list.is_object()) {
    if (list.contains("data"))
      list = list["data"];
    if (list.contains("fixtures"))
      list = list["fixtures"];
    if (list.contains("list"))
      list = list["list"];
  }
  if (!list.is_array())
    return {};

  std::vector<gdtf_catalog_matcher::GdtfCatalogEntry> entries;
  for (const auto &item : list) {
    if (!item.is_object())
      continue;
    gdtf_catalog_matcher::GdtfCatalogEntry entry;
    entry.rid = GetText(item, {"rid", "revisionId"});
    entry.manufacturer = GetText(item, {"manufacturer", "brand", "mfr"});
    entry.fixtureName = GetText(item, {"fixture", "name", "model"});
    entry.lastModifiedUnix = JsonToInteger(item.value("lastModified", Json{}));
    entry.rating = JsonToFloat(item.value("rating", Json{}));
    entry.modes = ParseModes(item);
    if (!entry.rid.empty())
      entries.push_back(std::move(entry));
  }
  return entries;
}

} // namespace mvr::gdtf_catalog_parser
