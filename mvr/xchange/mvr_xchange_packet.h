#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mvr::xchange {

constexpr std::size_t kMaxJsonPayloadBytes = 1024 * 1024;
constexpr std::size_t kMaxMvrPayloadBytes = 512 * 1024 * 1024;
constexpr std::size_t kPacketHeaderBytes = 28;
constexpr std::uint32_t kMaxPackageCount = 1024;
constexpr std::size_t kMaxBufferedInputBytes = kMaxMvrPayloadBytes + kPacketHeaderBytes * kMaxPackageCount;

enum class PacketType : uint32_t { Json = 0, MvrFile = 1 };

struct Packet {
  PacketType type = PacketType::Json;
  std::uint32_t packageNumber = 0;
  std::uint32_t packageCount = 1;
  std::vector<uint8_t> payload;
};

enum class DecodeStatus { NeedMoreData, Complete, Invalid };

std::vector<uint8_t> EncodePacket(PacketType type, const std::vector<uint8_t> &payload);
std::optional<Packet> TryDecodePacket(std::vector<uint8_t> &buffer);
DecodeStatus DecodePacket(std::vector<uint8_t> &buffer, Packet &packet, std::string &error);

class PacketReassembler {
public:
  DecodeStatus Add(Packet fragment, Packet &complete, std::string &error);
  void Reset();

private:
  PacketType type_ = PacketType::Json;
  std::uint32_t packageCount_ = 0;
  std::uint32_t nextPackageNumber_ = 0;
  std::vector<std::uint8_t> payload_;
};

}
