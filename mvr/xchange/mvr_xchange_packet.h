#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace mvr::xchange {

enum class PacketType : uint32_t { Json = 0, MvrFile = 1 };

struct Packet {
  PacketType type = PacketType::Json;
  std::vector<uint8_t> payload;
};

std::vector<uint8_t> EncodePacket(PacketType type, const std::vector<uint8_t> &payload);
std::optional<Packet> TryDecodePacket(std::vector<uint8_t> &buffer);

}
