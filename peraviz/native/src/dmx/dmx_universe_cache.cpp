#include "dmx_universe_cache.h"

#include <algorithm>
#include <cstring>

namespace peraviz::dmx {

DmxUniverseCache::DmxUniverseCache() {
    slots_.resize(32768);
}

void DmxUniverseCache::write_frame(uint16_t universe_id,
                                   const uint8_t *data,
                                   uint16_t length,
                                   uint8_t sequence,
                                   uint64_t now_us) {
    if (universe_id >= slots_.size() || data == nullptr) {
        return;
    }

    UniverseSlot *slot = get_or_create_slot(universe_id);
    if (slot == nullptr) {
        return;
    }

    const uint16_t safe_length = std::min<uint16_t>(length, 512);
    const uint8_t current_front = slot->front_index.load(std::memory_order_relaxed);
    const uint8_t next_front = static_cast<uint8_t>(1 - current_front);

    if (safe_length > 0) {
        std::memcpy(slot->buffers[next_front].data(), data, safe_length);
    }

    slot->length.store(safe_length, std::memory_order_relaxed);
    slot->last_rx_us.store(now_us, std::memory_order_relaxed);
    slot->sequence.store(sequence, std::memory_order_relaxed);
    slot->counter.fetch_add(1, std::memory_order_relaxed);
    slot->front_index.store(next_front, std::memory_order_release);
}

bool DmxUniverseCache::try_get_frame(uint16_t universe_id, DmxFrame &out_frame) const {
    if (universe_id >= slots_.size()) {
        return false;
    }

    const std::unique_ptr<UniverseSlot> &slot = slots_[universe_id];
    if (!slot) {
        return false;
    }

    const uint8_t front_index = slot->front_index.load(std::memory_order_acquire);
    const uint16_t length = slot->length.load(std::memory_order_relaxed);

    out_frame.universe_id = universe_id;
    out_frame.length = length;
    if (length > 0) {
        std::memcpy(out_frame.data.data(), slot->buffers[front_index].data(), length);
    }
    out_frame.last_rx_us = slot->last_rx_us.load(std::memory_order_relaxed);
    out_frame.counter = slot->counter.load(std::memory_order_relaxed);
    return true;
}

std::vector<uint16_t> DmxUniverseCache::get_active_universes(uint64_t now_us, uint64_t active_window_us) const {
    std::vector<uint16_t> active;
    for (size_t universe_id = 0; universe_id < slots_.size(); ++universe_id) {
        const std::unique_ptr<UniverseSlot> &slot = slots_[universe_id];
        if (!slot) {
            continue;
        }
        const uint64_t last_rx_us = slot->last_rx_us.load(std::memory_order_relaxed);
        if (last_rx_us == 0 || now_us < last_rx_us) {
            continue;
        }
        if (now_us - last_rx_us <= active_window_us) {
            active.push_back(static_cast<uint16_t>(universe_id));
        }
    }
    return active;
}

DmxUniverseCache::UniverseSlot *DmxUniverseCache::get_or_create_slot(uint16_t universe_id) {
    std::unique_ptr<UniverseSlot> &slot = slots_[universe_id];
    if (slot) {
        return slot.get();
    }

    std::lock_guard<std::mutex> lock(create_slot_mutex_);
    if (!slot) {
        slot = std::make_unique<UniverseSlot>();
    }
    return slot.get();
}

} // namespace peraviz::dmx
