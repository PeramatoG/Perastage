#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace perastage::osc {

using Argument = std::variant<std::int32_t, float, std::string, bool>;

struct Message {
  std::string address;
  std::vector<Argument> arguments;
};

inline constexpr std::size_t kMaximumPacketBytes = 4096;

// Parses one bounded OSC 1.0 message without accepting bundles.
bool ParseMessage(std::span<const std::uint8_t> packet, Message &message,
                  std::string &error);

} // namespace perastage::osc
