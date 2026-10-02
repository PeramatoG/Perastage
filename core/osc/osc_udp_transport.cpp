#include "osc/osc_udp_transport.h"

#include <array>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace perastage::osc {
namespace {
#if defined(_WIN32)
using SocketLength = int;
constexpr std::intptr_t kInvalidSocket = INVALID_SOCKET;
#else
using SocketLength = socklen_t;
constexpr std::intptr_t kInvalidSocket = -1;
#endif

// Initializes platform networking when Windows requires it.
bool InitializeSockets() {
#if defined(_WIN32)
  WSADATA data{};
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
  return true;
#endif
}

// Closes a native UDP socket.
void CloseSocket(std::intptr_t socket) {
#if defined(_WIN32)
  closesocket(static_cast<SOCKET>(socket));
#else
  close(static_cast<int>(socket));
#endif
}

} // namespace

// Stops the worker before transport state is released.
UdpServer::~UdpServer() { Stop(); }

// Starts a UDP listener bound exclusively to IPv4 loopback.
bool UdpServer::Start(std::uint16_t port, PacketHandler handler,
                      std::string &error) {
  if (worker_.joinable()) {
    error = "server_already_started";
    return false;
  }
  if (!handler || !InitializeSockets()) {
    error = "socket_initialization_failed";
    return false;
  }
  socket_ = static_cast<std::intptr_t>(socket(AF_INET, SOCK_DGRAM, 0));
  if (socket_ == kInvalidSocket) {
    error = "socket_creation_failed";
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (bind(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
      0) {
    CloseSocket(socket_);
    socket_ = kInvalidSocket;
    error = "loopback_bind_failed";
    return false;
  }
  SocketLength length = sizeof(address);
  getsockname(socket_, reinterpret_cast<sockaddr *>(&address), &length);
  port_ = ntohs(address.sin_port);
  handler_ = std::move(handler);
  stopping_ = false;
  worker_ = std::thread(&UdpServer::Run, this);
  return true;
}

// Stops the listener and waits for its worker.
void UdpServer::Stop() {
  if (!worker_.joinable())
    return;
  stopping_ = true;
  const std::intptr_t wake =
      static_cast<std::intptr_t>(socket(AF_INET, SOCK_DGRAM, 0));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  const char byte = 0;
  sendto(wake, &byte, 1, 0, reinterpret_cast<sockaddr *>(&address),
         sizeof(address));
  CloseSocket(wake);
  worker_.join();
  CloseSocket(socket_);
  socket_ = kInvalidSocket;
  handler_ = {};
  port_ = 0;
}

// Receives bounded datagrams until shutdown.
void UdpServer::Run() {
  std::array<std::uint8_t, kMaximumPacketBytes + 1> buffer{};
  while (!stopping_) {
    const int count =
        recvfrom(socket_, reinterpret_cast<char *>(buffer.data()),
                 static_cast<int>(buffer.size()), 0, nullptr, nullptr);
    if (count <= 0)
      continue;
    if (stopping_)
      break;
    if (static_cast<std::size_t>(count) <= kMaximumPacketBytes)
      handler_(std::span<const std::uint8_t>(buffer.data(), count));
  }
}

} // namespace perastage::osc
