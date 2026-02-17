#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace peraviz::dmx {

struct DmxFrame {
    uint16_t universe_id = 0;
    uint16_t length = 0;
    std::array<uint8_t, 512> data {};
    uint64_t last_rx_us = 0;
    uint32_t counter = 0;
};

class DmxUniverseCache {
public:
    DmxUniverseCache();

    void write_frame(uint16_t universe_id,
                     const uint8_t *data,
                     uint16_t length,
                     uint8_t sequence,
                     uint64_t now_us);

    bool try_get_frame(uint16_t universe_id, DmxFrame &out_frame) const;
    std::vector<uint16_t> get_active_universes(uint64_t now_us, uint64_t active_window_us) const;

private:
    struct UniverseSlot {
        std::array<std::array<uint8_t, 512>, 2> buffers {};
        std::atomic<uint8_t> front_index {0};
        std::atomic<uint16_t> length {0};
        std::atomic<uint64_t> last_rx_us {0};
        std::atomic<uint32_t> counter {0};
        std::atomic<uint8_t> sequence {0};
    };

    UniverseSlot *get_or_create_slot(uint16_t universe_id);

    std::vector<std::unique_ptr<UniverseSlot>> slots_;
    mutable std::mutex create_slot_mutex_;
};

} // namespace peraviz::dmx
