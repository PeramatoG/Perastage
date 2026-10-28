/* This file is part of Perastage. */
#pragma once
#include <optional>
#include <string>

// Geometry is a case-sensitive wire geometry name in the linked GDTF, not a UUID.
// Optional strings retain XML attribute presence and authored lexical spelling.
struct FixtureNetworkAssignment {
  std::string geometry;
  std::optional<std::string> ipv4, subnetmask, ipv6, dhcp, hostname;
  bool operator==(const FixtureNetworkAssignment &) const = default;
};
