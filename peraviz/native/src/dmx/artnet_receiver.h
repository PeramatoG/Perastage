#pragma once

#include "artnet_dmx_parser.h"
#include "dmx_platform.h"
#include "dmx_universe_cache.h"
#include "udp_socket.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace peraviz::dmx {

struct ArtNetReceiverStats {
    bool running = false;
    uint32_t packets_per_second = 0;
    uint64_t total_packets = 0;
    uint64_t last_packet_us = 0;
    std::vector<uint16_t> active_universes;
};

class ArtNetReceiver {
public:
    ArtNetReceiver();
    ~ArtNetReceiver();

    bool start(const std::string &bind_ip = "0.0.0.0", uint16_t port = 6454);
    void stop();
    bool is_running() const;

    bool try_get_frame(uint16_t universe_id, DmxFrame &out_frame) const;
    ArtNetReceiverStats get_stats(uint64_t now_us, uint64_t active_window_us) const;
    std::string get_last_error() const;

private:
    void run();

    SocketSystemInitializer socket_initializer_;
    UdpSocket socket_;
    ArtNetDmxParser parser_;
    DmxUniverseCache cache_;
    std::thread worker_;

    std::atomic<bool> running_ {false};
    std::atomic<uint64_t> total_packets_ {0};
    std::atomic<uint64_t> last_packet_us_ {0};
    std::atomic<uint64_t> second_window_us_ {0};
    std::atomic<uint32_t> packets_in_window_ {0};
    std::atomic<uint32_t> packets_per_second_ {0};

    mutable std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace peraviz::dmx
