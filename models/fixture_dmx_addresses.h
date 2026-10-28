/* This file is part of Perastage. */
#pragma once
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
  bool operator==(const FixtureDmxAddresses &) const = default;
};
