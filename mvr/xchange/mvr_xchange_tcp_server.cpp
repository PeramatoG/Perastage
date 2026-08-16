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
#ifdef _WIN32
using SocketLength = int;
#else
using SocketLength = socklen_t;
#endif
// Closes a native socket descriptor on the current platform.
void CloseSocketFd(std::intptr_t fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

// Shuts down a native socket to unblock a waiting client thread.
void ShutdownSocketFd(std::intptr_t fd) {
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
void ApplySocketTimeouts(std::intptr_t fd) {
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
bool MvrXchangeTcpServer::Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback, JoinCallback joinCallback, LeaveCallback leaveCallback, CommitCallback commitCallback) {
  if (running_) return true;
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
  networkInitialized_ = true;
#endif
  listenFd_ = static_cast<std::intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (listenFd_ < 0) {
#ifdef _WIN32
    WSACleanup();
    networkInitialized_ = false;
#endif
    return false;
  }
  int opt = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(settings.port > 0 ? settings.port : 0));
  if (bind(listenFd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || listen(listenFd_, 8) != 0) {
    CloseSocketFd(listenFd_);
    listenFd_ = -1;
#ifdef _WIN32
    WSACleanup();
    networkInitialized_ = false;
#endif
    return false;
  }
  SocketLength len = sizeof(addr);
  getsockname(listenFd_, reinterpret_cast<sockaddr *>(&addr), &len);
  port_ = ntohs(addr.sin_port);
  settings_ = settings;
  resolver_ = std::move(resolver);
  commitListProvider_ = std::move(commitListProvider);
  logCallback_ = std::move(logCallback);
  joinCallback_ = std::move(joinCallback);
  leaveCallback_ = std::move(leaveCallback);
  commitCallback_ = std::move(commitCallback);
  running_ = true;
  thread_ = std::thread(&MvrXchangeTcpServer::Run, this);
  return true;
}

// Stops the listener and waits for the background accept loop to exit.
void MvrXchangeTcpServer::Stop() {
  if (!running_ && listenFd_ < 0) return;
  running_ = false;
  {
    std::lock_guard lock(clientsMutex_);
    for (const auto fd : clientFds_) ShutdownSocketFd(fd);
    clientFds_.clear();
  }
  if (thread_.joinable()) thread_.join();
  if (listenFd_ >= 0) {
    CloseSocketFd(listenFd_);
    listenFd_ = -1;
  }
  {
    std::lock_guard lock(clientThreadsMutex_);
    for (auto &clientThread : clientThreads_) {
      if (clientThread.thread.joinable()) clientThread.thread.join();
    }
    clientThreads_.clear();
  }
#ifdef _WIN32
  if (networkInitialized_) WSACleanup();
  networkInitialized_ = false;
#endif
}

// Returns whether the TCP listener is currently running.
bool MvrXchangeTcpServer::IsRunning() const { return running_; }

// Returns the active TCP port, or zero when the server is stopped.
int MvrXchangeTcpServer::Port() const { return running_ ? port_ : 0; }

// Runs the blocking accept loop on a background thread.
void MvrXchangeTcpServer::Run() {
  while (running_) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(listenFd_, &readSet);
    timeval timeout{};
    timeout.tv_usec = 200000;
    const int ready = select(static_cast<int>(listenFd_ + 1), &readSet, nullptr, nullptr, &timeout);
    if (!running_) break;
    if (ready <= 0 || !FD_ISSET(listenFd_, &readSet)) continue;
    sockaddr_in client{};
    SocketLength len = sizeof(client);
    std::intptr_t fd = static_cast<std::intptr_t>(accept(listenFd_, reinterpret_cast<sockaddr *>(&client), &len));
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
    ReapCompletedTransactions();
    auto complete = std::make_shared<std::atomic<bool>>(false);
    std::lock_guard lock(clientThreadsMutex_);
    clientThreads_.push_back({std::thread([this, fd, complete] { HandleClient(fd); *complete = true; }), complete});
  }
}

// Handles one MVR-xchange TCP client using official packet framing.
void MvrXchangeTcpServer::HandleClient(std::intptr_t clientFd) {
  sockaddr_in peer{};
  SocketLength peerLen = sizeof(peer);
  const std::string endpoint = getpeername(clientFd, reinterpret_cast<sockaddr *>(&peer), &peerLen) == 0 ? FormatEndpoint(peer) : std::string("unknown endpoint");
  std::string disconnectReason = "stop requested";
  std::vector<uint8_t> buffer;
  mvr::xchange::PacketReassembler reassembler;
  char chunk[4096];
  bool receiveRequired = true;
  while (running_ && disconnectReason != "transaction complete") {
    if (receiveRequired) {
      int n = static_cast<int>(recv(clientFd, chunk, sizeof(chunk), 0));
      if (n == 0) { disconnectReason = "recv returned 0"; break; }
      if (n < 0) { disconnectReason = "socket error"; break; }
      if (buffer.size() > mvr::xchange::kMaxBufferedInputBytes - static_cast<std::size_t>(n)) { disconnectReason = "input limit exceeded"; break; }
      buffer.insert(buffer.end(), chunk, chunk + n);
    }
    mvr::xchange::Packet decoded;
    std::string decodeError;
    const auto decodeStatus = mvr::xchange::DecodePacket(buffer, decoded, decodeError);
    if (decodeStatus == mvr::xchange::DecodeStatus::NeedMoreData) { receiveRequired = true; continue; }
    if (decodeStatus == mvr::xchange::DecodeStatus::Invalid) { disconnectReason = decodeError; break; }
    if (decodeStatus == mvr::xchange::DecodeStatus::Complete) {
      mvr::xchange::Packet complete;
      const auto reassemblyStatus = reassembler.Add(std::move(decoded), complete, decodeError);
      if (reassemblyStatus == mvr::xchange::DecodeStatus::Invalid) { disconnectReason = decodeError; break; }
      if (reassemblyStatus == mvr::xchange::DecodeStatus::NeedMoreData) { receiveRequired = buffer.empty(); continue; }
      auto packet = std::make_optional(std::move(complete));
      if (packet->type != mvr::xchange::PacketType::Json) { if (logCallback_) logCallback_("MVR-xchange received non-JSON packet from " + endpoint + "."); continue; }
      if (logCallback_) logCallback_("MVR-xchange received JSON packet from " + endpoint + ", bytes=" + std::to_string(packet->payload.size()) + ".");
      std::string line(packet->payload.begin(), packet->payload.end());
      auto msg = mvr::xchange::ParseMessage(line);
      if (!msg) { if (logCallback_) logCallback_("MVR-xchange received malformed JSON from " + endpoint + "."); SendJson(clientFd, mvr::xchange::BuildRequestError("Malformed JSON message.")); continue; }
      if (logCallback_) logCallback_("MVR-xchange received message " + msg->type + " from " + endpoint + ".");
      const std::string validationError = mvr::xchange::ValidateMessage(*msg);
      if (!validationError.empty()) {
        if (logCallback_) logCallback_("MVR-xchange rejected " + msg->type + " from " + endpoint + ": " + validationError);
        if (msg->type == "MVR_JOIN") SendJson(clientFd, mvr::xchange::BuildJoinRet(settings_.stationUuid, settings_.stationName, false, validationError));
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
      else if (msg->type == "MVR_LEAVE") { const std::string error = leaveCallback_ ? leaveCallback_(msg->stationUuid) : "Station membership is unavailable."; SendJson(clientFd, mvr::xchange::BuildLeaveRet(error.empty(), error)); disconnectReason = "explicit MVR_LEAVE"; }
      else if (msg->type == "MVR_COMMIT") { const std::string error = commitCallback_ && !msg->commits.empty() ? commitCallback_(msg->commits.front()) : "Incoming commits are not accepted."; SendJson(clientFd, mvr::xchange::BuildCommitRet(error.empty(), error)); if (logCallback_) logCallback_(error.empty() ? "MVR-xchange accepted MVR_COMMIT from " + endpoint + "." : "MVR-xchange rejected MVR_COMMIT from " + endpoint + ": " + error); }
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
bool MvrXchangeTcpServer::SendJson(std::intptr_t fd, const std::string &json) {
  const std::vector<uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  return SendPacket(fd, packet);
}

// Sends all bytes from a preframed MVR-xchange TCP packet.
bool MvrXchangeTcpServer::SendPacket(std::intptr_t fd, const std::vector<uint8_t> &packet) {
  std::size_t sent = 0;
  while (sent < packet.size()) {
    const int n = static_cast<int>(send(fd, reinterpret_cast<const char *>(packet.data() + sent), static_cast<int>(packet.size() - sent), 0));
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

// Removes a completed transaction from the active shutdown set.
void MvrXchangeTcpServer::RemoveClient(std::intptr_t fd) {
  std::lock_guard lock(clientsMutex_);
  clientFds_.erase(std::remove(clientFds_.begin(), clientFds_.end(), fd), clientFds_.end());
}

// Joins and removes completed transaction threads during normal operation.
void MvrXchangeTcpServer::ReapCompletedTransactions() {
  std::lock_guard lock(clientThreadsMutex_);
  for (auto it = clientThreads_.begin(); it != clientThreads_.end();) {
    if (!*it->complete) { ++it; continue; }
    if (it->thread.joinable()) it->thread.join();
    it = clientThreads_.erase(it);
  }
}
