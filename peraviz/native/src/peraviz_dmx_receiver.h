#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "dmx/artnet_receiver.h"

#include <memory>

namespace godot {

class PeravizDmxReceiver : public RefCounted {
    GDCLASS(PeravizDmxReceiver, RefCounted)

protected:
    static void _bind_methods();

public:
    PeravizDmxReceiver();
    ~PeravizDmxReceiver() override;

    bool start(const String &bind_ip = "0.0.0.0", int port = 6454);
    void stop();
    bool is_running() const;
    String get_last_error() const;

    PackedInt32Array get_active_universes(int active_window_ms = 2000) const;
    Dictionary get_stats() const;
    PackedByteArray get_universe_data(int universe_id) const;

private:
    static uint64_t now_microseconds();

    std::unique_ptr<peraviz::dmx::ArtNetReceiver> receiver_;
};

} // namespace godot
