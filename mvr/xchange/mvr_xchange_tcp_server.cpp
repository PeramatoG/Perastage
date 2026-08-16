#include "mvr_xchange_tcp_server.h"
#include "mvr_xchange_message.h"
#include "mvr_xchange_packet.h"
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t kMaxConcurrentTransactions = 16;
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


// Formats a socket address as host:port for diagnostics.
std::string FormatEndpoint(const sockaddr_in &addr) {
  char host[INET_ADDRSTRLEN]{};
  inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host));
  return std::string(host) + ":" + std::to_string(ntohs(addr.sin_port));
}

// Applies bounded send and receive timeouts to one short-lived transaction.
void ApplySocketTimeouts(int fd) {
#ifdef _WIN32
  DWORD timeout = 5000;
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
  timeval timeout{};
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}
}

// Creates an inactive TCP server wrapper.
MvrXchangeTcpServer::MvrXchangeTcpServer() = default;

// Stops the TCP server before destroying it.
MvrXchangeTcpServer::~MvrXchangeTcpServer() { Stop(); }

// Starts the MVR-xchange TCP Mode listener on the configured port.
bool MvrXchangeTcpServer::Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback, JoinCallback joinCallback) {
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
  joinCallback_ = std::move(joinCallback);
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

// Runs the blocking accept loop on a background thread.
void MvrXchangeTcpServer::Run() {
  while (running_) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = static_cast<int>(accept(listenFd_, reinterpret_cast<sockaddr *>(&client), &len));
    if (fd < 0) continue;
    {
      std::lock_guard lock(clientsMutex_);
      if (clientFds_.size() >= kMaxConcurrentTransactions) {
        CloseSocketFd(fd);
        if (logCallback_) logCallback_("MVR-xchange rejected a TCP transaction because the concurrency limit was reached.");
        continue;
      }
      clientFds_.push_back(fd);
    }
    ApplySocketTimeouts(fd);
    if (logCallback_) logCallback_("MVR-xchange TCP client connected from " + FormatEndpoint(client) + ".");
    std::lock_guard lock(clientThreadsMutex_);
    clientThreads_.emplace_back(&MvrXchangeTcpServer::HandleClient, this, fd);
  }
}

// Handles one MVR-xchange TCP client using official packet framing.
void MvrXchangeTcpServer::HandleClient(int clientFd) {
  sockaddr_in peer{};
  socklen_t peerLen = sizeof(peer);
  const std::string endpoint = getpeername(clientFd, reinterpret_cast<sockaddr *>(&peer), &peerLen) == 0 ? FormatEndpoint(peer) : std::string("unknown endpoint");
  std::string disconnectReason = "stop requested";
  std::vector<uint8_t> buffer;
  char chunk[4096];
  while (running_ && disconnectReason != "transaction complete") {
    int n = static_cast<int>(recv(clientFd, chunk, sizeof(chunk), 0));
    if (n == 0) { disconnectReason = "recv returned 0"; break; }
    if (n < 0) { disconnectReason = "socket error"; break; }
    if (buffer.size() > mvr::xchange::kMaxBufferedInputBytes - static_cast<std::size_t>(n)) { disconnectReason = "input limit exceeded"; break; }
    buffer.insert(buffer.end(), chunk, chunk + n);
    mvr::xchange::Packet decoded;
    std::string decodeError;
    const auto decodeStatus = mvr::xchange::DecodePacket(buffer, decoded, decodeError);
    if (decodeStatus == mvr::xchange::DecodeStatus::Invalid) { disconnectReason = decodeError; break; }
    if (decodeStatus == mvr::xchange::DecodeStatus::Complete) {
      auto packet = std::make_optional(std::move(decoded));
      if (packet->type != mvr::xchange::PacketType::Json) { if (logCallback_) logCallback_("MVR-xchange received non-JSON packet from " + endpoint + "."); continue; }
      if (logCallback_) logCallback_("MVR-xchange received JSON packet from " + endpoint + ", bytes=" + std::to_string(packet->payload.size()) + ".");
      std::string line(packet->payload.begin(), packet->payload.end());
      auto msg = mvr::xchange::ParseMessage(line);
      if (!msg) { if (logCallback_) logCallback_("MVR-xchange received malformed JSON from " + endpoint + "."); SendJson(clientFd, mvr::xchange::BuildRequestError("Malformed JSON message.")); continue; }
      if (logCallback_) logCallback_("MVR-xchange received message " + msg->type + " from " + endpoint + ".");
      const std::string validationError = mvr::xchange::ValidateMessage(*msg);
      if (!validationError.empty()) {
        if (logCallback_) logCallback_("MVR-xchange rejected " + msg->type + " from " + endpoint + ": " + validationError);
        if (msg->type == "MVR_JOIN") SendJson(clientFd, mvr::xchange::BuildJoinRet(false, validationError));
        else if (msg->type == "MVR_COMMIT") SendJson(clientFd, mvr::xchange::BuildCommitRet(false, validationError));
        else if (msg->type == "MVR_LEAVE") SendJson(clientFd, mvr::xchange::BuildLeaveRet(false, validationError));
        else SendJson(clientFd, mvr::xchange::BuildRequestError(validationError));
        disconnectReason = "invalid transaction";
        break;
      }
      if (msg->type == "MVR_JOIN") {
        if (logCallback_) logCallback_("MVR-xchange received MVR_JOIN from " + endpoint + ":\n  provider=" + msg->provider + "\n  station=" + msg->stationName + "\n  uuid=" + msg->stationUuid + "\n  version=" + std::to_string(msg->verMajor) + "." + std::to_string(msg->verMinor) + "\n  commits=" + std::to_string(msg->commits.size()) + "\n  files=" + std::to_string(msg->filesCount));
        MvrXchangeRemoteStation station;
        station.stationUuid = msg->stationUuid;
        station.stationName = msg->stationName;
        station.provider = msg->provider;
        station.verMajor = msg->verMajor;
        station.verMinor = msg->verMinor;
        station.ipAddress = endpoint.substr(0, endpoint.find(':'));
        station.incomingJoined = true;
        station.inventorySpecified = msg->inventoryPresence != mvr::xchange::InventoryPresence::Absent;
        station.commits = msg->commits;
        if (joinCallback_) joinCallback_(station);
        auto commits = commitListProvider_ ? commitListProvider_() : std::vector<MvrXchangeCommit>{};
        SendJson(clientFd, mvr::xchange::BuildJoinRet(settings_.stationUuid, settings_.stationName, commits));
        if (logCallback_) logCallback_("MVR-xchange sent MVR_JOIN_RET to " + endpoint + ", commits=" + std::to_string(commits.size()) + ".");
      }
      else if (msg->type == "MVR_LEAVE") { SendJson(clientFd, mvr::xchange::BuildLeaveRet()); disconnectReason = "explicit MVR_LEAVE"; RemoveClient(clientFd); CloseSocketFd(clientFd); if (logCallback_) logCallback_("MVR-xchange TCP client disconnected from " + endpoint + ", reason=" + disconnectReason + "."); return; }
      else if (msg->type == "MVR_COMMIT") { SendJson(clientFd, mvr::xchange::BuildCommitRet(true)); if (logCallback_) logCallback_("MVR-xchange sent MVR_COMMIT_RET to " + endpoint + "."); }
      else if (msg->type == "MVR_REQUEST") {
        auto commit = resolver_ ? resolver_(msg->fileUuid) : std::nullopt;
        if (!commit || commit->payload.empty()) { SendJson(clientFd, mvr::xchange::BuildRequestError("The MVR is not available on this client")); if (logCallback_) logCallback_("MVR-xchange sent MVR_REQUEST_RET error to " + endpoint + " for FileUUID=" + msg->fileUuid + "."); }
        else { const auto payloadPacket = mvr::xchange::EncodePacket(mvr::xchange::PacketType::MvrFile, commit->payload); SendPacket(clientFd, payloadPacket); if (logCallback_) logCallback_("MVR-xchange sent MVR binary payload to " + endpoint + ", FileUUID=" + commit->fileUuid + ", bytes=" + std::to_string(commit->payload.size()) + "."); }
      } else { SendJson(clientFd, mvr::xchange::BuildRequestError("Unsupported MVR-xchange message.")); if (logCallback_) logCallback_("MVR-xchange received unsupported message " + msg->type + " from " + endpoint + "."); }
      disconnectReason = "transaction complete";
      break;
    }
  }
  RemoveClient(clientFd);
  CloseSocketFd(clientFd);
  if (logCallback_) logCallback_("MVR-xchange TCP client disconnected from " + endpoint + ", reason=" + disconnectReason + ".");
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

// Removes a completed transaction from the active shutdown set.
void MvrXchangeTcpServer::RemoveClient(int fd) {
  std::lock_guard lock(clientsMutex_);
  clientFds_.erase(std::remove(clientFds_.begin(), clientFds_.end(), fd), clientFds_.end());
}
