#include "local_ipc/local_ipc_transport.h"

#include <array>
#include <cstring>
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

namespace perastage::local_ipc {
namespace {

#if defined(_WIN32)
using SocketLength = int;
constexpr std::intptr_t kInvalidSocket = INVALID_SOCKET;
#else
using SocketLength = socklen_t;
constexpr std::intptr_t kInvalidSocket = -1;
#endif

// Initializes the platform socket runtime when required.
bool InitializeSockets() {
#if defined(_WIN32)
  WSADATA data{};
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
  return true;
#endif
}

// Closes one native socket handle.
void CloseSocket(std::intptr_t socket) {
  if (socket == kInvalidSocket)
    return;
#if defined(_WIN32)
  closesocket(static_cast<SOCKET>(socket));
#else
  close(static_cast<int>(socket));
#endif
}

// Applies bounded send and receive waits to one connected socket.
void SetSocketTimeout(std::intptr_t socket) {
#if defined(_WIN32)
  const DWORD timeout = 3000;
  setsockopt(static_cast<SOCKET>(socket), SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char *>(&timeout), sizeof(timeout));
  setsockopt(static_cast<SOCKET>(socket), SOL_SOCKET, SO_SNDTIMEO,
             reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
  const timeval timeout{3, 0};
  setsockopt(static_cast<int>(socket), SOL_SOCKET, SO_RCVTIMEO, &timeout,
             sizeof(timeout));
  setsockopt(static_cast<int>(socket), SOL_SOCKET, SO_SNDTIMEO, &timeout,
             sizeof(timeout));
#endif
}

// Sends the complete byte sequence or reports a disconnected peer.
bool SendAll(std::intptr_t socket, const std::string &bytes) {
  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const int count = send(socket, bytes.data() + sent,
                           static_cast<int>(bytes.size() - sent), 0);
    if (count <= 0)
      return false;
    sent += static_cast<std::size_t>(count);
  }
  return true;
}

// Reads one bounded newline-terminated frame.
bool ReceiveFrame(std::intptr_t socket, std::string &frame) {
  frame.clear();
  std::array<char, 4096> buffer{};
  while (frame.size() <= kMaximumMessageBytes) {
    const int count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (count <= 0)
      return false;
    frame.append(buffer.data(), static_cast<std::size_t>(count));
    const auto newline = frame.find('\n');
    if (newline != std::string::npos) {
      frame.resize(newline);
      return frame.size() <= kMaximumMessageBytes;
    }
  }
  return false;
}

} // namespace

// Stops the worker before releasing transport state.
Server::~Server() { Stop(); }

// Starts a request server bound exclusively to the IPv4 loopback address.
bool Server::Start(std::uint16_t port, RequestHandler handler,
                   std::string &error) {
  if (worker_.joinable()) {
    error = "server_already_started";
    return false;
  }
  if (!InitializeSockets()) {
    error = "socket_initialization_failed";
    return false;
  }
  listenSocket_ = static_cast<std::intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (listenSocket_ == kInvalidSocket) {
    error = "socket_creation_failed";
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (bind(listenSocket_, reinterpret_cast<sockaddr *>(&address),
           sizeof(address)) != 0 || listen(listenSocket_, 4) != 0) {
    CloseSocket(listenSocket_);
    listenSocket_ = kInvalidSocket;
    error = "loopback_bind_failed";
    return false;
  }
  SocketLength length = sizeof(address);
  getsockname(listenSocket_, reinterpret_cast<sockaddr *>(&address), &length);
  port_ = ntohs(address.sin_port);
  handler_ = std::move(handler);
  stopping_ = false;
  worker_ = std::thread(&Server::Run, this);
  return true;
}

// Stops accepting connections and waits for the worker to finish.
void Server::Stop() {
  if (!worker_.joinable())
    return;
  stopping_ = true;
  const std::intptr_t wakeSocket =
      static_cast<std::intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  sockaddr_in wakeAddress{};
  wakeAddress.sin_family = AF_INET;
  wakeAddress.sin_port = htons(port_);
  inet_pton(AF_INET, "127.0.0.1", &wakeAddress.sin_addr);
  connect(wakeSocket, reinterpret_cast<sockaddr *>(&wakeAddress),
          sizeof(wakeAddress));
  CloseSocket(wakeSocket);
  worker_.join();
  CloseSocket(listenSocket_);
  listenSocket_ = kInvalidSocket;
  handler_ = {};
  port_ = 0;
}

// Accepts and serves bounded newline-framed requests until stopped.
void Server::Run() {
  while (!stopping_) {
    sockaddr_in peer{};
    SocketLength length = sizeof(peer);
    const std::intptr_t connection = static_cast<std::intptr_t>(
        accept(listenSocket_, reinterpret_cast<sockaddr *>(&peer), &length));
    if (connection == kInvalidSocket)
      break;
    if (stopping_) {
      CloseSocket(connection);
      break;
    }
    SetSocketTimeout(connection);
    std::string request;
    std::string response;
    if (ReceiveFrame(connection, request))
      response = handler_(request);
    else
      response = R"({"schema_version":1,"ok":false,"error":{"code":"invalid_frame","message":"Request must be one newline-terminated frame of at most 65536 bytes."}})";
    response.push_back('\n');
    SendAll(connection, response);
    CloseSocket(connection);
  }
}

// Sends one bounded request to a loopback server and receives one response.
bool Exchange(std::uint16_t port, const std::string &request,
              std::string &response, std::string &error) {
  if (request.size() > kMaximumMessageBytes) {
    error = "request_too_large";
    return false;
  }
  if (!InitializeSockets()) {
    error = "socket_initialization_failed";
    return false;
  }
  const std::intptr_t socketFd =
      static_cast<std::intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (socketFd == kInvalidSocket) {
    error = "socket_creation_failed";
    return false;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (connect(socketFd, reinterpret_cast<sockaddr *>(&address),
              sizeof(address)) != 0) {
    CloseSocket(socketFd);
    error = "connection_failed";
    return false;
  }
  SetSocketTimeout(socketFd);
  const bool sent = SendAll(socketFd, request + '\n');
  const bool received = sent && ReceiveFrame(socketFd, response);
  CloseSocket(socketFd);
  if (!received)
    error = sent ? "response_failed" : "request_failed";
  return received;
}

} // namespace perastage::local_ipc
