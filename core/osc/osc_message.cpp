#include "osc/osc_message.h"

#include <bit>
#include <utility>

namespace perastage::osc {
namespace {

// Reads one OSC string and advances across its four-byte padding.
bool ReadString(std::span<const std::uint8_t> packet, std::size_t &offset,
                std::string &value) {
  if (offset >= packet.size())
    return false;
  const std::size_t start = offset;
  while (offset < packet.size() && packet[offset] != 0)
    ++offset;
  if (offset == packet.size())
    return false;
  value.assign(reinterpret_cast<const char *>(packet.data() + start),
               offset - start);
  const std::size_t paddedEnd = (offset + 4) & ~std::size_t{3};
  if (paddedEnd > packet.size())
    return false;
  while (offset < paddedEnd)
    if (packet[offset++] != 0)
      return false;
  return true;
}

// Reads one big-endian OSC 32-bit word.
bool ReadWord(std::span<const std::uint8_t> packet, std::size_t &offset,
              std::uint32_t &word) {
  if (packet.size() - offset < 4)
    return false;
  word = (static_cast<std::uint32_t>(packet[offset]) << 24) |
         (static_cast<std::uint32_t>(packet[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(packet[offset + 2]) << 8) |
         static_cast<std::uint32_t>(packet[offset + 3]);
  offset += 4;
  return true;
}

} // namespace

// Parses one bounded OSC 1.0 message without accepting bundles.
bool ParseMessage(std::span<const std::uint8_t> packet, Message &message,
                  std::string &error) {
  message = {};
  if (packet.empty() || packet.size() > kMaximumPacketBytes ||
      packet.size() % 4 != 0) {
    error = "invalid_packet_size";
    return false;
  }
  std::size_t offset = 0;
  if (!ReadString(packet, offset, message.address) || message.address.empty() ||
      message.address.front() != '/') {
    error = message.address == "#bundle" ? "bundles_unsupported"
                                         : "invalid_address";
    return false;
  }
  std::string tags;
  if (!ReadString(packet, offset, tags) || tags.empty() ||
      tags.front() != ',') {
    error = "invalid_type_tags";
    return false;
  }
  for (std::size_t index = 1; index < tags.size(); ++index) {
    std::uint32_t word = 0;
    switch (tags[index]) {
    case 'i':
      if (!ReadWord(packet, offset, word)) {
        error = "truncated_argument";
        return false;
      }
      message.arguments.emplace_back(static_cast<std::int32_t>(word));
      break;
    case 'f':
      if (!ReadWord(packet, offset, word)) {
        error = "truncated_argument";
        return false;
      }
      message.arguments.emplace_back(std::bit_cast<float>(word));
      break;
    case 's': {
      std::string value;
      if (!ReadString(packet, offset, value)) {
        error = "truncated_argument";
        return false;
      }
      message.arguments.emplace_back(std::move(value));
      break;
    }
    case 'T':
      message.arguments.emplace_back(true);
      break;
    case 'F':
      message.arguments.emplace_back(false);
      break;
    default:
      error = "unsupported_type_tag";
      return false;
    }
  }
  if (offset != packet.size()) {
    error = "trailing_packet_data";
    return false;
  }
  return true;
}

} // namespace perastage::osc
