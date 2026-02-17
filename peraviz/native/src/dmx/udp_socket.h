#pragma once

#include "dmx_platform.h"

#include <cstdint>
#include <string>
#include <vector>

namespace peraviz::dmx {

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;

    bool open_and_bind(const std::string &bind_ip, uint16_t port, std::string &error_message);
    void close();

    bool is_open() const;
    bool set_non_blocking(bool enabled, std::string &error_message);
    bool set_receive_buffer(int bytes, std::string &error_message);
    bool set_reuse_address(bool enabled, std::string &error_message);

    bool wait_readable(int timeout_ms, std::string &error_message) const;
    int recv_from(uint8_t *buffer,
                  size_t buffer_size,
                  std::string &sender_ip,
                  uint16_t &sender_port,
                  std::string &error_message) const;

private:
    SocketHandle socket_handle_ =
#ifdef _WIN32
        static_cast<SocketHandle>(~0ULL);
#else
        -1;
#endif
};

} // namespace peraviz::dmx
