#pragma once

#include <cstdint>
#include <string>

namespace peraviz::dmx {

#ifdef _WIN32
using SocketHandle = uintptr_t;
#else
using SocketHandle = int;
#endif

class SocketSystemInitializer {
public:
    SocketSystemInitializer();
    ~SocketSystemInitializer();

    SocketSystemInitializer(const SocketSystemInitializer &) = delete;
    SocketSystemInitializer &operator=(const SocketSystemInitializer &) = delete;

    bool is_valid() const;
    const std::string &error_message() const;

private:
    bool valid_ = false;
    std::string error_message_;
};

bool set_socket_non_blocking(SocketHandle socket_handle, bool enabled, std::string &error_message);
bool set_socket_receive_buffer(SocketHandle socket_handle, int bytes, std::string &error_message);
bool set_socket_reuse_address(SocketHandle socket_handle, bool enabled, std::string &error_message);
void close_socket(SocketHandle socket_handle);
bool is_would_block_error(int error_code);
int get_last_socket_error();
std::string describe_socket_error(int error_code);

} // namespace peraviz::dmx
