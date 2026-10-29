/* This file is part of Perastage. */
#pragma once
#include "fixture_network_assignment.h"
#include <optional>
#include <string>
#include <vector>

// Ordered standard MVR assignments. An absent break attribute means break 0.
struct FixtureDmxAddress {
  std::optional<std::string> breakId;
  std::string value;
  bool operator==(const FixtureDmxAddress &) const = default;
};
struct FixtureDmxAddresses {
  std::vector<FixtureDmxAddress> entries;
  // MVR orders all DMX Address children before ordered Network children.
  std::vector<FixtureNetworkAssignment> networks;
  bool operator==(const FixtureDmxAddresses &) const = default;
};
