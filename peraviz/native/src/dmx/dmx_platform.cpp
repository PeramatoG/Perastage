#include "dmx_platform.h"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace peraviz::dmx {

SocketSystemInitializer::SocketSystemInitializer() {
#ifdef _WIN32
    WSADATA wsa_data;
    const int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        error_message_ = "WSAStartup failed with code " + std::to_string(result);
        return;
    }
#endif
    valid_ = true;
}

SocketSystemInitializer::~SocketSystemInitializer() {
#ifdef _WIN32
    if (valid_) {
        WSACleanup();
    }
#endif
}

bool SocketSystemInitializer::is_valid() const {
    return valid_;
}

const std::string &SocketSystemInitializer::error_message() const {
    return error_message_;
}

bool set_socket_non_blocking(SocketHandle socket_handle, bool enabled, std::string &error_message) {
#ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    if (ioctlsocket(static_cast<SOCKET>(socket_handle), FIONBIO, &mode) != 0) {
        error_message = "Failed to configure non-blocking mode. Error=" + std::to_string(get_last_socket_error());
        return false;
    }
    return true;
#else
    const int flags = fcntl(socket_handle, F_GETFL, 0);
    if (flags < 0) {
        error_message = "Failed to get socket flags: " + std::string(std::strerror(errno));
        return false;
    }
    const int updated_flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(socket_handle, F_SETFL, updated_flags) != 0) {
        error_message = "Failed to set socket flags: " + std::string(std::strerror(errno));
        return false;
    }
    return true;
#endif
}

bool set_socket_receive_buffer(SocketHandle socket_handle, int bytes, std::string &error_message) {
#ifdef _WIN32
    const char *buffer_ptr = reinterpret_cast<const char *>(&bytes);
    const int buffer_size = sizeof(bytes);
#else
    const void *buffer_ptr = &bytes;
    const socklen_t buffer_size = sizeof(bytes);
#endif
    if (setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, buffer_ptr, buffer_size) != 0) {
        error_message = "Failed to set SO_RCVBUF. Error=" + std::to_string(get_last_socket_error());
        return false;
    }
    return true;
}

bool set_socket_reuse_address(SocketHandle socket_handle, bool enabled, std::string &error_message) {
    const int flag = enabled ? 1 : 0;
#ifdef _WIN32
    const char *buffer_ptr = reinterpret_cast<const char *>(&flag);
    const int buffer_size = sizeof(flag);
#else
    const void *buffer_ptr = &flag;
    const socklen_t buffer_size = sizeof(flag);
#endif
    if (setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, buffer_ptr, buffer_size) != 0) {
        error_message = "Failed to set SO_REUSEADDR. Error=" + std::to_string(get_last_socket_error());
        return false;
    }
    return true;
}

void close_socket(SocketHandle socket_handle) {
#ifdef _WIN32
    if (socket_handle != INVALID_SOCKET) {
        closesocket(static_cast<SOCKET>(socket_handle));
    }
#else
    if (socket_handle >= 0) {
        close(socket_handle);
    }
#endif
}

bool is_would_block_error(int error_code) {
#ifdef _WIN32
    return error_code == WSAEWOULDBLOCK;
#else
    return error_code == EWOULDBLOCK || error_code == EAGAIN;
#endif
}

int get_last_socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}


std::string describe_socket_error(int error_code) {
#ifdef _WIN32
    switch (error_code) {
    case WSAEADDRINUSE:
        return "Address/port already in use (WSAEADDRINUSE)";
    case WSAEACCES:
        return "Permission denied to bind socket (WSAEACCES)";
    case WSAEADDRNOTAVAIL:
        return "Requested bind address is not available (WSAEADDRNOTAVAIL)";
    default:
        return "";
    }
#else
    switch (error_code) {
    case EADDRINUSE:
        return "Address/port already in use (EADDRINUSE)";
    case EACCES:
        return "Permission denied to bind socket (EACCES)";
    case EADDRNOTAVAIL:
        return "Requested bind address is not available (EADDRNOTAVAIL)";
    default:
        return "";
    }
#endif
}

} // namespace peraviz::dmx
