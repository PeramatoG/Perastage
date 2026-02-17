#include "artnet_dmx_parser.h"

#include <algorithm>
#include <cstring>

namespace peraviz::dmx {
namespace {

constexpr size_t kArtNetHeaderSize = 8;
constexpr size_t kMinimumArtDmxPacketSize = 18;
constexpr uint16_t kOpCodeArtDmx = 0x5000;
constexpr uint16_t kMinimumProtocolVersion = 14;

uint16_t read_u16_le(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint16_t read_u16_be(const uint8_t *data) {
    return (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
}

} // namespace

bool ArtNetDmxParser::parse(const uint8_t *packet_data, size_t packet_size, ArtNetDmxFrameView &out_frame) const {
    if (packet_data == nullptr || packet_size < kMinimumArtDmxPacketSize) {
        return false;
    }

    static constexpr char kArtNetHeader[kArtNetHeaderSize] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};
    if (std::memcmp(packet_data, kArtNetHeader, kArtNetHeaderSize) != 0) {
        return false;
    }

    const uint16_t opcode = read_u16_le(packet_data + 8);
    if (opcode != kOpCodeArtDmx) {
        return false;
    }

    const uint16_t protocol_version = read_u16_be(packet_data + 10);
    if (protocol_version < kMinimumProtocolVersion) {
        return false;
    }

    const uint8_t sequence = packet_data[12];
    const uint8_t subuni = packet_data[14];
    const uint8_t net = packet_data[15];

    const uint16_t declared_length = read_u16_be(packet_data + 16);
    const uint16_t clamped_length = std::min<uint16_t>(declared_length, 512);
    if (packet_size < kMinimumArtDmxPacketSize + clamped_length) {
        return false;
    }

    out_frame.universe_id = static_cast<uint16_t>((static_cast<uint16_t>(net) << 8) | subuni);
    out_frame.sequence = sequence;
    out_frame.length = clamped_length;
    out_frame.data = packet_data + kMinimumArtDmxPacketSize;

    return true;
}

} // namespace peraviz::dmx
