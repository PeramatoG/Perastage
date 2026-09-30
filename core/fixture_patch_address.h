#pragma once

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace perastage::patch {

struct FixturePatchAddress {
  int universe = 0;
  int channel = 0;

  bool operator==(const FixturePatchAddress &) const = default;
};

// Parses the canonical stored fixture address form "universe.channel".
inline std::optional<FixturePatchAddress>
ParseFixturePatchAddress(std::string_view address) {
  const std::size_t separator = address.find('.');
  if (separator == std::string_view::npos || separator == 0 ||
      separator + 1 == address.size() ||
      address.find('.', separator + 1) != std::string_view::npos)
    return std::nullopt;

  FixturePatchAddress result;
  const auto parsePositive = [](std::string_view text, int &value) {
    if (text.empty())
      return false;
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} &&
           parsed.ptr == text.data() + text.size() && value > 0;
  };
  if (!parsePositive(address.substr(0, separator), result.universe) ||
      !parsePositive(address.substr(separator + 1), result.channel) ||
      result.channel > 512)
    return std::nullopt;
  return result;
}

} // namespace perastage::patch
