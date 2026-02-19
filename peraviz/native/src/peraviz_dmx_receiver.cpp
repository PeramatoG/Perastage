#include "peraviz_dmx_receiver.h"

#include <algorithm>
#include <chrono>

namespace godot {

void PeravizDmxReceiver::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start", "bind_ip", "port"), &PeravizDmxReceiver::start, DEFVAL(String("0.0.0.0")), DEFVAL(6454));
    ClassDB::bind_method(D_METHOD("stop"), &PeravizDmxReceiver::stop);
    ClassDB::bind_method(D_METHOD("is_running"), &PeravizDmxReceiver::is_running);
    ClassDB::bind_method(D_METHOD("get_last_error"), &PeravizDmxReceiver::get_last_error);
    ClassDB::bind_method(D_METHOD("get_active_universes", "active_window_ms"), &PeravizDmxReceiver::get_active_universes, DEFVAL(2000));
    ClassDB::bind_method(D_METHOD("get_stats"), &PeravizDmxReceiver::get_stats);
    ClassDB::bind_method(D_METHOD("get_universe_data", "universe_id"), &PeravizDmxReceiver::get_universe_data);
}

PeravizDmxReceiver::PeravizDmxReceiver()
    : receiver_(std::make_unique<peraviz::dmx::ArtNetReceiver>()) {
}

PeravizDmxReceiver::~PeravizDmxReceiver() {
    stop();
}

bool PeravizDmxReceiver::start(const String &bind_ip, int port) {
    const int safe_port = std::max(1, std::min(port, 65535));
    return receiver_->start(std::string(bind_ip.utf8().get_data()), static_cast<uint16_t>(safe_port));
}

void PeravizDmxReceiver::stop() {
    receiver_->stop();
}

bool PeravizDmxReceiver::is_running() const {
    return receiver_->is_running();
}

String PeravizDmxReceiver::get_last_error() const {
    return String(receiver_->get_last_error().c_str());
}

PackedInt32Array PeravizDmxReceiver::get_active_universes(int active_window_ms) const {
    const uint64_t safe_window_us = static_cast<uint64_t>(std::max(active_window_ms, 0)) * 1000ULL;
    const peraviz::dmx::ArtNetReceiverStats stats = receiver_->get_stats(now_microseconds(), safe_window_us);

    PackedInt32Array universe_array;
    universe_array.resize(static_cast<int64_t>(stats.active_universes.size()));
    for (int64_t i = 0; i < universe_array.size(); ++i) {
        universe_array[i] = stats.active_universes[static_cast<size_t>(i)];
    }
    return universe_array;
}

Dictionary PeravizDmxReceiver::get_stats() const {
    const uint64_t now_us = now_microseconds();
    const peraviz::dmx::ArtNetReceiverStats stats = receiver_->get_stats(now_us, 2000ULL * 1000ULL);

    Dictionary out;
    out["running"] = stats.running;
    out["packets_per_sec"] = static_cast<int64_t>(stats.packets_per_second);
    out["total_packets"] = static_cast<int64_t>(stats.total_packets);

    int64_t last_packet_ms_ago = -1;
    if (stats.last_packet_us > 0 && now_us >= stats.last_packet_us) {
        last_packet_ms_ago = static_cast<int64_t>((now_us - stats.last_packet_us) / 1000ULL);
    }
    out["last_packet_ms_ago"] = last_packet_ms_ago;
    return out;
}

PackedByteArray PeravizDmxReceiver::get_universe_data(int universe_id) const {
    PackedByteArray bytes;
    if (universe_id < 0 || universe_id > 32767) {
        return bytes;
    }

    peraviz::dmx::DmxFrame frame;
    if (!receiver_->try_get_frame(static_cast<uint16_t>(universe_id), frame)) {
        return bytes;
    }

    bytes.resize(frame.length);
    for (int64_t i = 0; i < frame.length; ++i) {
        bytes[i] = frame.data[static_cast<size_t>(i)];
    }
    return bytes;
}

uint64_t PeravizDmxReceiver::now_microseconds() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

} // namespace godot
