#include "mvr_xchange_tcp_client.h"
#include "mvr_xchange_message.h"
#include "mvr_xchange_packet.h"
#include "../../core/uuidutils.h"
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
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


// Sets a socket into blocking or non-blocking mode.
bool SetSocketBlocking(int fd, bool blocking) {
#ifdef _WIN32
  u_long mode = blocking ? 0 : 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  const int updated = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
  return fcntl(fd, F_SETFL, updated) == 0;
#endif
}

// Waits for a non-blocking connection to complete or time out.
bool WaitForConnect(int fd) {
  fd_set writeSet;
  FD_ZERO(&writeSet);
  FD_SET(fd, &writeSet);
  timeval timeout{};
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  if (select(fd + 1, nullptr, &writeSet, nullptr, &timeout) <= 0) return false;
  int error = 0;
#ifdef _WIN32
  int len = sizeof(error);
#else
  socklen_t len = sizeof(error);
#endif
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &len) != 0) return false;
  return error == 0;
}

// Applies short send and receive timeouts to an outgoing client socket.
void ApplySocketTimeouts(int fd) {
#ifdef _WIN32
  DWORD timeout = 2000;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
  timeval timeout{};
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

// Formats the display name used in outgoing connection diagnostics.
std::string StationDisplayName(const MvrXchangeRemoteStation &station) {
  if (!station.stationName.empty()) return station.stationName;
  if (!station.serviceInstanceName.empty()) return station.serviceInstanceName;
  return station.ipAddress + ":" + std::to_string(station.port);
}
}

// Sends an outgoing MVR_JOIN and parses the remote station's MVR_JOIN_RET response.
bool MvrXchangeTcpClient::SendJoin(const MvrXchangeRemoteStation &station, const MvrXchangeSettings &settings, const std::vector<MvrXchangeCommit> &localCommits, MvrXchangeRemoteStation &joinedStation, LogCallback logCallback) {
  int fd = -1;
  if (!Connect(station, fd, logCallback)) return false;
  if (logCallback) logCallback("MVR-xchange sent outgoing MVR_JOIN to " + StationDisplayName(station) + ", local commits=" + std::to_string(localCommits.size()) + ".");
  const bool sent = SendJson(fd, mvr::xchange::BuildJoin(settings.stationUuid, settings.stationName, localCommits));
  std::string response;
  const bool received = sent && ReceiveJson(fd, response);
  CloseSocketFd(fd);
  if (!sent) { if (logCallback) logCallback("MVR-xchange outgoing MVR_JOIN failed while sending to " + StationDisplayName(station) + "."); return false; }
  if (!received) { if (logCallback) logCallback("MVR-xchange outgoing MVR_JOIN failed because no valid response was received from " + StationDisplayName(station) + "."); return false; }
  auto message = mvr::xchange::ParseMessage(response);
  if (!message || message->type != "MVR_JOIN_RET") { if (logCallback) logCallback("MVR-xchange outgoing MVR_JOIN received an unexpected response from " + StationDisplayName(station) + "."); return false; }
  if (!message->ok) { if (logCallback) logCallback("MVR-xchange outgoing MVR_JOIN was rejected by " + StationDisplayName(station) + ": " + message->text); return false; }
  joinedStation = station;
  joinedStation.stationUuid = message->stationUuid.empty() ? station.stationUuid : message->stationUuid;
  joinedStation.stationName = message->stationName.empty() ? station.stationName : message->stationName;
  joinedStation.provider = message->provider;
  joinedStation.verMajor = message->verMajor;
  joinedStation.verMinor = message->verMinor;
  joinedStation.commits = message->commits;
  joinedStation.outgoingJoined = true;
  if (logCallback) logCallback("MVR-xchange received MVR_JOIN_RET from " + StationDisplayName(joinedStation) + ", ok=true, remote commits=" + std::to_string(joinedStation.commits.size()) + ".");
  return true;
}

// Sends one outgoing MVR_COMMIT announcement to a joined remote station.
bool MvrXchangeTcpClient::SendCommit(const MvrXchangeRemoteStation &station, const MvrXchangeCommit &commit, LogCallback logCallback) {
  int fd = -1;
  if (!Connect(station, fd, logCallback)) return false;
  const bool sent = SendJson(fd, mvr::xchange::BuildCommit(commit));
  std::string response;
  const bool received = sent && ReceiveJson(fd, response);
  CloseSocketFd(fd);
  if (!sent) { if (logCallback) logCallback("MVR-xchange failed to send MVR_COMMIT to " + StationDisplayName(station) + "."); return false; }
  if (!received) { if (logCallback) logCallback("MVR-xchange sent MVR_COMMIT to " + StationDisplayName(station) + " but did not receive MVR_COMMIT_RET."); return false; }
  auto message = mvr::xchange::ParseMessage(response);
  const bool ok = message && message->type == "MVR_COMMIT_RET" && message->ok;
  if (logCallback) logCallback(ok ? "MVR-xchange sent MVR_COMMIT to " + StationDisplayName(station) + " and received MVR_COMMIT_RET." : "MVR-xchange MVR_COMMIT was not acknowledged by " + StationDisplayName(station) + ".");
  return ok;
}


// Requests one advertised MVR file from a remote MVR-xchange station.
std::optional<MvrXchangeCommit> MvrXchangeTcpClient::RequestCommit(const MvrXchangeRemoteStation &station, const std::string &fileUuid, LogCallback logCallback) {
  int fd = -1;
  if (!Connect(station, fd, logCallback)) return std::nullopt;
  const bool sent = SendJson(fd, mvr::xchange::BuildRequest(fileUuid, station.stationUuid));
  std::optional<std::uint64_t> advertisedSize;
  for (const auto &metadata : station.commits) {
    if ((fileUuid.empty() || metadata.fileUuid == CanonicalizeUuid(fileUuid)) && metadata.declaredFileSizeSpecified) advertisedSize = metadata.declaredFileSize;
  }
  std::vector<uint8_t> buffer;
  char chunk[4096];
  while (sent) {
    const int n = static_cast<int>(recv(fd, chunk, sizeof(chunk), 0));
    if (n <= 0) break;
    if (buffer.size() > mvr::xchange::kMaxBufferedInputBytes - static_cast<std::size_t>(n)) break;
    buffer.insert(buffer.end(), chunk, chunk + n);
    mvr::xchange::Packet decoded;
    std::string decodeError;
    const auto status = mvr::xchange::DecodePacket(buffer, decoded, decodeError);
    if (status == mvr::xchange::DecodeStatus::Invalid) break;
    if (status == mvr::xchange::DecodeStatus::Complete) {
      auto packet = std::make_optional(std::move(decoded));
      CloseSocketFd(fd);
      if (packet->type == mvr::xchange::PacketType::MvrFile) {
        if (advertisedSize && packet->payload.size() != *advertisedSize) {
          if (logCallback) logCallback("MVR-xchange rejected a payload whose byte count differs from advertised FileSize.");
          return std::nullopt;
        }
        MvrXchangeCommit commit;
        commit.fileUuid = CanonicalizeUuid(fileUuid);
        commit.stationUuid = CanonicalizeUuid(station.stationUuid);
        commit.payload = std::move(packet->payload);
        if (logCallback) logCallback("MVR-xchange received requested MVR payload from " + StationDisplayName(station) + ", bytes=" + std::to_string(commit.payload.size()) + ".");
        return commit;
      }
      if (packet->type == mvr::xchange::PacketType::Json && logCallback) {
        const std::string response(packet->payload.begin(), packet->payload.end());
        auto message = mvr::xchange::ParseMessage(response);
        logCallback("MVR-xchange request was rejected by " + StationDisplayName(station) + (message && !message->text.empty() ? ": " + message->text : "."));
      }
      return std::nullopt;
    }
  }
  CloseSocketFd(fd);
  if (logCallback) logCallback("MVR-xchange request failed while receiving from " + StationDisplayName(station) + ".");
  return std::nullopt;
}

// Opens a short-lived TCP connection to a discovered MVR-xchange station.
bool MvrXchangeTcpClient::Connect(const MvrXchangeRemoteStation &station, int &fd, LogCallback logCallback) {
#ifdef _WIN32
  WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
  fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
  if (fd < 0) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(station.port));
  if (inet_pton(AF_INET, station.ipAddress.c_str(), &addr.sin_addr) != 1) { CloseSocketFd(fd); fd = -1; return false; }
  ApplySocketTimeouts(fd);
  SetSocketBlocking(fd, false);
  if (logCallback) logCallback("MVR-xchange connecting to discovered station " + StationDisplayName(station) + " at " + station.ipAddress + ":" + std::to_string(station.port) + ".");
  const int result = connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
#ifdef _WIN32
  const bool pending = result != 0 && WSAGetLastError() == WSAEWOULDBLOCK;
#else
  const bool pending = result != 0 && errno == EINPROGRESS;
#endif
  if (result != 0 && (!pending || !WaitForConnect(fd))) { CloseSocketFd(fd); fd = -1; if (logCallback) logCallback("MVR-xchange connection failed or timed out to " + StationDisplayName(station) + "."); return false; }
  SetSocketBlocking(fd, true);
  return true;
}

// Sends one JSON message with official MVR-xchange TCP packet framing.
bool MvrXchangeTcpClient::SendJson(int fd, const std::string &json) {
  const std::vector<uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  std::size_t sent = 0;
  while (sent < packet.size()) {
    const int n = static_cast<int>(send(fd, reinterpret_cast<const char *>(packet.data() + sent), static_cast<int>(packet.size() - sent), 0));
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

// Receives one JSON packet from a remote MVR-xchange station.
bool MvrXchangeTcpClient::ReceiveJson(int fd, std::string &json) {
  std::vector<uint8_t> buffer;
  char chunk[4096];
  for (;;) {
    const int n = static_cast<int>(recv(fd, chunk, sizeof(chunk), 0));
    if (n <= 0) return false;
    if (buffer.size() > mvr::xchange::kMaxJsonPayloadBytes + 28 - static_cast<std::size_t>(n)) return false;
    buffer.insert(buffer.end(), chunk, chunk + n);
    mvr::xchange::Packet decoded;
    std::string error;
    const auto status = mvr::xchange::DecodePacket(buffer, decoded, error);
    if (status == mvr::xchange::DecodeStatus::Invalid) return false;
    if (status == mvr::xchange::DecodeStatus::Complete) {
      auto packet = std::make_optional(std::move(decoded));
      if (packet->type != mvr::xchange::PacketType::Json) return false;
      json.assign(packet->payload.begin(), packet->payload.end());
      return true;
    }
  }
}
