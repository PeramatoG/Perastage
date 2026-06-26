#include "mvr_xchange_tcp_server.h"
#include "mvr_xchange_message.h"
#include "mvr_xchange_packet.h"
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
}

// Creates an inactive TCP server wrapper.
MvrXchangeTcpServer::MvrXchangeTcpServer() = default;

// Stops the TCP server before destroying it.
MvrXchangeTcpServer::~MvrXchangeTcpServer() { Stop(); }

// Starts the MVR-xchange TCP Mode listener on the configured port.
bool MvrXchangeTcpServer::Start(const MvrXchangeSettings &settings, CommitResolver resolver, LogCallback logCallback) {
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
  if (thread_.joinable()) thread_.join();
#ifdef _WIN32
  WSACleanup();
#endif
}

// Returns whether the TCP listener is currently running.
bool MvrXchangeTcpServer::IsRunning() const { return running_; }

// Returns the active TCP port, or zero when the server is stopped.
int MvrXchangeTcpServer::Port() const { return running_ ? port_ : 0; }

// Logs the commit announcement that should be sent to connected stations.
void MvrXchangeTcpServer::BroadcastCommit(const MvrXchangeCommit &commit) {
  if (logCallback_) logCallback_("Published MVR revision " + commit.fileUuid + ".");
}

// Runs the blocking accept loop on a background thread.
void MvrXchangeTcpServer::Run() {
  while (running_) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = static_cast<int>(accept(listenFd_, reinterpret_cast<sockaddr *>(&client), &len));
    if (fd < 0) continue;
    std::thread(&MvrXchangeTcpServer::HandleClient, this, fd).detach();
  }
}

// Handles one MVR-xchange TCP client using official packet framing.
void MvrXchangeTcpServer::HandleClient(int clientFd) {
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
      if (msg.type == "MVR_JOIN") SendJson(clientFd, mvr::xchange::BuildJoinRet(settings_.stationUuid, settings_.stationName, settings_.groupName));
      else if (msg.type == "MVR_LEAVE") { SendJson(clientFd, mvr::xchange::BuildLeaveRet(settings_.stationUuid)); CloseSocketFd(clientFd); return; }
      else if (msg.type == "MVR_COMMIT") SendJson(clientFd, mvr::xchange::BuildCommitRet(msg.fileUuid, true));
      else if (msg.type == "MVR_REQUEST") {
        auto commit = resolver_ ? resolver_(msg.fileUuid) : std::nullopt;
        if (!commit) SendJson(clientFd, mvr::xchange::BuildError("MVR_REQUEST_RET", "Requested MVR file is not available."));
        else { SendJson(clientFd, mvr::xchange::BuildRequestRet(*commit)); const auto payloadPacket = mvr::xchange::EncodePacket(mvr::xchange::PacketType::MvrFile, commit->payload); send(clientFd, reinterpret_cast<const char *>(payloadPacket.data()), static_cast<int>(payloadPacket.size()), 0); }
      } else SendJson(clientFd, mvr::xchange::BuildError("MVR_ERROR", "Unsupported MVR-xchange message."));
    }
  }
  CloseSocketFd(clientFd);
}

// Sends one JSON message with official MVR-xchange TCP packet framing.
bool MvrXchangeTcpServer::SendJson(int fd, const std::string &json) {
  const std::vector<uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  return send(fd, reinterpret_cast<const char *>(packet.data()), static_cast<int>(packet.size()), 0) == static_cast<int>(packet.size());
}
