/* This file is part of Perastage. */
#pragma once
#include "../models/fixture.h"
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace fixture_network {
enum class Conflict { None, MissingGeometry, InvalidValue, UnknownGeometry,
                      AmbiguousGeometry, UnavailableGdtf, InvalidBinding };
struct Result {
  Conflict conflict = Conflict::None;
  std::string fixtureUuid, geometry, detail;
  explicit operator bool() const { return conflict == Conflict::None; }
};
template <class Node>
inline Result Failure(const Node &fixture, Conflict conflict,
                      const std::string &geometry, const std::string &reason) {
  return {conflict, fixture.uuid, geometry, "MVR Network (uuid=" + fixture.uuid +
      ", geometry=" + geometry + "): " + reason};
}
inline bool Ipv4(std::string_view text) {
  int count = 0;
  while (!text.empty()) {
    const auto end = text.find('.');
    const auto part = text.substr(0, end);
    if (part.empty() || part.size() > 3) return false;
    unsigned value = 0;
    for (const char ch : part) {
      if (ch < '0' || ch > '9') return false;
      value = value * 10 + static_cast<unsigned>(ch - '0');
    }
    if (value > 255 || ++count > 4) return false;
    if (end == std::string_view::npos) return count == 4;
    text.remove_prefix(end + 1);
  }
  return false;
}
inline bool Ipv6(std::string_view text) {
  const auto compression = text.find("::");
  if (compression != std::string_view::npos &&
      text.find("::", compression + 2) != std::string_view::npos) return false;
  const auto groups = [](std::string_view side, bool ipv4Tail) -> int {
    if (side.empty()) return 0;
    int count = 0;
    while (!side.empty()) {
      const auto end = side.find(':');
      const auto part = side.substr(0, end);
      if (part.empty()) return -100;
      if (part.find('.') != std::string_view::npos) {
        if (!ipv4Tail || end != std::string_view::npos || !Ipv4(part)) return -100;
        return count + 2;
      }
      if (part.size() > 4 || !std::all_of(part.begin(), part.end(), [](char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
      })) return -100;
      ++count;
      if (end == std::string_view::npos) return count;
      side.remove_prefix(end + 1);
      if (side.empty()) return -100;
    }
    return count;
  };
  if (compression == std::string_view::npos) return groups(text, true) == 8;
  const int left = groups(text.substr(0, compression), false);
  const int right = groups(text.substr(compression + 2), true);
  return left >= 0 && right >= 0 && left + right < 8;
}
template <class Node>
inline Result Validate(const Node &fixture) {
  if (!fixture.dmxAddresses) return {};
  for (const auto &entry : fixture.dmxAddresses->networks) {
    const auto control = [](const std::string &value) {
      return std::any_of(value.begin(), value.end(), [](char ch) {
        return static_cast<unsigned char>(ch) < 0x20;
      });
    };
    // The canonical XML writer cannot retain character-reference whitespace in attributes.
    if (control(entry.geometry) || (entry.hostname && control(*entry.hostname)))
      return Failure(fixture, Conflict::InvalidValue, entry.geometry,
          "control characters in Network attributes cannot be serialized faithfully");
    if (entry.geometry.find_first_not_of(" \t\r\n") == std::string::npos)
      return Failure(fixture, Conflict::MissingGeometry, entry.geometry, "required geometry name is missing");
    if ((entry.ipv4 && !Ipv4(*entry.ipv4)) || (entry.subnetmask && !Ipv4(*entry.subnetmask)) ||
        (entry.ipv6 && !Ipv6(*entry.ipv6)) || (entry.dhcp && *entry.dhcp != "true" &&
        *entry.dhcp != "false" && *entry.dhcp != "on"))
      return Failure(fixture, Conflict::InvalidValue, entry.geometry, "invalid IP address or DHCP lexical value");
  }
  return {};
}
template <class Node>
inline Result CheckBindingReference(const Node &fixture, const std::string &reference) {
  if (fixture.dmxAddresses && !fixture.dmxAddresses->networks.empty() && reference.empty())
    return Failure(fixture, Conflict::InvalidBinding, {}, "Network assignments require a linked GDTF reference");
  return {};
}
// The caller supplies exact wire-geometry name counts from the selected GDTF.
// Unavailable evidence is distinct from a name proven missing or ambiguous.
using WireNames = std::unordered_map<std::string, unsigned>;
template <class Node>
inline Result CheckReferences(const Node &fixture,
    const std::optional<WireNames> &wireNames) {
  if (auto result = Validate(fixture); !result) return result;
  if (!fixture.dmxAddresses || fixture.dmxAddresses->networks.empty()) return {};
  if (!wireNames) return Failure(fixture, Conflict::UnavailableGdtf, {}, "linked GDTF cannot currently be verified");
  for (const auto &entry : fixture.dmxAddresses->networks) {
    const auto found = wireNames->find(entry.geometry);
    if (found == wireNames->end())
      return Failure(fixture, Conflict::UnknownGeometry, entry.geometry, "wire geometry does not exist in selected GDTF");
    if (found->second != 1)
      return Failure(fixture, Conflict::AmbiguousGeometry, entry.geometry, "wire geometry name is ambiguous in selected GDTF");
  }
  return {};
}
} // namespace fixture_network
