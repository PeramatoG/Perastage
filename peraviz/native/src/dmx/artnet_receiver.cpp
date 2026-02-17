#include "artnet_receiver.h"

#include <array>
#include <chrono>

namespace peraviz::dmx {
namespace {

uint64_t now_microseconds() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

} // namespace

ArtNetReceiver::ArtNetReceiver() = default;

ArtNetReceiver::~ArtNetReceiver() {
    stop();
}

bool ArtNetReceiver::start(const std::string &bind_ip, uint16_t port) {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }

    if (!socket_initializer_.is_valid()) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = socket_initializer_.error_message();
        return false;
    }

    std::string error_message;
    if (!socket_.open_and_bind(bind_ip, port, error_message) ||
        !socket_.set_reuse_address(true, error_message) ||
        !socket_.set_non_blocking(true, error_message) ||
        !socket_.set_receive_buffer(4 * 1024 * 1024, error_message)) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = error_message;
        socket_.close();
        return false;
    }

    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&ArtNetReceiver::run, this);
    return true;
}

void ArtNetReceiver::stop() {
    running_.store(false, std::memory_order_release);
    socket_.close();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ArtNetReceiver::is_running() const {
    return running_.load(std::memory_order_acquire);
}

bool ArtNetReceiver::try_get_frame(uint16_t universe_id, DmxFrame &out_frame) const {
    return cache_.try_get_frame(universe_id, out_frame);
}

ArtNetReceiverStats ArtNetReceiver::get_stats(uint64_t now_us, uint64_t active_window_us) const {
    ArtNetReceiverStats stats;
    stats.running = is_running();
    stats.packets_per_second = packets_per_second_.load(std::memory_order_relaxed);
    stats.total_packets = total_packets_.load(std::memory_order_relaxed);
    stats.last_packet_us = last_packet_us_.load(std::memory_order_relaxed);
    stats.active_universes = cache_.get_active_universes(now_us, active_window_us);
    return stats;
}

std::string ArtNetReceiver::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void ArtNetReceiver::run() {
    std::array<uint8_t, 1024> receive_buffer {};
    while (running_.load(std::memory_order_acquire)) {
        std::string wait_error;
        const bool readable = socket_.wait_readable(10, wait_error);
        if (!wait_error.empty()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = wait_error;
            continue;
        }
        if (!readable) {
            continue;
        }

        std::string sender_ip;
        uint16_t sender_port = 0;
        std::string receive_error;
        const int bytes_read = socket_.recv_from(receive_buffer.data(), receive_buffer.size(), sender_ip, sender_port, receive_error);
        if (bytes_read < 0) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = receive_error;
            continue;
        }
        if (bytes_read == 0) {
            continue;
        }

        ArtNetDmxFrameView frame_view;
        if (!parser_.parse(receive_buffer.data(), static_cast<size_t>(bytes_read), frame_view)) {
            continue;
        }

        const uint64_t packet_time_us = now_microseconds();
        cache_.write_frame(frame_view.universe_id, frame_view.data, frame_view.length, frame_view.sequence, packet_time_us);

        total_packets_.fetch_add(1, std::memory_order_relaxed);
        last_packet_us_.store(packet_time_us, std::memory_order_relaxed);

        uint64_t window_start = second_window_us_.load(std::memory_order_relaxed);
        if (window_start == 0) {
            second_window_us_.store(packet_time_us, std::memory_order_relaxed);
            packets_in_window_.store(1, std::memory_order_relaxed);
            continue;
        }

        if (packet_time_us - window_start >= 1000000ULL) {
            packets_per_second_.store(packets_in_window_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            second_window_us_.store(packet_time_us, std::memory_order_relaxed);
            packets_in_window_.store(1, std::memory_order_relaxed);
        } else {
            packets_in_window_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

} // namespace peraviz::dmx
