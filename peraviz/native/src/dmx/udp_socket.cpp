#include "udp_socket.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

namespace peraviz::dmx {
namespace {

bool is_invalid_socket(SocketHandle socket_handle) {
#ifdef _WIN32
    return socket_handle == static_cast<SocketHandle>(INVALID_SOCKET);
#else
    return socket_handle < 0;
#endif
}

} // namespace

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open_and_bind(const std::string &bind_ip, uint16_t port, std::string &error_message) {
    close();

    const SocketHandle handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (is_invalid_socket(handle)) {
        error_message = "Failed to create UDP socket. Error=" + std::to_string(get_last_socket_error());
        return false;
    }
    socket_handle_ = handle;

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (bind_ip.empty() || bind_ip == "0.0.0.0") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, bind_ip.c_str(), &address.sin_addr) != 1) {
        error_message = "Invalid bind IPv4 address: " + bind_ip;
        close();
        return false;
    }

    if (::bind(socket_handle_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        const int error_code = get_last_socket_error();
        error_message = "Failed to bind UDP socket on " + bind_ip + ":" + std::to_string(port) +
                        ". Error=" + std::to_string(error_code);
        const std::string error_detail = describe_socket_error(error_code);
        if (!error_detail.empty()) {
            error_message += " (" + error_detail + ")";
        }
        close();
        return false;
    }

    return true;
}

void UdpSocket::close() {
    if (is_invalid_socket(socket_handle_)) {
        return;
    }
    close_socket(socket_handle_);
#ifdef _WIN32
    socket_handle_ = static_cast<SocketHandle>(INVALID_SOCKET);
#else
    socket_handle_ = -1;
#endif
}

bool UdpSocket::is_open() const {
    return !is_invalid_socket(socket_handle_);
}

bool UdpSocket::set_non_blocking(bool enabled, std::string &error_message) {
    if (!is_open()) {
        error_message = "Socket is not open";
        return false;
    }
    return set_socket_non_blocking(socket_handle_, enabled, error_message);
}

bool UdpSocket::set_receive_buffer(int bytes, std::string &error_message) {
    if (!is_open()) {
        error_message = "Socket is not open";
        return false;
    }
    return set_socket_receive_buffer(socket_handle_, bytes, error_message);
}

bool UdpSocket::set_reuse_address(bool enabled, std::string &error_message) {
    if (!is_open()) {
        error_message = "Socket is not open";
        return false;
    }
    return set_socket_reuse_address(socket_handle_, enabled, error_message);
}

bool UdpSocket::wait_readable(int timeout_ms, std::string &error_message) const {
    if (!is_open()) {
        error_message = "Socket is not open";
        return false;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_handle_, &read_set);

    timeval timeout {};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    const int result = select(static_cast<int>(socket_handle_ + 1), &read_set, nullptr, nullptr, &timeout);
    if (result < 0) {
        error_message = "select() failed. Error=" + std::to_string(get_last_socket_error());
        return false;
    }
    return result > 0 && FD_ISSET(socket_handle_, &read_set);
}

int UdpSocket::recv_from(uint8_t *buffer,
                         size_t buffer_size,
                         std::string &sender_ip,
                         uint16_t &sender_port,
                         std::string &error_message) const {
    if (!is_open()) {
        error_message = "Socket is not open";
        return -1;
    }

    sockaddr_in sender_address {};
#ifdef _WIN32
    int sender_address_size = sizeof(sender_address);
#else
    socklen_t sender_address_size = sizeof(sender_address);
#endif

    const int bytes_read = recvfrom(socket_handle_,
#ifdef _WIN32
                                    reinterpret_cast<char *>(buffer),
#else
                                    buffer,
#endif
                                    static_cast<int>(buffer_size),
                                    0,
                                    reinterpret_cast<sockaddr *>(&sender_address),
                                    &sender_address_size);
    if (bytes_read < 0) {
        const int error = get_last_socket_error();
        if (is_would_block_error(error)) {
            return 0;
        }
        error_message = "recvfrom() failed. Error=" + std::to_string(error);
        return -1;
    }

    char ip_buffer[INET_ADDRSTRLEN] = {0};
    const char *ip_result = inet_ntop(AF_INET, &sender_address.sin_addr, ip_buffer, sizeof(ip_buffer));
    sender_ip = ip_result != nullptr ? ip_buffer : std::string();
    sender_port = ntohs(sender_address.sin_port);
    return bytes_read;
}

} // namespace peraviz::dmx
