/* This file is part of Perastage. */
#pragma once
#include "fixture.h"
#include "fixture_network_service.h"
#include "fixture_patch_address.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace perastage::patch {
enum class FixturePatchConflict {
  None, InvalidBreak, DuplicateBreak, InvalidValue, MissingPrimary,
  PrimaryOutsideEditableRange, InvalidPrimary, InvalidNetworkAssignment
};
struct FixturePatchResult {
  FixturePatchConflict conflict = FixturePatchConflict::None;
  std::string fixtureUuid;
  std::string detail;
  std::optional<FixtureDmxAddresses> addresses;
  std::string primary;
  explicit operator bool() const { return conflict == FixturePatchConflict::None; }
};

inline std::string_view TrimDmx(std::string_view value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) return {};
  return value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
}
// Decimal identities and values are not bounded by the legacy int universe UI.
inline std::optional<std::string> DmxInteger(std::string_view text, bool signedValue) {
  text = TrimDmx(text);
  bool negative = false;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.empty() || !std::all_of(text.begin(), text.end(),
      [](char ch) { return ch >= '0' && ch <= '9'; })) return std::nullopt;
  const auto first = text.find_first_not_of('0');
  std::string value = first == std::string_view::npos ? "0" : std::string(text.substr(first));
  if (negative && value != "0") {
    if (!signedValue) return std::nullopt;
    value.insert(value.begin(), '-');
  }
  return value;
}
inline bool IsDmxValue(std::string_view text) {
  text = TrimDmx(text);
  const auto dot = text.find('.');
  if (dot == std::string_view::npos) {
    const auto value = DmxInteger(text, false);
    return value && *value != "0";
  }
  const auto universe = DmxInteger(text.substr(0, dot), false);
  const auto channel = DmxInteger(text.substr(dot + 1), false);
  return universe && *universe != "0" && channel && *channel != "0" &&
      (channel->size() < 3 || (channel->size() == 3 && *channel <= "512"));
}
inline std::string DmxPrimaryProjection(std::string_view text) {
  text = TrimDmx(text);
  if (!IsDmxValue(text)) return {};
  const auto dot = text.find('.');
  const auto first = DmxInteger(text.substr(0, dot), false);
  if (!first || first->size() > 19) return {};
  std::uint64_t value = 0;
  const auto parsed = std::from_chars(first->data(), first->data() + first->size(), value);
  if (parsed.ec != std::errc{}) return {};
  const std::uint64_t universe = dot == std::string_view::npos ? (value - 1) / 512 + 1 : value;
  if (universe > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return {};
  const auto channel = dot == std::string_view::npos ? std::to_string((value - 1) % 512 + 1)
      : *DmxInteger(text.substr(dot + 1), false);
  return std::to_string(universe) + "." + channel;
}
inline FixturePatchResult PatchFailure(const Fixture &fixture, FixturePatchConflict code,
                                       const std::string &reason) {
  FixturePatchResult result;
  result.conflict = code;
  result.fixtureUuid = fixture.uuid;
  result.detail = "Fixture DMX patch conflict (uuid=" + fixture.uuid + "): " + reason;
  return result;
}
// Validates native values as well as source data. Missing break defaults to 0;
// duplicates after integer normalization are ambiguous and are never repaired.
inline FixturePatchResult InspectFixtureAddresses(const Fixture &fixture) {
  if (const auto network = fixture_network::Validate(fixture); !network)
    return PatchFailure(fixture, FixturePatchConflict::InvalidNetworkAssignment, network.detail);
  FixturePatchResult result;
  result.fixtureUuid = fixture.uuid;
  result.addresses = fixture.dmxAddresses;
  if (!fixture.dmxAddresses) { result.primary = fixture.address; return result; }
  std::unordered_set<std::string> identifiers;
  for (const auto &entry : fixture.dmxAddresses->entries) {
    const auto key = DmxInteger(entry.breakId.value_or("0"), true);
    if (!key) return PatchFailure(fixture, FixturePatchConflict::InvalidBreak, "invalid break identifier");
    if (!identifiers.insert(*key).second)
      return PatchFailure(fixture, FixturePatchConflict::DuplicateBreak, "duplicate break identifier " + *key);
    if (!IsDmxValue(entry.value))
      return PatchFailure(fixture, FixturePatchConflict::InvalidValue, "invalid Address value");
    if (*key == "0") result.primary = DmxPrimaryProjection(entry.value);
  }
  return result;
}
// Applies an explicit primary edit on a private value before publication.
inline FixturePatchResult PrepareFixturePrimaryPatch(const Fixture &fixture,
                                                      const std::string &requested) {
  auto result = InspectFixtureAddresses(fixture);
  if (!result) return result;
  if (requested == result.primary) return result;
  std::optional<FixturePatchAddress> parsed;
  if (!requested.empty()) {
    parsed = ParseFixturePatchAddress(requested);
    if (!parsed) return PatchFailure(fixture, FixturePatchConflict::InvalidPrimary,
                                    "expected a positive universe.channel with channel 1..512");
  }
  if (!result.addresses) result.addresses.emplace();
  auto &entries = result.addresses->entries;
  const auto primary = std::find_if(entries.begin(), entries.end(), [](const auto &entry) {
    return DmxInteger(entry.breakId.value_or("0"), true) == std::optional<std::string>("0");
  });
  if (primary == entries.end() && !entries.empty())
    return PatchFailure(fixture, FixturePatchConflict::MissingPrimary,
                        "no break 0 assignment maps to the primary patch field");
  if (primary != entries.end() && DmxPrimaryProjection(primary->value).empty())
    return PatchFailure(fixture, FixturePatchConflict::PrimaryOutsideEditableRange,
                        "break 0 exceeds the editable universe range");
  if (requested.empty()) {
    if (primary != entries.end()) entries.erase(primary);
  } else {
    const auto absolute = (static_cast<std::int64_t>(parsed->universe) - 1) * 512 + parsed->channel;
    if (primary == entries.end()) entries.push_back({std::string("0"), std::to_string(absolute)});
    else primary->value = std::to_string(absolute);
  }
  result.primary = requested;
  return result;
}
inline FixturePatchResult SetFixturePrimaryPatch(Fixture &fixture, const std::string &requested) {
  auto result = PrepareFixturePrimaryPatch(fixture, requested);
  if (!result) return result;
  fixture.address = result.primary;
  fixture.dmxAddresses = result.addresses;
  return result;
}
// Compatibility writes to Fixture.address are interpreted only on a private
// export snapshot. Conflicts block serialization; authored side assignments stay.
inline FixturePatchResult ResolveFixturePatch(const Fixture &fixture) {
  auto result = InspectFixtureAddresses(fixture);
  if (!result || !fixture.dmxAddresses || fixture.address == result.primary) return result;
  return PrepareFixturePrimaryPatch(fixture, fixture.address);
}
} // namespace perastage::patch
