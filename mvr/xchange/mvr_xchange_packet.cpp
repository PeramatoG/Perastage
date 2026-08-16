#include "mvr_xchange_packet.h"
#include <algorithm>

namespace mvr::xchange {
namespace {
constexpr uint32_t kHeader = 778682;
constexpr uint32_t kVersion = 1;
constexpr std::size_t kPacketHeaderSize = 28;

// Appends a 32-bit integer in network byte order.
void AppendU32(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  out.push_back(static_cast<uint8_t>(value & 0xff));
}

// Appends a 64-bit integer in network byte order.
void AppendU64(std::vector<uint8_t> &out, uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8)
    out.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
}

// Reads a 32-bit integer in network byte order.
uint32_t ReadU32(const std::vector<uint8_t> &buffer, std::size_t offset) {
  return (static_cast<uint32_t>(buffer[offset]) << 24) |
         (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
         (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
         static_cast<uint32_t>(buffer[offset + 3]);
}

// Reads a 64-bit integer in network byte order.
uint64_t ReadU64(const std::vector<uint8_t> &buffer, std::size_t offset) {
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i)
    value = (value << 8) | buffer[offset + i];
  return value;
}
}

// Encodes a single-package MVR-xchange TCP packet.
std::vector<uint8_t> EncodePacket(PacketType type, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> out;
  out.reserve(kPacketHeaderSize + payload.size());
  AppendU32(out, kHeader);
  AppendU32(out, kVersion);
  AppendU32(out, 0);
  AppendU32(out, 1);
  AppendU32(out, static_cast<uint32_t>(type));
  AppendU64(out, payload.size());
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

// Decodes the first complete MVR-xchange TCP packet from a byte buffer.
std::optional<Packet> TryDecodePacket(std::vector<uint8_t> &buffer) {
  Packet packet;
  std::string error;
  if (DecodePacket(buffer, packet, error) != DecodeStatus::Complete) return std::nullopt;
  return packet;
}

// Validates and decodes one bounded, single-package TCP Mode packet.
DecodeStatus DecodePacket(std::vector<uint8_t> &buffer, Packet &packet, std::string &error) {
  if (buffer.size() < kPacketHeaderSize) return DecodeStatus::NeedMoreData;
  if (ReadU32(buffer, 0) != kHeader || ReadU32(buffer, 4) != kVersion) {
    error = "Invalid package header or version.";
    return DecodeStatus::Invalid;
  }
  const uint32_t packageNumber = ReadU32(buffer, 8);
  const uint32_t packageCount = ReadU32(buffer, 12);
  const uint32_t rawType = ReadU32(buffer, 16);
  if (packageNumber != 0 || packageCount != 1) {
    error = "Multipart packages are not supported; the transaction was rejected before reassembly.";
    return DecodeStatus::Invalid;
  }
  if (rawType > static_cast<uint32_t>(PacketType::MvrFile)) {
    error = "Invalid package payload type.";
    return DecodeStatus::Invalid;
  }
  const auto type = static_cast<PacketType>(rawType);
  const uint64_t payloadLength = ReadU64(buffer, 20);
  const uint64_t limit = type == PacketType::Json ? kMaxJsonPayloadBytes : kMaxMvrPayloadBytes;
  if (payloadLength > limit || payloadLength > static_cast<uint64_t>(SIZE_MAX - kPacketHeaderSize)) {
    error = "Package payload exceeds the configured limit.";
    return DecodeStatus::Invalid;
  }
  const std::size_t total = kPacketHeaderSize + static_cast<std::size_t>(payloadLength);
  if (buffer.size() < total) return DecodeStatus::NeedMoreData;
  packet.type = type;
  packet.payload.assign(buffer.begin() + kPacketHeaderSize, buffer.begin() + total);
  buffer.erase(buffer.begin(), buffer.begin() + total);
  return DecodeStatus::Complete;
}

}
