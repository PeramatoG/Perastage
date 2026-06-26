#include "mvr_xchange_tcp_server.h"
#include "mvr_xchange_message.h"
#include "mvr_xchange_packet.h"
#include <algorithm>
#include <cstring>
#include <wx/log.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
// Closes a native socket descriptor on the current platform.
void CloseSocketFd(int fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

// Shuts down a native socket to unblock a waiting client thread.
void ShutdownSocketFd(int fd) {
#ifdef _WIN32
  shutdown(fd, SD_BOTH);
#else
  shutdown(fd, SHUT_RDWR);
#endif
}
}

// Creates an inactive TCP server wrapper.
MvrXchangeTcpServer::MvrXchangeTcpServer() = default;

// Stops the TCP server before destroying it.
MvrXchangeTcpServer::~MvrXchangeTcpServer() { Stop(); }

// Starts the MVR-xchange TCP Mode listener on the configured port.
bool MvrXchangeTcpServer::Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback) {
  if (running_) return true;
#ifdef _WIN32
  WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
  listenFd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
  if (listenFd_ < 0) return false;
  int opt = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(settings.port > 0 ? settings.port : 0));
  if (bind(listenFd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || listen(listenFd_, 8) != 0) {
    CloseSocketFd(listenFd_);
    listenFd_ = -1;
    return false;
  }
  socklen_t len = sizeof(addr);
  getsockname(listenFd_, reinterpret_cast<sockaddr *>(&addr), &len);
  port_ = ntohs(addr.sin_port);
  settings_ = settings;
  resolver_ = std::move(resolver);
  commitListProvider_ = std::move(commitListProvider);
  logCallback_ = std::move(logCallback);
  running_ = true;
  thread_ = std::thread(&MvrXchangeTcpServer::Run, this);
  return true;
}

// Stops the listener and waits for the background accept loop to exit.
void MvrXchangeTcpServer::Stop() {
  if (!running_ && listenFd_ < 0) return;
  running_ = false;
  if (listenFd_ >= 0) {
    CloseSocketFd(listenFd_);
    listenFd_ = -1;
  }
  {
    std::lock_guard lock(clientsMutex_);
    for (int fd : clientFds_) ShutdownSocketFd(fd);
    clientFds_.clear();
  }
  if (thread_.joinable()) thread_.join();
  {
    std::lock_guard lock(clientThreadsMutex_);
    for (auto &clientThread : clientThreads_) {
      if (clientThread.joinable()) clientThread.join();
    }
    clientThreads_.clear();
  }
#ifdef _WIN32
  WSACleanup();
#endif
}

// Returns whether the TCP listener is currently running.
bool MvrXchangeTcpServer::IsRunning() const { return running_; }

// Returns the active TCP port, or zero when the server is stopped.
int MvrXchangeTcpServer::Port() const { return running_ ? port_ : 0; }

// Broadcasts a commit announcement to currently connected TCP clients.
void MvrXchangeTcpServer::BroadcastCommit(const MvrXchangeCommit &commit) {
  const std::string message = mvr::xchange::BuildCommit(commit);
  const std::vector<uint8_t> payload(message.begin(), message.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  std::vector<int> clients;
  {
    std::lock_guard lock(clientsMutex_);
    clients = clientFds_;
  }
  for (int fd : clients) SendPacket(fd, packet);
  if (logCallback_) logCallback_("Published MVR revision " + commit.fileUuid + " to " + std::to_string(clients.size()) + " connected MVR-xchange client(s).");
}

// Runs the blocking accept loop on a background thread.
void MvrXchangeTcpServer::Run() {
  while (running_) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = static_cast<int>(accept(listenFd_, reinterpret_cast<sockaddr *>(&client), &len));
    if (fd < 0) continue;
    std::lock_guard lock(clientThreadsMutex_);
    clientThreads_.emplace_back(&MvrXchangeTcpServer::HandleClient, this, fd);
  }
}

// Handles one MVR-xchange TCP client using official packet framing.
void MvrXchangeTcpServer::HandleClient(int clientFd) {
  AddClient(clientFd);
  if (logCallback_) logCallback_("MVR-xchange client connected.");
  std::vector<uint8_t> buffer;
  char chunk[4096];
  while (running_) {
    int n = static_cast<int>(recv(clientFd, chunk, sizeof(chunk), 0));
    if (n <= 0) break;
    buffer.insert(buffer.end(), chunk, chunk + n);
    while (auto packet = mvr::xchange::TryDecodePacket(buffer)) {
      if (packet->type != mvr::xchange::PacketType::Json) continue;
      std::string line(packet->payload.begin(), packet->payload.end());
      auto msg = mvr::xchange::ParseMessage(line);
      if (!msg) { SendJson(clientFd, mvr::xchange::BuildRequestError("Malformed JSON message.")); continue; }
      if (msg->type == "MVR_JOIN") SendJson(clientFd, mvr::xchange::BuildJoinRet(settings_.stationUuid, settings_.stationName, commitListProvider_ ? commitListProvider_() : std::vector<MvrXchangeCommit>{}));
      else if (msg->type == "MVR_LEAVE") { SendJson(clientFd, mvr::xchange::BuildLeaveRet()); RemoveClient(clientFd); CloseSocketFd(clientFd); return; }
      else if (msg->type == "MVR_COMMIT") SendJson(clientFd, mvr::xchange::BuildCommitRet(true));
      else if (msg->type == "MVR_REQUEST") {
        auto commit = resolver_ ? resolver_(msg->fileUuid) : std::nullopt;
        if (!commit) SendJson(clientFd, mvr::xchange::BuildRequestError("The MVR is not available on this client"));
        else { const auto payloadPacket = mvr::xchange::EncodePacket(mvr::xchange::PacketType::MvrFile, commit->payload); SendPacket(clientFd, payloadPacket); }
      } else SendJson(clientFd, mvr::xchange::BuildRequestError("Unsupported MVR-xchange message."));
    }
  }
  RemoveClient(clientFd);
  CloseSocketFd(clientFd);
}

// Sends one JSON message with official MVR-xchange TCP packet framing.
bool MvrXchangeTcpServer::SendJson(int fd, const std::string &json) {
  const std::vector<uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  return SendPacket(fd, packet);
}

// Sends all bytes from a preframed MVR-xchange TCP packet.
bool MvrXchangeTcpServer::SendPacket(int fd, const std::vector<uint8_t> &packet) {
  std::size_t sent = 0;
  while (sent < packet.size()) {
    const int n = static_cast<int>(send(fd, reinterpret_cast<const char *>(packet.data() + sent), static_cast<int>(packet.size() - sent), 0));
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

// Tracks a connected TCP client for commit broadcasts.
void MvrXchangeTcpServer::AddClient(int fd) {
  std::lock_guard lock(clientsMutex_);
  clientFds_.push_back(fd);
}

// Removes a disconnected TCP client from commit broadcasts.
void MvrXchangeTcpServer::RemoveClient(int fd) {
  std::lock_guard lock(clientsMutex_);
  clientFds_.erase(std::remove(clientFds_.begin(), clientFds_.end(), fd), clientFds_.end());
}
