#pragma once

#include <cstddef>
#include <cstdint>

namespace peraviz::dmx {

struct ArtNetDmxFrameView {
    uint16_t universe_id = 0;
    uint8_t sequence = 0;
    uint16_t length = 0;
    const uint8_t *data = nullptr;
};

class ArtNetDmxParser {
public:
    bool parse(const uint8_t *packet_data, size_t packet_size, ArtNetDmxFrameView &out_frame) const;
};

} // namespace peraviz::dmx
