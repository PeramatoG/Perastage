#include "xchange/mvr_xchange_commit.h"
#include "xchange/mvr_xchange_dns_names.h"
#include "../core/uuidutils.h"
#include "xchange/mvr_xchange_message.h"
#include "xchange/mvr_xchange_mdns_cache.h"
#include "xchange/mvr_xchange_mdns_discovery.h"
#include "xchange/mvr_xchange_network_interfaces.h"
#include "xchange/mvr_xchange_packet.h"
#include "xchange/mvr_xchange_publication_policy.h"
#include "xchange/mvr_xchange_station_registry.h"
#include "xchange/mvr_xchange_tcp_client.h"
#include "xchange/mvr_xchange_tcp_server.h"
#include "json.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

struct LoopbackConnection {
  std::intptr_t fd = -1;
  std::vector<std::uint8_t> receiveBuffer;
};

enum class SocketReadiness {
  Ready,
  Error,
  Timeout
};

enum class PeerCloseResult {
  Closed,
  UnexpectedData,
  SocketError,
  Timeout
};

class TestSocketRuntime {
public:
  TestSocketRuntime();
  ~TestSocketRuntime();
};

// Keeps Winsock available for every client socket owned by this test executable.
TestSocketRuntime::TestSocketRuntime() {
#ifdef _WIN32
  WSADATA data;
  const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
  if (startupResult != 0) {
    std::cerr << "Winsock initialization failed for the MVR-xchange protocol test; error="
              << startupResult << std::endl;
    std::abort();
  }
#endif
}

// Releases the test executable's Winsock ownership after all sockets are closed.
TestSocketRuntime::~TestSocketRuntime() {
#ifdef _WIN32
  WSACleanup();
#endif
}

// Waits for a socket read operation to become safe within a fixed deadline.
static SocketReadiness WaitForSocketReadable(std::intptr_t fd, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (true) {
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now());
    if (remaining <= std::chrono::microseconds::zero()) return SocketReadiness::Timeout;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    timeval wait{};
    wait.tv_sec = static_cast<long>(remaining.count() / 1000000);
    wait.tv_usec = static_cast<long>(remaining.count() % 1000000);
#ifdef _WIN32
    const int ready = select(0, &readSet, nullptr, nullptr, &wait);
    if (ready < 0 && WSAGetLastError() == WSAEINTR) continue;
    if (ready < 0) return SocketReadiness::Error;
#else
    const int ready = select(static_cast<int>(fd + 1), &readSet, nullptr, nullptr, &wait);
    if (ready < 0 && errno == EINTR) continue;
    if (ready < 0) return SocketReadiness::Error;
#endif
    if (ready == 0) return SocketReadiness::Timeout;
    return FD_ISSET(fd, &readSet) ? SocketReadiness::Ready : SocketReadiness::Error;
  }
}

// Requires bounded socket readability before a test performs a receive operation.
static void RequireSocketReadable(std::intptr_t fd, const char *operation) {
  const auto readiness = WaitForSocketReadable(fd, std::chrono::seconds(2));
  if (readiness == SocketReadiness::Ready) return;
  std::cerr << operation << " did not become readable within the test deadline; result="
            << (readiness == SocketReadiness::Timeout ? "timeout" : "socket error") << std::endl;
  std::abort();
}

// Waits for a peer to close without allowing recv to block the test executable.
static PeerCloseResult WaitForPeerClose(std::intptr_t fd, std::chrono::milliseconds timeout) {
  const auto readiness = WaitForSocketReadable(fd, timeout);
  if (readiness == SocketReadiness::Timeout) return PeerCloseResult::Timeout;
  if (readiness == SocketReadiness::Error) return PeerCloseResult::SocketError;
  char probe = 0;
  const int received = static_cast<int>(recv(fd, &probe, 1, 0));
  if (received == 0) return PeerCloseResult::Closed;
  return received > 0 ? PeerCloseResult::UnexpectedData : PeerCloseResult::SocketError;
}

// Connects a test socket without allowing a stalled loopback connection to hang CTest.
static bool ConnectSocketWithDeadline(std::intptr_t fd, const sockaddr_in &address, std::chrono::milliseconds timeout) {
#ifdef _WIN32
  u_long nonBlocking = 1;
  if (ioctlsocket(fd, FIONBIO, &nonBlocking) != 0) return false;
#else
  const int originalFlags = fcntl(static_cast<int>(fd), F_GETFL, 0);
  if (originalFlags < 0 || fcntl(static_cast<int>(fd), F_SETFL, originalFlags | O_NONBLOCK) != 0) return false;
#endif
  const int connectResult = connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address));
#ifdef _WIN32
  const int connectError = connectResult == 0 ? 0 : WSAGetLastError();
  const bool pending = connectError == WSAEWOULDBLOCK || connectError == WSAEINPROGRESS || connectError == WSAEALREADY;
#else
  const int connectError = connectResult == 0 ? 0 : errno;
  const bool pending = connectError == EINPROGRESS;
#endif
  bool connected = connectResult == 0;
  if (!connected && pending) {
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(fd, &writeSet);
    timeval wait{};
    wait.tv_sec = static_cast<long>(timeout.count() / 1000);
    wait.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
#ifdef _WIN32
    const int ready = select(0, nullptr, &writeSet, nullptr, &wait);
#else
    const int ready = select(static_cast<int>(fd + 1), nullptr, &writeSet, nullptr, &wait);
#endif
    int socketError = 0;
#ifdef _WIN32
    int errorLength = sizeof(socketError);
#else
    socklen_t errorLength = sizeof(socketError);
#endif
    connected = ready > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR,
                                         reinterpret_cast<char *>(&socketError), &errorLength) == 0 && socketError == 0;
  }
#ifdef _WIN32
  u_long blocking = 0;
  if (ioctlsocket(fd, FIONBIO, &blocking) != 0) connected = false;
#else
  if (fcntl(static_cast<int>(fd), F_SETFL, originalFlags) != 0) connected = false;
#endif
  return connected;
}

// Bounds test-side sends so a stalled peer fails instead of hanging CTest.
static bool ApplyTestSocketSendTimeout(std::intptr_t fd, std::chrono::milliseconds timeout) {
#ifdef _WIN32
  const DWORD timeoutMs = static_cast<DWORD>(timeout.count());
  return setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                    reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) == 0;
#else
  timeval sendTimeout{};
  sendTimeout.tv_sec = static_cast<long>(timeout.count() / 1000);
  sendTimeout.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
  return setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout)) == 0;
#endif
}

// Opens one reusable TCP connection to a loopback MVR-xchange server.
static LoopbackConnection ConnectLoopback(int port) {
  LoopbackConnection connection;
  connection.fd = static_cast<std::intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  assert(connection.fd >= 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (!ConnectSocketWithDeadline(connection.fd, address, std::chrono::seconds(2))) {
    std::cerr << "Loopback MVR-xchange connection did not complete within the test deadline." << std::endl;
    std::abort();
  }
  if (!ApplyTestSocketSendTimeout(connection.fd, std::chrono::seconds(2))) {
    std::cerr << "Loopback MVR-xchange send timeout could not be configured." << std::endl;
    std::abort();
  }
  return connection;
}

// Sends one framed JSON message over an existing test connection.
static void SendPersistentJson(const LoopbackConnection &connection, const std::string &json) {
  const std::vector<std::uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  std::size_t sent = 0;
  while (sent < packet.size()) {
    const int count = static_cast<int>(send(connection.fd, reinterpret_cast<const char *>(packet.data() + sent), static_cast<int>(packet.size() - sent), 0));
    assert(count > 0);
    sent += static_cast<std::size_t>(count);
  }
}

// Receives one framed packet while preserving coalesced bytes for the next message.
static mvr::xchange::Packet ReceivePersistentPacket(LoopbackConnection &connection) {
  while (true) {
    mvr::xchange::Packet packet;
    std::string error;
    const auto status = mvr::xchange::DecodePacket(connection.receiveBuffer, packet, error);
    if (status == mvr::xchange::DecodeStatus::Complete) return packet;
    assert(status == mvr::xchange::DecodeStatus::NeedMoreData);
    char buffer[4096];
    RequireSocketReadable(connection.fd, "Persistent MVR-xchange response");
    const int received = static_cast<int>(recv(connection.fd, buffer, sizeof(buffer), 0));
    assert(received > 0);
    connection.receiveBuffer.insert(connection.receiveBuffer.end(), buffer, buffer + received);
  }
}

// Closes one reusable loopback test connection.
static void CloseLoopback(LoopbackConnection &connection) {
#ifdef _WIN32
  closesocket(connection.fd);
#else
  close(connection.fd);
#endif
  connection.fd = -1;
}

// Exchanges one raw JSON transaction with a loopback test server.
static std::string ExchangeRawJson(int port, const std::string &json) {
  auto connection = ConnectLoopback(port);
  const std::vector<std::uint8_t> payload(json.begin(), json.end());
  const auto packet = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  std::size_t sent = 0;
  while (sent < packet.size()) {
    const int count = static_cast<int>(send(connection.fd, reinterpret_cast<const char *>(packet.data() + sent), static_cast<int>(packet.size() - sent), 0));
    assert(count > 0);
    sent += static_cast<std::size_t>(count);
  }
  std::vector<std::uint8_t> response;
  char buffer[4096];
  while (true) {
    RequireSocketReadable(connection.fd, "Raw MVR-xchange response");
    const int received = static_cast<int>(recv(connection.fd, buffer, sizeof(buffer), 0));
    assert(received > 0);
    response.insert(response.end(), buffer, buffer + received);
    if (auto decoded = mvr::xchange::TryDecodePacket(response)) {
      CloseLoopback(connection);
      return {decoded->payload.begin(), decoded->payload.end()};
    }
  }
}

// Appends one uncompressed DNS name to a synthetic datagram.
static void AppendDnsName(std::vector<std::uint8_t> &out, const std::string &name) {
  std::size_t start = 0;
  while (start < name.size()) {
    const auto end = name.find('.', start);
    if (end == start) break;
    const auto length = (end == std::string::npos ? name.size() : end) - start;
    out.push_back(static_cast<std::uint8_t>(length));
    out.insert(out.end(), name.begin() + start, name.begin() + start + length);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  out.push_back(0);
}

// Appends one synthetic DNS resource-record header and payload.
static void AppendDnsRecord(std::vector<std::uint8_t> &out, const std::string &owner, std::uint16_t type,
                            std::uint32_t ttl, const std::vector<std::uint8_t> &payload) {
  AppendDnsName(out, owner);
  out.push_back(static_cast<std::uint8_t>(type >> 8)); out.push_back(static_cast<std::uint8_t>(type));
  out.push_back(0); out.push_back(1);
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(ttl >> shift));
  out.push_back(static_cast<std::uint8_t>(payload.size() >> 8)); out.push_back(static_cast<std::uint8_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
}

// Verifies bounded commit history and latest commit lookup behavior.
static void TestCommitStore() {
  MvrXchangeCommitStore store(2);
  MvrXchangeCommit first{"11111111-1111-1111-1111-111111111111", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "one.mvr", {}, {}, {1}, 0, false, 0, 0, {}, true};
  MvrXchangeCommit second{"22222222-2222-2222-2222-222222222222", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "two.mvr", {}, {}, {2, 3}, 0, false, 0, 0, {}, true};
  MvrXchangeCommit third{"33333333-3333-3333-3333-333333333333", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "three.mvr", {}, {}, {4}, 0, false, 0, 0, {}, true};
  store.Add(first);
  store.Add(second);
  store.Add(third);
  assert(!store.FindByFileUuid("11111111-1111-1111-1111-111111111111"));
  assert(store.FindByFileUuid("22222222-2222-2222-2222-222222222222"));
  assert(store.Latest()->fileUuid == "33333333-3333-3333-3333-333333333333");
  assert(store.Latest()->FileSize() == 1);
}

// Verifies official MVR-xchange TCP packet framing constants and roundtrip behavior.
static void TestPackets() {
  const std::string json = "{\"Type\":\"MVR_JOIN\"}";
  const std::vector<uint8_t> payload(json.begin(), json.end());
  auto encoded = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  assert(encoded.size() == 28 + payload.size());
  assert(encoded[0] == 0x00 && encoded[1] == 0x0b && encoded[2] == 0xe1 && encoded[3] == 0xba);
  assert(encoded[7] == 0x01);
  assert(encoded[15] == 0x01);
  auto decoded = mvr::xchange::TryDecodePacket(encoded);
  assert(decoded);
  assert(decoded->type == mvr::xchange::PacketType::Json);
  assert(std::string(decoded->payload.begin(), decoded->payload.end()) == json);
}

// Verifies official MVR-xchange JSON parsing and response serialization.
static void TestMessages() {
  auto msg = mvr::xchange::ParseMessage("{\"Type\":\"MVR_REQUEST\",\"FileUUID\":\"abcdefab-cdef-abcd-efab-cdefabcdefab\"}");
  assert(msg);
  assert(msg->type == "MVR_REQUEST");
  assert(msg->fileUuid == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(mvr::xchange::ValidateMessage(*msg).empty());
  assert(!mvr::xchange::ParseMessage("not json"));
  assert(!mvr::xchange::ParseMessage("[]"));

  MvrXchangeCommit commit{"ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB", "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "scene.mvr", "Manual", {}, {1, 2, 3}, 0, false, 0, 0, {}, true};
  const auto commitJson = nlohmann::json::parse(mvr::xchange::BuildCommit(commit));
  assert(commitJson["Type"] == "MVR_COMMIT");
  assert(commitJson["verMajor"] == 1);
  assert(commitJson["verMinor"] == 6);
  assert(commitJson["FileSize"] == 3);
  assert(commitJson["FileUUID"] == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(commitJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(commitJson["ForStationsUUID"].is_array());
  auto parsedCommit = mvr::xchange::ParseMessage(commitJson.dump());
  assert(parsedCommit);
  assert(parsedCommit->type == "MVR_COMMIT");
  assert(parsedCommit->fileUuid == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(parsedCommit->stationUuid == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(mvr::xchange::ValidateMessage(*parsedCommit).empty());

  const auto outgoingJoinJson = nlohmann::json::parse(mvr::xchange::BuildJoin("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(outgoingJoinJson["Type"] == "MVR_JOIN");
  assert(outgoingJoinJson["Provider"] == "Perastage");
  assert(outgoingJoinJson["verMajor"] == 1);
  assert(outgoingJoinJson["verMinor"] == 6);
  assert(outgoingJoinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(outgoingJoinJson["Commits"].size() == 1);
  assert(!outgoingJoinJson.contains("Files"));

  auto parsedJoin = mvr::xchange::ParseMessage(outgoingJoinJson.dump());
  assert(parsedJoin);
  assert(parsedJoin->type == "MVR_JOIN");
  assert(parsedJoin->provider == "Perastage");
  assert(parsedJoin->stationName == "Perastage");
  assert(parsedJoin->verMajor == 1);
  assert(parsedJoin->verMinor == 6);
  assert(parsedJoin->commits.size() == 1);
  assert(parsedJoin->commits[0].payload.empty());
  assert(parsedJoin->commits[0].declaredFileSize == 3);
  assert(mvr::xchange::ValidateMessage(*parsedJoin).empty());

  const auto joinJson = nlohmann::json::parse(mvr::xchange::BuildJoinRet("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(joinJson["Type"] == "MVR_JOIN_RET");
  assert(joinJson["OK"] == true);
  assert(joinJson["Provider"] == "Perastage");
  assert(joinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(joinJson["Commits"].size() == 1);
  assert(!joinJson.contains("Files"));
  auto parsedJoinRet = mvr::xchange::ParseMessage(joinJson.dump());
  assert(parsedJoinRet);
  assert(parsedJoinRet->type == "MVR_JOIN_RET");
  assert(parsedJoinRet->ok);
  assert(parsedJoinRet->commits.size() == 1);
  assert(mvr::xchange::ValidateMessage(*parsedJoinRet).empty());

  auto adjustedExampleJoinRet = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_JOIN_RET","OK":true,"Message":"","verMajor":1,"verMinor":6,"StationUUID":"a7669ff9-bd61-4486-aea6-c190f8ba6b8c","StationName":"MVR Application","Commits":[]})");
  assert(adjustedExampleJoinRet && adjustedExampleJoinRet->provider.empty());
  assert(mvr::xchange::ValidateMessage(*adjustedExampleJoinRet).empty());
  auto newMemberJoinRet = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_JOIN_RET","OK":true,"Message":"","Provider":"grandMA3","verMajor":0,"verMinor":0,"StationUUID":"a7669ff9-bd61-4486-aea6-c190f8ba6b8c","StationName":"Console","Commits":[]})");
  assert(newMemberJoinRet && mvr::xchange::ValidateMessage(*newMemberJoinRet).empty());
  auto newMemberJoin = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_JOIN","Provider":"grandMA3","verMajor":0,"verMinor":0,"StationUUID":"a7669ff9-bd61-4486-aea6-c190f8ba6b8c","StationName":"Console","Commits":[]})");
  assert(newMemberJoin && mvr::xchange::ValidateMessage(*newMemberJoin).empty());
  auto mixedJoinRetVersion = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_JOIN_RET","OK":true,"Message":"","Provider":"Peer","verMajor":0,"verMinor":6,"StationUUID":"a7669ff9-bd61-4486-aea6-c190f8ba6b8c","StationName":"Peer","Commits":[]})");
  assert(mixedJoinRetVersion && !mvr::xchange::ValidateMessage(*mixedJoinRetVersion).empty());
  auto missingJoinProvider = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_JOIN","verMajor":1,"verMinor":6,"StationUUID":"a7669ff9-bd61-4486-aea6-c190f8ba6b8c","StationName":"MVR Application","Commits":[]})");
  assert(missingJoinProvider && mvr::xchange::ValidateMessage(*missingJoinProvider) == "MVR_JOIN is missing Provider.");

  const auto errorJson = nlohmann::json::parse(mvr::xchange::BuildRequestError("The MVR is not available on this client"));
  assert(errorJson["Type"] == "MVR_REQUEST_RET");
  assert(errorJson["OK"] == false);
  assert(errorJson["Message"] == "The MVR is not available on this client");
}

// Verifies canonical request source arrays and the specification-example string compatibility boundary.
static void TestRequestSourceStations() {
  const std::string firstUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::string secondUuid = "11111111-2222-3333-4444-555555555555";
  const std::string fileUuid = "abcdefab-cdef-abcd-efab-cdefabcdefab";
  auto canonical = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_REQUEST","FileUUID":"abcdefab-cdef-abcd-efab-cdefabcdefab","FromStationUUID":["AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"]})");
  assert(canonical && mvr::xchange::ValidateMessage(*canonical).empty());
  assert(canonical->sourceStationUuids == std::vector<std::string>{firstUuid});

  auto multiple = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_REQUEST","FromStationUUID":["AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE","11111111-2222-3333-4444-555555555555"]})");
  assert(multiple && mvr::xchange::ValidateMessage(*multiple).empty());
  assert(multiple->sourceStationUuids == (std::vector<std::string>{firstUuid, secondUuid}));

  auto legacy = mvr::xchange::ParseMessage(
      R"({"Type":"MVR_REQUEST","FromStationUUID":"AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"})");
  assert(legacy && mvr::xchange::ValidateMessage(*legacy).empty());
  assert(legacy->sourceStationUuids == std::vector<std::string>{firstUuid});
  auto emptyLegacy = mvr::xchange::ParseMessage(R"({"Type":"MVR_REQUEST","FromStationUUID":""})");
  assert(emptyLegacy && mvr::xchange::ValidateMessage(*emptyLegacy).empty() && emptyLegacy->sourceStationUuids.empty());

  const auto outgoing = nlohmann::json::parse(mvr::xchange::BuildRequest(
      "ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB", {secondUuid}));
  assert(outgoing["Type"] == "MVR_REQUEST");
  assert(outgoing["FileUUID"] == fileUuid);
  assert(outgoing["FromStationUUID"].is_array());
  assert(outgoing["FromStationUUID"] == nlohmann::json::array({secondUuid}));
  assert(std::find(outgoing["FromStationUUID"].begin(), outgoing["FromStationUUID"].end(), firstUuid) == outgoing["FromStationUUID"].end());
  const auto noSources = nlohmann::json::parse(mvr::xchange::BuildRequest("", {}));
  assert(noSources["FromStationUUID"].is_array() && noSources["FromStationUUID"].empty());
}

// Verifies canonical LEAVE output and the isolated legacy sender alias.
static void TestLeaveMessages() {
  const std::string uuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const auto outgoing = nlohmann::json::parse(mvr::xchange::BuildLeave(uuid));
  assert(outgoing["FromStationUUID"].is_string());
  assert(outgoing["FromStationUUID"] == uuid && !outgoing.contains("StationUUID"));
  auto canonical = mvr::xchange::ParseMessage(R"({"Type":"MVR_LEAVE","FromStationUUID":"AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"})");
  assert(canonical && canonical->fromStationUuid == uuid && mvr::xchange::ValidateMessage(*canonical).empty());
  auto legacy = mvr::xchange::ParseMessage(R"({"Type":"MVR_LEAVE","StationUUID":"AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE"})");
  assert(legacy && legacy->stationUuid == uuid && mvr::xchange::ValidateMessage(*legacy).empty());
  auto missing = mvr::xchange::ParseMessage(R"({"Type":"MVR_LEAVE"})");
  assert(missing && !mvr::xchange::ValidateMessage(*missing).empty());
}

// Verifies recognizable malformed requests retain their matching RET type.
static void TestTypedErrors() {
  const std::string stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::vector<std::pair<std::string, std::string>> cases = {
      {R"({"Type":"MVR_JOIN","Commits":{}})", "MVR_JOIN_RET"},
      {R"({"Type":"MVR_COMMIT","FileUUID":"bad"})", "MVR_COMMIT_RET"},
      {R"({"Type":"MVR_LEAVE","FromStationUUID":"bad"})", "MVR_LEAVE_RET"},
      {R"({"Type":"MVR_REQUEST","FileUUID":"bad"})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FileUUID":123})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":123})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":{}})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":["bad"]})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":["aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",123]})", "MVR_REQUEST_RET"}};
  for (const auto &[json, expectedType] : cases) {
    const auto message = mvr::xchange::ParseMessage(json);
    assert(message && !message->type.empty());
    const std::string error = mvr::xchange::ValidateMessage(*message);
    assert(!error.empty());
    const auto response = nlohmann::json::parse(mvr::xchange::BuildTypedErrorResponse(message->type, stationUuid, "Perastage", error));
    assert(response["Type"] == expectedType && response["OK"] == false);
  }
}

// Verifies canonical inventory presence and isolated legacy Files compatibility.
static void TestInventoryCompatibility() {
  const std::string entry = R"({"Type":"MVR_COMMIT","verMajor":1,"verMinor":6,"FileSize":999999999999,"FileUUID":"11111111-1111-1111-1111-111111111111","StationUUID":"22222222-2222-2222-2222-222222222222","ForStationsUUID":[],"FileName":"scene.mvr","Comment":"remote"})";
  auto absent = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222"})");
  assert(absent && absent->inventoryPresence == mvr::xchange::InventoryPresence::Absent);
  auto empty = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Commits":[]})");
  assert(empty && empty->inventoryPresence == mvr::xchange::InventoryPresence::PresentEmpty);
  auto files = mvr::xchange::ParseMessage(std::string(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Files":[)") + entry + "]}");
  assert(files && files->commits.size() == 1 && files->commits[0].payload.empty());
  auto both = mvr::xchange::ParseMessage(std::string(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Commits":[)") + entry + R"(],"Files":[)" + entry + "]}");
  assert(both && both->commits.size() == 1);
  assert(both->commits[0].fileName == "scene.mvr" && both->commits[0].comment == "remote");
  auto malformedEntry = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":1,"verMinor":6,"Commits":[{"Type":"MVR_COMMIT","FileSize":-1,"FileUUID":"11111111-1111-1111-1111-111111111111","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":1,"verMinor":6}]})");
  assert(malformedEntry && !mvr::xchange::ValidateMessage(*malformedEntry).empty());
  auto newMember = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":1,"verMinor":6,"Commits":[{"Type":"MVR_COMMIT","FileSize":0,"FileUUID":"11111111-1111-1111-1111-111111111111","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":0,"verMinor":0}]})");
  assert(newMember && mvr::xchange::ValidateMessage(*newMember).empty());
  auto mixedVersion = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":1,"verMinor":6,"Commits":[{"Type":"MVR_COMMIT","FileSize":0,"FileUUID":"11111111-1111-1111-1111-111111111111","StationUUID":"22222222-2222-2222-2222-222222222222","verMajor":0,"verMinor":6}]})");
  assert(mixedVersion && !mvr::xchange::ValidateMessage(*mixedVersion).empty());
}

// Verifies framing rejects invalid packages and reassembles bounded multipart payloads.
static void TestPacketRejection() {
  std::vector<uint8_t> payload{'x'};
  auto multipart = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  multipart[15] = 2;
  mvr::xchange::Packet packet;
  std::string error;
  assert(mvr::xchange::DecodePacket(multipart, packet, error) == mvr::xchange::DecodeStatus::Complete);
  auto invalidType = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  invalidType[19] = 9;
  assert(mvr::xchange::DecodePacket(invalidType, packet, error) == mvr::xchange::DecodeStatus::Invalid);

  mvr::xchange::PacketReassembler reassembler;
  mvr::xchange::Packet complete;
  mvr::xchange::Packet first{mvr::xchange::PacketType::Json, 0, 2, {'a'}};
  mvr::xchange::Packet second{mvr::xchange::PacketType::Json, 1, 2, {'b'}};
  assert(reassembler.Add(first, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  assert(reassembler.Add(second, complete, error) == mvr::xchange::DecodeStatus::Complete);
  assert(std::string(complete.payload.begin(), complete.payload.end()) == "ab");
  assert(reassembler.Add(second, complete, error) == mvr::xchange::DecodeStatus::Invalid);
  auto firstWire = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {'a'});
  auto secondWire = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {'b'});
  firstWire[15] = 2;
  secondWire[11] = 1;
  secondWire[15] = 2;
  firstWire.insert(firstWire.end(), secondWire.begin(), secondWire.end());
  reassembler.Reset();
  assert(mvr::xchange::DecodePacket(firstWire, packet, error) == mvr::xchange::DecodeStatus::Complete);
  assert(reassembler.Add(packet, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  assert(mvr::xchange::DecodePacket(firstWire, packet, error) == mvr::xchange::DecodeStatus::Complete);
  assert(reassembler.Add(packet, complete, error) == mvr::xchange::DecodeStatus::Complete);

  auto invalidHeader = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); invalidHeader[0] = 1;
  assert(mvr::xchange::DecodePacket(invalidHeader, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto invalidVersion = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); invalidVersion[7] = 2;
  assert(mvr::xchange::DecodePacket(invalidVersion, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto zeroCount = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); zeroCount[15] = 0;
  assert(mvr::xchange::DecodePacket(zeroCount, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto outOfRange = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); outOfRange[11] = 1;
  assert(mvr::xchange::DecodePacket(outOfRange, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto tooMany = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); tooMany[14] = 4; tooMany[15] = 1;
  assert(mvr::xchange::DecodePacket(tooMany, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto oversizedJson = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {}); oversizedJson[25] = 0x10; oversizedJson[27] = 1;
  assert(mvr::xchange::DecodePacket(oversizedJson, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto oversizedMvr = mvr::xchange::EncodePacket(mvr::xchange::PacketType::MvrFile, {}); oversizedMvr[24] = 0x20; oversizedMvr[27] = 1;
  assert(mvr::xchange::DecodePacket(oversizedMvr, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto truncated = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {'x'}); truncated.pop_back();
  assert(mvr::xchange::DecodePacket(truncated, packet, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  reassembler.Reset();
  mvr::xchange::Packet nonzeroStart{mvr::xchange::PacketType::Json, 1, 2, {'x'}};
  assert(reassembler.Add(nonzeroStart, complete, error) == mvr::xchange::DecodeStatus::Invalid);
  reassembler.Reset();
  assert(reassembler.Add(first, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  auto inconsistentCount = second; inconsistentCount.packageCount = 3;
  assert(reassembler.Add(inconsistentCount, complete, error) == mvr::xchange::DecodeStatus::Invalid);
  reassembler.Reset();
  assert(reassembler.Add(first, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  auto inconsistentType = second; inconsistentType.type = mvr::xchange::PacketType::MvrFile;
  assert(reassembler.Add(inconsistentType, complete, error) == mvr::xchange::DecodeStatus::Invalid);
  reassembler.Reset();
  mvr::xchange::Packet largeFirst{mvr::xchange::PacketType::Json, 0, 2, std::vector<std::uint8_t>(600 * 1024)};
  mvr::xchange::Packet largeSecond{mvr::xchange::PacketType::Json, 1, 2, std::vector<std::uint8_t>(600 * 1024)};
  assert(reassembler.Add(std::move(largeFirst), complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  assert(reassembler.Add(std::move(largeSecond), complete, error) == mvr::xchange::DecodeStatus::Invalid);
}

// Verifies cached DNS-SD records resolve across layouts, order, TTL, and goodbye.
static void TestMdnsRecordCache() {
  using namespace mvr::xchange;
  MdnsRecordCache cache;
  const std::string group = "Default._mvrxchange._tcp.local.";
  cache.Apply({DnsRecordType::A, "HOST.local.", {}, "192.0.2.10", {}, 0, 120, 4, 1000});
  cache.Apply({DnsRecordType::Txt, "Peer.Default._mvrxchange._tcp.local.", {}, {}, {{"stationname", "Peer"}, {"stationuuid", "BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF"}}, 0, 120, 4, 1000});
  cache.Apply({DnsRecordType::Srv, "peer.default._mvrxchange._tcp.local", "host.LOCAL", {}, {}, 42424, 120, 4, 1000});
  cache.Apply({DnsRecordType::Ptr, group, "PEER.Default._mvrxchange._tcp.local.", {}, {}, 0, 120, 4, 1000});
  auto stations = cache.Resolve(group, 1000);
  assert(stations.size() == 1 && stations[0].ipAddress == "192.0.2.10" && stations[0].stationName == "Peer");
  cache.Apply({DnsRecordType::Ptr, group, "PEER.Default._mvrxchange._tcp.local.", {}, {}, 0, 0, 4, 2000});
  assert(cache.Resolve(group, 2500).size() == 1);
  cache.Expire(3000);
  assert(cache.Resolve(group, 3000).empty());

  MdnsRecordCache official;
  const std::string officialInstance = "Member.Default._mvrxchange._tcp.local.";
  official.Apply({DnsRecordType::Ptr, group, officialInstance, {}, {}, 0, 60, 1, 5000, "192.0.2.1"});
  official.Apply({DnsRecordType::Srv, officialInstance, "group-host.local.", {}, {}, 43000, 60, 1, 5000, "192.0.2.1"});
  official.Apply({DnsRecordType::Txt, officialInstance, {}, {}, {{"stationname", "Group member"}, {"stationuuid", "11111111-2222-3333-4444-555555555555"}}, 0, 60, 1, 5000, "192.0.2.1"});
  official.Apply({DnsRecordType::Aaaa, "group-host.local", {}, "2001:db8::1", {}, 0, 60, 1, 5000, "192.0.2.1"});
  stations = official.Resolve(group, 5000);
  assert(stations.size() == 1 && stations[0].ipAddress == "2001:db8::1" && stations[0].port == 43000);
  official.Apply({DnsRecordType::Srv, officialInstance, "group-host.local.", {}, {}, 43001, 60, 1, 5100, "192.0.2.1"});
  official.Apply({DnsRecordType::Aaaa, "group-host.local", {}, "2001:db8::2", {}, 0, 60, 1, 5100, "192.0.2.1"});
  stations = official.Resolve(group, 5100);
  assert(stations.size() == 1 && stations[0].ipAddress == "2001:db8::2" && stations[0].port == 43001 &&
         stations[0].stationUuid == "11111111-2222-3333-4444-555555555555");

  MdnsRecordCache multipleResponders;
  for (const auto &[responder, instance, host, address, port, uuid] : std::vector<std::tuple<std::string, std::string, std::string, std::string, std::uint16_t, std::string>>{
           {"192.0.2.11", "Station-A.Default._mvrxchange._tcp.local.", "station-a.local.", "192.0.2.11", 41001, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"},
           {"192.0.2.12", "Station-B.Default._mvrxchange._tcp.local.", "station-b.local.", "192.0.2.12", 41002, "bbbbbbbb-cccc-dddd-eeee-ffffffffffff"}}) {
    multipleResponders.Apply({DnsRecordType::Ptr, group, instance, {}, {}, 0, 120, 3, 8000, responder});
    multipleResponders.Apply({DnsRecordType::Srv, instance, host, {}, {}, port, 120, 3, 8000, responder});
    multipleResponders.Apply({DnsRecordType::Txt, instance, {}, {}, {{"stationname", host}, {"stationuuid", uuid}}, 0, 120, 3, 8000, responder});
    multipleResponders.Apply({DnsRecordType::A, host, {}, address, {}, 0, 120, 3, 8000, responder});
  }
  stations = multipleResponders.Resolve(group, 8000);
  assert(stations.size() == 2);
  assert(stations[0].stationUuid != stations[1].stationUuid && stations[0].ipAddress != stations[1].ipAddress && stations[0].port != stations[1].port);

  MdnsRecordCache isolatedOrigins;
  const std::string isolatedInstance = "Isolated.Default._mvrxchange._tcp.local.";
  isolatedOrigins.Apply({DnsRecordType::Ptr, group, isolatedInstance, {}, {}, 0, 120, 3, 8000, "192.0.2.21"});
  isolatedOrigins.Apply({DnsRecordType::Srv, isolatedInstance, "isolated.local.", {}, {}, 42000, 120, 3, 8000, "192.0.2.21"});
  isolatedOrigins.Apply({DnsRecordType::Txt, isolatedInstance, {}, {}, {{"stationname", "Wrong responder"}, {"stationuuid", "cccccccc-dddd-eeee-ffff-000000000000"}}, 0, 120, 3, 8000, "192.0.2.22"});
  isolatedOrigins.Apply({DnsRecordType::A, "isolated.local.", {}, "192.0.2.22", {}, 0, 120, 3, 8000, "192.0.2.22"});
  assert(isolatedOrigins.Resolve(group, 8000).empty());

  MdnsRecordCache sameAddressCompatibility;
  for (const auto &[instance, port, uuid] : std::vector<std::tuple<std::string, std::uint16_t, std::string>>{
           {"Perastage.Default._mvrxchange._tcp.local.", 63610, "dddddddd-eeee-ffff-0000-111111111111"},
           {"DMXRouter.Default._mvrxchange._tcp.local.", 52187, "eeeeeeee-ffff-0000-1111-222222222222"},
           {"grandMA3.Default._mvrxchange._tcp.local.", 42424, "ffffffff-0000-1111-2222-333333333333"}}) {
    sameAddressCompatibility.Apply({DnsRecordType::Ptr, kMvrXchangeServiceType, instance, {}, {}, 0, 120, 3, 8000, "192.0.2.30"});
    sameAddressCompatibility.Apply({DnsRecordType::Srv, instance, "shared.local.", {}, {}, port, 120, 3, 8000, "192.0.2.30"});
    sameAddressCompatibility.Apply({DnsRecordType::Txt, instance, {}, {}, {{"stationuuid", uuid}}, 0, 120, 3, 8000, "192.0.2.30"});
  }
  sameAddressCompatibility.Apply({DnsRecordType::A, "shared.local.", {}, "192.0.2.30", {}, 0, 120, 3, 8000, "192.0.2.30"});
  stations = sameAddressCompatibility.Resolve(group, 8000);
  assert(stations.size() == 3);
  std::set<int> sameHostPorts;
  for (const auto &station : stations) sameHostPorts.insert(station.port);
  assert(sameHostPorts == std::set<int>({42424, 52187, 63610}));

  MdnsRecordCache unsafeFallback;
  const std::string grandMaInstance = "grandMA3.Default._mvrxchange._tcp.local.";
  unsafeFallback.Apply({DnsRecordType::Ptr, group, grandMaInstance, {}, {}, 0, 120, 5, 9000, "192.168.0.107"});
  unsafeFallback.Apply({DnsRecordType::Txt, grandMaInstance, {}, {}, {{"stationname", "grandMA3"}, {"stationuuid", "ffffffff-0000-1111-2222-333333333333"}}, 0, 120, 5, 9000, "192.168.0.107"});
  unsafeFallback.Apply({DnsRecordType::Srv, group, "perastage.local.", {}, {}, 63610, 120, 5, 9000, "192.168.0.107"});
  unsafeFallback.Apply({DnsRecordType::A, "perastage.local.", {}, "192.168.0.107", {}, 0, 120, 5, 9000, "192.168.0.107"});
  assert(unsafeFallback.Resolve(group, 9000).empty());
  unsafeFallback.Apply({DnsRecordType::Srv, grandMaInstance, "grandma.local.", {}, {}, 42424, 120, 5, 9100, "192.168.0.107"});
  unsafeFallback.Apply({DnsRecordType::A, "grandma.local.", {}, "192.168.0.107", {}, 0, 120, 5, 9100, "192.168.0.107"});
  stations = unsafeFallback.Resolve(group, 9100);
  assert(stations.size() == 1 && stations[0].stationUuid == "ffffffff-0000-1111-2222-333333333333" && stations[0].port == 42424);

  MdnsRecordCache compatible;
  compatible.Apply({DnsRecordType::A, "base-host.local", {}, "192.0.2.20", {}, 0, 60, 2, 6000});
  compatible.Apply({DnsRecordType::Srv, "BasePeer.Default._mvrxchange._tcp.local.", "base-host.local", {}, {}, 44000, 60, 2, 6000});
  compatible.Apply({DnsRecordType::Txt, "BasePeer.Default._mvrxchange._tcp.local.", {}, {}, {{"stationname", "Base peer"}}, 0, 60, 2, 6000});
  compatible.Apply({DnsRecordType::Txt, "basepeer.default._mvrxchange._tcp.local", {}, {}, {{"stationuuid", "99999999-2222-3333-4444-555555555555"}}, 0, 60, 2, 6001});
  compatible.Apply({DnsRecordType::Ptr, mvr::xchange::kMvrXchangeServiceType, "BASEPEER.Default._mvrxchange._tcp.local.", {}, {}, 0, 60, 2, 6000});
  compatible.Apply({DnsRecordType::Ptr, "Other._mvrxchange._tcp.local.", "OtherPeer.Other._mvrxchange._tcp.local.", {}, {}, 0, 60, 2, 6000});
  stations = compatible.Resolve(group, 6001);
  assert(stations.size() == 1 && stations[0].stationName == "Base peer" && stations[0].stationUuid == "99999999-2222-3333-4444-555555555555");

  compatible.ApplyBatch({
      {DnsRecordType::Txt, "BasePeer.Default._mvrxchange._tcp.local.", {}, {}, {{"stationname", "Refreshed"}}, 0, 60, 2, 7000},
      {DnsRecordType::Txt, "BasePeer.Default._mvrxchange._tcp.local.", {}, {}, {{"stationuuid", "99999999-2222-3333-4444-555555555555"}}, 0, 60, 2, 7000}});
  stations = compatible.Resolve(group, 7000);
  assert(stations.size() == 1 && stations[0].stationName == "Refreshed" && !stations[0].stationUuid.empty());
}

// Verifies raw answer and additional DNS records resolve through the normal cache.
static void TestRawMdnsDatagram() {
  const std::string group = "Default._mvrxchange._tcp.local.";
  const std::string instance = "Peer.Default._mvrxchange._tcp.local.";
  const std::string host = "PeerHost.local.";
  std::vector<std::uint8_t> datagram(12, 0);
  datagram[6] = 0; datagram[7] = 1;
  datagram[10] = 0; datagram[11] = 3;
  std::vector<std::uint8_t> ptr; AppendDnsName(ptr, instance);
  AppendDnsRecord(datagram, group, 12, 120, ptr);
  std::vector<std::uint8_t> srv{0, 0, 0, 0, 0xa5, 0xe0}; AppendDnsName(srv, host);
  AppendDnsRecord(datagram, instance, 33, 120, srv);
  const std::string txtValue = "StationName=Peer";
  const std::string txtUuid = "StationUUID=bbbbbbbb-cccc-dddd-eeee-ffffffffffff";
  std::vector<std::uint8_t> txt{static_cast<std::uint8_t>(txtValue.size())}; txt.insert(txt.end(), txtValue.begin(), txtValue.end());
  txt.push_back(static_cast<std::uint8_t>(txtUuid.size())); txt.insert(txt.end(), txtUuid.begin(), txtUuid.end());
  AppendDnsRecord(datagram, instance, 16, 120, txt);
  AppendDnsRecord(datagram, host, 1, 120, {192, 0, 2, 40});
  mvr::xchange::MdnsRecordCache cache;
  cache.ApplyBatch(mvr::xchange::ParseMdnsRecords(datagram.data(), datagram.size(), 7, 1000, "192.0.2.40"));
  auto stations = cache.Resolve(group, 1000);
  assert(stations.size() == 1 && stations[0].port == 42464 && stations[0].ipAddress == "192.0.2.40");
  std::vector<std::uint8_t> goodbye(12, 0);
  goodbye[6] = 0; goodbye[7] = 1;
  AppendDnsRecord(goodbye, group, 12, 0, ptr);
  const auto goodbyeRecords = mvr::xchange::ParseMdnsRecords(goodbye.data(), goodbye.size(), 7, 2000, "192.0.2.40");
  assert(goodbyeRecords.size() == 1 && goodbyeRecords[0].type == mvr::xchange::DnsRecordType::Ptr);
  cache.ApplyBatch(goodbyeRecords);
  assert(cache.Resolve(group, 2500).size() == 1);
  cache.Expire(3000);
  assert(cache.Resolve(group, 3000).empty());
}

// Verifies malformed official messages are parsed safely and rejected with clear errors.
static void TestMalformedMessages() {
  auto invalidJoinUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_JOIN\",\"Provider\":\"Other\",\"StationName\":\"Desk\",\"StationUUID\":\"not-a-uuid\"}");
  assert(invalidJoinUuid);
  assert(!mvr::xchange::ValidateMessage(*invalidJoinUuid).empty());

  auto missingProvider = mvr::xchange::ParseMessage("{\"Type\":\"MVR_JOIN\",\"StationName\":\"Desk\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(missingProvider);
  assert(mvr::xchange::ValidateMessage(*missingProvider) == "MVR_JOIN is missing Provider.");

  auto invalidCommitUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_COMMIT\",\"FileUUID\":\"bad\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(invalidCommitUuid);
  assert(mvr::xchange::ValidateMessage(*invalidCommitUuid) == "MVR_COMMIT is missing a valid FileUUID.");

  auto invalidRequestUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_REQUEST\",\"FileUUID\":\"latest\"}");
  assert(invalidRequestUuid);
  assert(mvr::xchange::ValidateMessage(*invalidRequestUuid) == "MVR_REQUEST contains an invalid FileUUID.");

  auto unknown = mvr::xchange::ParseMessage("{\"Type\":\"PRIVATE_SYNC\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(unknown);
  assert(mvr::xchange::ValidateMessage(*unknown) == "Unsupported MVR-xchange message type.");
}

// Verifies MVR-xchange DNS-SD naming helpers.
static void TestDnsNames() {
  assert(mvr::xchange::BuildMvrXchangeGroupServiceName("Default") == "Default._mvrxchange._tcp.local.");
  assert(mvr::xchange::BuildMvrXchangeServiceInstanceName("Perastage", "Default") == "Perastage.Default._mvrxchange._tcp.local.");
  assert(mvr::xchange::BuildMvrXchangeServiceInstanceName("Desk / Main", "Tour Group") == "Desk--Main.Tour-Group._mvrxchange._tcp.local.");
  assert(mvr::xchange::BuildMvrXchangeHostName("PERAMATO-DESKTOP", "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE") == "perastage-PERAMATO-DESKTOP-aaaaaaaabbbb");
  const auto longHost = mvr::xchange::BuildMvrXchangeHostName(std::string(80, 'x'), "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE");
  assert(longHost.size() <= 63 && longHost.compare(longHost.size() - 13, 13, "-aaaaaaaabbbb") == 0);
  assert(mvr::xchange::NormalizeDnsName("Peer.DEFAULT.Local.") == "peer.default.local");
  assert(mvr::xchange::DnsNamesEqual("Peer.Default.local", "peer.default.LOCAL."));
}

// Verifies MVR-xchange UUID helpers use canonical lowercase UUIDs.
static void TestCanonicalUuidUse() {
  const auto generated = GenerateUuid();
  assert(CanonicalizeUuid(generated) == generated);
  assert(CanonicalizeUuid("ABCDEFABCDEFABCDEFABCDEFABCDEFAB") == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(CanonicalizeUuid("ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB") == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(CanonicalizeUuid("not-a-uuid").empty());
}

// Verifies remote station registry self-filtering, deduplication, and join states.
static void TestStationRegistry() {
  MvrXchangeStationRegistry registry;
  registry.SetLocalIdentity("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage.Default._mvrxchange._tcp.local.", "127.0.0.1", 42424);
  MvrXchangeRemoteStation own;
  own.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  own.ipAddress = "127.0.0.1";
  own.port = 42424;
  assert(!registry.UpsertDiscovered(own));
  MvrXchangeStationRegistry endpointRegistry;
  endpointRegistry.SetLocalIdentity("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage.Default._mvrxchange._tcp.local.", "127.0.0.1", 42424);
  MvrXchangeRemoteStation mixedSelf;
  mixedSelf.stationUuid = "99999999-bbbb-cccc-dddd-eeeeeeeeeeee";
  mixedSelf.serviceInstanceName = "Wrong.Default._mvrxchange._tcp.local.";
  mixedSelf.ipAddress = "127.0.0.1";
  mixedSelf.port = 42424;
  assert(!endpointRegistry.UpsertDiscovered(mixedSelf));
  mixedSelf.port = 42425;
  assert(endpointRegistry.UpsertDiscovered(mixedSelf));

  MvrXchangeRemoteStation discovered;
  discovered.stationUuid = "BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF";
  discovered.stationName = "grandMA3";
  discovered.serviceInstanceName = "grandMA3.Default._mvrxchange._tcp.local.";
  discovered.ipAddress = "127.0.0.1";
  discovered.port = 50000;
  assert(registry.UpsertDiscovered(discovered));
  assert(registry.List().size() == 1);
  assert(registry.List()[0].discovered);

  MvrXchangeRemoteStation incoming = discovered;
  incoming.provider = "grandMA3";
  incoming.verMajor = 1;
  incoming.verMinor = 6;
  incoming.inventorySpecified = true;
  incoming.commits.push_back({"cccccccc-dddd-eeee-ffff-000000000000", "bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "remote.mvr", {}, {}, {1}, 0, false, 0, 0, {}, true});
  assert(registry.UpsertIncomingJoin(incoming));
  assert(registry.List().size() == 1);
  assert(registry.List()[0].incomingJoined);
  assert(registry.List()[0].commits.size() == 1);
  assert(registry.MarkOutgoingJoined("bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "127.0.0.1", 50000));
  assert(registry.List()[0].outgoingJoined);
  assert(registry.JoinedStations().size() == 1);
  assert(registry.MarkLeft("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
  assert(!registry.CanSendCommitTo("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
  assert(registry.JoinedStations().empty());
  assert(registry.UpsertIncomingJoin(incoming));
  assert(registry.JoinedStations().size() == 1);
  MvrXchangeRemoteStation outgoing = discovered;
  outgoing.outgoingJoined = true;
  outgoing.inventorySpecified = true;
  outgoing.commits.clear();
  assert(registry.UpsertOutgoingJoin(outgoing));
  assert(registry.List()[0].commits.empty());
  MvrXchangeCommit update{"dddddddd-eeee-ffff-0000-111111111111", "bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "new.mvr", {}, {}, {}, 0, false, 0, 0, {}, true};
  update.declaredFileSize = 42;
  update.declaredFileSizeSpecified = true;
  assert(registry.ApplyCommit(update));
  assert(registry.List()[0].commits.size() == 1 && registry.List()[0].commits[0].payload.empty());
  assert(registry.ApplyCommit(update));
  assert(registry.List()[0].commits.size() == 1);

  assert(registry.MarkLeft("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
  auto rediscovered = discovered;
  rediscovered.ipAddress = "127.0.0.2";
  assert(registry.UpsertDiscovered(rediscovered));
  assert(!registry.ShouldInitiateOutgoingJoin(rediscovered));
  assert(registry.JoinedStations().empty());
  assert(registry.UpsertIncomingJoin(incoming));
  assert(registry.JoinedStations().size() == 1 && registry.List()[0].ipAddress == "127.0.0.1");
  assert(registry.CanSendCommitTo("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));

  auto absentInventory = incoming;
  absentInventory.inventorySpecified = false;
  absentInventory.commits.clear();
  assert(registry.UpsertIncomingJoin(absentInventory));
  assert(registry.List()[0].commits.size() == 1);
  auto emptyInventory = incoming;
  emptyInventory.inventorySpecified = true;
  emptyInventory.commits.clear();
  assert(registry.UpsertIncomingJoin(emptyInventory));
  assert(registry.List()[0].commits.empty());

  MvrXchangeStationRegistry promotion;
  MvrXchangeRemoteStation provisional;
  provisional.serviceInstanceName = "Peer.Default._mvrxchange._tcp.local.";
  provisional.ipAddress = "192.0.2.30";
  provisional.port = 45000;
  assert(promotion.UpsertDiscovered(provisional));
  auto identified = provisional;
  identified.serviceInstanceName = "peer.default._mvrxchange._tcp.local";
  identified.stationUuid = "12345678-2222-3333-4444-555555555555";
  assert(promotion.UpsertDiscovered(identified));
  assert(promotion.List().size() == 1 && promotion.List()[0].stationUuid == identified.stationUuid);
  identified.ipAddress = "192.0.2.31";
  assert(promotion.UpsertDiscovered(identified));
  assert(promotion.List().size() == 1 && promotion.List()[0].ipAddress == "192.0.2.31");
  MvrXchangeRemoteStation reused = provisional;
  reused.stationUuid = "87654321-2222-3333-4444-555555555555";
  reused.serviceInstanceName = "Different.Default._mvrxchange._tcp.local.";
  assert(promotion.UpsertDiscovered(reused));
  assert(promotion.List().size() == 2);
  MvrXchangeCommit unjoinedCommit{"aaaaaaaa-1111-2222-3333-444444444444", reused.stationUuid, "unjoined.mvr", {}, {}, {}, 0, false, 0, 0, {}, true};
  assert(!promotion.ApplyCommit(unjoinedCommit));

  MvrXchangeStationRegistry deferredPromotion;
  assert(deferredPromotion.UpsertDiscovered(provisional));
  MvrXchangeRemoteStation incomingIdentity;
  incomingIdentity.stationUuid = identified.stationUuid;
  incomingIdentity.ipAddress = provisional.ipAddress;
  incomingIdentity.incomingJoined = true;
  assert(deferredPromotion.UpsertIncomingJoin(incomingIdentity));
  assert(deferredPromotion.List().size() == 2);
  assert(deferredPromotion.UpsertDiscovered(identified));
  assert(deferredPromotion.List().size() == 1);
  assert(deferredPromotion.List()[0].incomingJoined && deferredPromotion.List()[0].port == provisional.port);
}

// Verifies the service publication decision path mentions each new commit once.
static void TestPublicationDestinationPolicy() {
  MvrXchangeStationRegistry registry;
  MvrXchangeRemoteStation existing;
  existing.stationUuid = "11111111-2222-3333-4444-555555555555";
  existing.incomingJoined = true;
  assert(registry.UpsertIncomingJoin(existing));
  const mvr::xchange::PublicationSession publication(registry);
  const auto destinations = publication.CommitDestinations();
  int joins = publication.ShouldInitiateJoin(registry, existing) ? 1 : 0;
  int commits = publication.ShouldSendCommit(registry, existing.stationUuid) ? 1 : 0;
  assert(joins == 0 && commits == 1);
  MvrXchangeRemoteStation newlyDiscovered;
  newlyDiscovered.stationUuid = "99999999-2222-3333-4444-555555555555";
  newlyDiscovered.outgoingJoined = true;
  assert(registry.UpsertOutgoingJoin(newlyDiscovered));
  assert(destinations.size() == 1 && destinations[0].stationUuid == existing.stationUuid);
  assert(registry.JoinedStations().size() == 2);

  MvrXchangeStationRegistry newPeerRegistry;
  MvrXchangeRemoteStation newPeer;
  newPeer.stationUuid = "88888888-2222-3333-4444-555555555555";
  assert(newPeerRegistry.UpsertDiscovered(newPeer));
  const mvr::xchange::PublicationSession newPeerPublication(newPeerRegistry);
  const auto newPeerDestinations = newPeerPublication.CommitDestinations();
  joins = newPeerPublication.ShouldInitiateJoin(newPeerRegistry, newPeer) ? 1 : 0;
  assert(newPeerRegistry.UpsertOutgoingJoin(newPeer));
  commits = static_cast<int>(newPeerDestinations.size());
  assert(joins == 1 && commits == 0);

  MvrXchangeStationRegistry outgoingRegistry;
  MvrXchangeRemoteStation outgoing = newPeer;
  assert(outgoingRegistry.UpsertOutgoingJoin(outgoing));
  assert(!outgoingRegistry.ShouldInitiateOutgoingJoin(outgoing));
  assert(mvr::xchange::CapturePublicationDestinations(outgoingRegistry).size() == 1);
  assert(outgoingRegistry.MarkLeft(outgoing.stationUuid));
  assert(!outgoingRegistry.ShouldInitiateOutgoingJoin(outgoing));
  assert(mvr::xchange::CapturePublicationDestinations(outgoingRegistry).empty());
  outgoing.incomingJoined = true;
  assert(outgoingRegistry.UpsertIncomingJoin(outgoing));
  assert(mvr::xchange::CapturePublicationDestinations(outgoingRegistry).size() == 1);
}

// Verifies that loopback is always available for same-machine MVR-xchange tests.
static void TestNetworkInterfaces() {
  const auto interfaces = ListMvrXchangeNetworkInterfaces();
  bool foundLoopback = false;
  for (const auto &iface : interfaces) foundLoopback = foundLoopback || iface.ipv4Address == "127.0.0.1";
  assert(foundLoopback);
  const auto loopback = SelectMvrXchangeNetworkInterface("127.0.0.1");
  assert(loopback.ipv4Address == "127.0.0.1");
}

// Verifies JOIN, COMMIT, and REQUEST use independent matching TCP transactions.
static void TestTcpTransactions() {
  const std::string localUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::string remoteUuid = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";
  MvrXchangeCommit available{"11111111-2222-3333-4444-555555555555", localUuid, "scene.mvr", {}, {}, {'m', 'v', 'r'}, 0, false, 0, 0, {}, true};
  MvrXchangeSettings settings;
  settings.stationName = "Server";
  settings.stationUuid = localUuid;
  settings.port = 0;
  int joins = 0;
  int commits = 0;
  std::string leftStation;
  MvrXchangeTcpServer server;
  assert(server.Start(settings,
                      [&](const std::string &fileUuid) { return fileUuid.empty() || fileUuid == available.fileUuid ? std::optional<MvrXchangeCommit>(available) : std::nullopt; },
                      [&] { return std::vector<MvrXchangeCommit>{available}; }, {},
                      [&](const MvrXchangeRemoteStation &) { ++joins; }, [&](const std::string &uuid) { leftStation = uuid; return std::string{}; },
                      [&](const MvrXchangeCommit &) { ++commits; return std::string{}; }));
  MvrXchangeRemoteStation endpoint;
  endpoint.stationName = "Server";
  endpoint.stationUuid = localUuid;
  endpoint.ipAddress = "127.0.0.1";
  endpoint.port = server.Port();
  MvrXchangeTcpClient client;
  MvrXchangeSettings clientSettings;
  clientSettings.stationName = "Client";
  clientSettings.stationUuid = remoteUuid;
  MvrXchangeRemoteStation joined;
  assert(client.SendJoin(endpoint, clientSettings, {}, joined, {}));
  assert(joins == 1 && joined.stationUuid == localUuid && joined.commits.size() == 1);
  MvrXchangeCommit announcement{"22222222-3333-4444-5555-666666666666", remoteUuid, "remote.mvr", {}, {}, {}, 0, false, 0, 0, {}, true};
  announcement.declaredFileSize = 10;
  announcement.declaredFileSizeSpecified = true;
  assert(client.SendCommit(endpoint, announcement, {}));
  assert(commits == 1);
  for (int i = 0; i < 32; ++i) assert(client.SendCommit(endpoint, announcement, {}));
  assert(commits == 33);
  endpoint.commits = {available};
  endpoint.commits[0].declaredFileSize = 3;
  endpoint.commits[0].declaredFileSizeSpecified = true;
  std::vector<std::string> requestLogs;
  const auto requested = client.RequestCommit(endpoint, available.fileUuid, localUuid, [&](const std::string &message) { requestLogs.push_back(message); });
  assert(requested && requested->payload == available.payload);
  assert(std::find(requestLogs.begin(), requestLogs.end(),
                   "MVR-xchange requesting FileUUID=" + available.fileUuid + " from StationUUID=" + localUuid + ".") != requestLogs.end());
  assert(std::none_of(requestLogs.begin(), requestLogs.end(), [&](const std::string &message) {
    return message.find("from StationUUID=" + remoteUuid) != std::string::npos;
  }));
  assert(client.SendLeave(endpoint, remoteUuid, {}));
  assert(leftStation == remoteUuid);
  const auto stopStarted = std::chrono::steady_clock::now();
  server.Stop();
  assert(std::chrono::steady_clock::now() - stopStarted < std::chrono::seconds(2));
}

// Verifies joined peers can issue multiple messages on one persistent TCP connection.
static void TestPersistentTcpConnection() {
  const std::string localUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::string peerUuid = "11111111-2222-3333-4444-555555555555";
  MvrXchangeCommit available{"abcdefab-cdef-abcd-efab-cdefabcdefab", localUuid, "persistent.mvr", {}, {}, {'m', 'v', 'r'}, 0, false, 0, 0, {}, true};
  MvrXchangeSettings settings;
  settings.stationName = "Persistent server";
  settings.stationUuid = localUuid;
  int joins = 0;
  int commits = 0;
  int leaves = 0;
  MvrXchangeTcpServer server;
  assert(server.Start(settings,
                      [&](const std::string &fileUuid) { return fileUuid == available.fileUuid ? std::optional<MvrXchangeCommit>(available) : std::nullopt; },
                      [&] { return std::vector<MvrXchangeCommit>{available}; }, {},
                      [&](const MvrXchangeRemoteStation &) { ++joins; },
                      [&](const std::string &uuid) { ++leaves; return uuid == peerUuid ? std::string{} : "Unexpected station."; },
                      [&](const MvrXchangeCommit &) { ++commits; return std::string{}; }));

  const std::string join = mvr::xchange::BuildJoin(peerUuid, "DMXRouter", {});
  auto reconnectingPeer = ConnectLoopback(server.Port());
  SendPersistentJson(reconnectingPeer, join);
  auto packet = ReceivePersistentPacket(reconnectingPeer);
  assert(packet.type == mvr::xchange::PacketType::Json);
  CloseLoopback(reconnectingPeer);

  auto persistentPeer = ConnectLoopback(server.Port());
  SendPersistentJson(persistentPeer, join);
  packet = ReceivePersistentPacket(persistentPeer);
  auto joinRet = mvr::xchange::ParseMessage(std::string(packet.payload.begin(), packet.payload.end()));
  assert(joinRet && joinRet->type == "MVR_JOIN_RET");
  SendPersistentJson(persistentPeer, join);
  packet = ReceivePersistentPacket(persistentPeer);
  joinRet = mvr::xchange::ParseMessage(std::string(packet.payload.begin(), packet.payload.end()));
  assert(joinRet && joinRet->type == "MVR_JOIN_RET" && joins == 3);

  MvrXchangeCommit incoming{"22222222-3333-4444-5555-666666666666", peerUuid, "remote.mvr", {}, {}, {}, 0, false, 0, 0, {}, true};
  incoming.declaredFileSize = 0;
  incoming.declaredFileSizeSpecified = true;
  SendPersistentJson(persistentPeer, mvr::xchange::BuildCommit(incoming));
  packet = ReceivePersistentPacket(persistentPeer);
  auto commitRet = mvr::xchange::ParseMessage(std::string(packet.payload.begin(), packet.payload.end()));
  assert(commitRet && commitRet->type == "MVR_COMMIT_RET" && commitRet->ok && commits == 1);

  SendPersistentJson(persistentPeer, mvr::xchange::BuildRequest(available.fileUuid, {localUuid}));
  packet = ReceivePersistentPacket(persistentPeer);
  assert(packet.type == mvr::xchange::PacketType::MvrFile && packet.payload == available.payload);
  SendPersistentJson(persistentPeer, mvr::xchange::BuildLeave(peerUuid));
  packet = ReceivePersistentPacket(persistentPeer);
  auto leaveRet = mvr::xchange::ParseMessage(std::string(packet.payload.begin(), packet.payload.end()));
  assert(leaveRet && leaveRet->type == "MVR_LEAVE_RET" && leaveRet->ok && leaves == 1);
  CloseLoopback(persistentPeer);
  server.Stop();
}

// Verifies an idle listener stops promptly without relying on accept interruption.
static void TestTcpServerIdleStop() {
  MvrXchangeSettings settings;
  settings.stationName = "Idle server";
  settings.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  for (int iteration = 0; iteration < 10; ++iteration) {
    MvrXchangeTcpServer server;
    assert(server.Start(settings, {}, {}, {}, {}, {}, {}));
    auto idleClient = ConnectLoopback(server.Port());
    const auto stopStarted = std::chrono::steady_clock::now();
    server.Stop();
    assert(std::chrono::steady_clock::now() - stopStarted < std::chrono::seconds(2));
    CloseLoopback(idleClient);
  }
}

// Verifies persistent clients remain bounded by the server connection limit.
static void TestPersistentConnectionLimit() {
  MvrXchangeSettings settings;
  settings.stationName = "Connection limit server";
  settings.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  std::mutex logMutex;
  std::condition_variable logChanged;
  std::size_t acceptedConnections = 0;
  bool connectionRejected = false;
  MvrXchangeTcpServer server;
  assert(server.Start(settings, {}, {},
                      [&](const std::string &message) {
                        std::lock_guard lock(logMutex);
                        if (message.find("TCP client connected") != std::string::npos) ++acceptedConnections;
                        if (message.find("connection limit was reached") != std::string::npos) connectionRejected = true;
                        logChanged.notify_all();
                      },
                      {}, {}, {}));
  std::vector<LoopbackConnection> clients;
  for (int index = 0; index < 16; ++index) {
    clients.push_back(ConnectLoopback(server.Port()));
    std::unique_lock lock(logMutex);
    const bool accepted = logChanged.wait_for(lock, std::chrono::seconds(2), [&] {
      return acceptedConnections == clients.size();
    });
    assert(accepted);
  }
  auto rejected = ConnectLoopback(server.Port());
  {
    std::unique_lock lock(logMutex);
    const bool rejectedAtLimit = logChanged.wait_for(lock, std::chrono::seconds(2), [&] { return connectionRejected; });
    assert(rejectedAtLimit);
  }
  const auto closeResult = WaitForPeerClose(rejected.fd, std::chrono::seconds(2));
  if (closeResult != PeerCloseResult::Closed) {
    std::cerr << "Rejected MVR-xchange connection was not closed within the test deadline; result="
              << (closeResult == PeerCloseResult::Timeout ? "timeout" :
                  closeResult == PeerCloseResult::UnexpectedData ? "unexpected data" : "socket error")
              << std::endl;
    std::abort();
  }
  CloseLoopback(rejected);
  for (auto &client : clients) CloseLoopback(client);
  server.Stop();
}

// Verifies the server returns the RET matching each recognizable malformed request.
static void TestTcpTypedErrorResponses() {
  MvrXchangeSettings settings;
  settings.stationName = "Error server";
  settings.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  MvrXchangeTcpServer server;
  assert(server.Start(settings, {}, {}, {}, {}, {}, {}));
  const std::vector<std::pair<std::string, std::string>> cases = {
      {R"({"Type":"MVR_JOIN","Commits":{}})", "MVR_JOIN_RET"},
      {R"({"Type":"MVR_COMMIT","FileUUID":"bad"})", "MVR_COMMIT_RET"},
      {R"({"Type":"MVR_LEAVE","FromStationUUID":"bad"})", "MVR_LEAVE_RET"},
      {R"({"Type":"MVR_REQUEST","FileUUID":"bad"})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FileUUID":123})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":123})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":{}})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":["bad"]})", "MVR_REQUEST_RET"},
      {R"({"Type":"MVR_REQUEST","FromStationUUID":["aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",123]})", "MVR_REQUEST_RET"}};
  for (const auto &[request, expectedType] : cases) {
    const auto response = nlohmann::json::parse(ExchangeRawJson(server.Port(), request));
    assert(response["Type"] == expectedType && response["OK"] == false);
  }
  server.Stop();
}

// Runs focused non-GUI MVR-xchange protocol coverage.
int main() {
  [[maybe_unused]] TestSocketRuntime socketRuntime;
  TestCommitStore();
  TestPackets();
  TestMessages();
  TestRequestSourceStations();
  TestLeaveMessages();
  TestTypedErrors();
  TestMalformedMessages();
  TestInventoryCompatibility();
  TestPacketRejection();
  TestMdnsRecordCache();
  TestRawMdnsDatagram();
  TestDnsNames();
  TestCanonicalUuidUse();
  TestStationRegistry();
  TestPublicationDestinationPolicy();
  TestNetworkInterfaces();
  TestTcpTransactions();
  TestPersistentTcpConnection();
  TestTcpServerIdleStop();
  TestPersistentConnectionLimit();
  TestTcpTypedErrorResponses();
  return 0;
}
