#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mvr::xchange {

constexpr std::size_t kMaxJsonPayloadBytes = 1024 * 1024;
constexpr std::size_t kMaxMvrPayloadBytes = 512 * 1024 * 1024;
constexpr std::size_t kMaxBufferedInputBytes = kMaxMvrPayloadBytes + 28;

enum class PacketType : uint32_t { Json = 0, MvrFile = 1 };

struct Packet {
  PacketType type = PacketType::Json;
  std::vector<uint8_t> payload;
};

enum class DecodeStatus { NeedMoreData, Complete, Invalid };

std::vector<uint8_t> EncodePacket(PacketType type, const std::vector<uint8_t> &payload);
std::optional<Packet> TryDecodePacket(std::vector<uint8_t> &buffer);
DecodeStatus DecodePacket(std::vector<uint8_t> &buffer, Packet &packet, std::string &error);

}
