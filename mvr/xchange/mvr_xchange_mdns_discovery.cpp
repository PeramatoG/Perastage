#include "mvr_xchange_mdns_discovery.h"
#include "mvr_xchange_dns_names.h"
#include "mvr_xchange_network_interfaces.h"
#include "../../core/uuidutils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <limits>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
constexpr std::uint16_t kMdnsPort = 5353;
constexpr std::uint32_t kInitialQueryIntervalSeconds = 3;
constexpr std::uint32_t kSteadyQueryIntervalSeconds = 30;

// Returns monotonic milliseconds for TTL and scheduling calculations.
std::uint64_t MonotonicMilliseconds() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Closes a multicast socket on the current platform.
void CloseSocket(std::intptr_t socketFd) {
#ifdef _WIN32
  closesocket(static_cast<SOCKET>(socketFd));
#else
  close(static_cast<int>(socketFd));
#endif
}

// Reads one unsigned 16-bit DNS field in network order.
bool ReadU16(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint16_t &value) {
  if (offset > size || size - offset < 2) return false;
  value = static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]);
  offset += 2;
  return true;
}

// Reads one unsigned 32-bit DNS field in network order.
bool ReadU32(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint32_t &value) {
  if (offset > size || size - offset < 4) return false;
  value = (static_cast<std::uint32_t>(data[offset]) << 24) |
          (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
  offset += 4;
  return true;
}

// Decodes a bounded DNS name including RFC compression pointers.
bool ReadDnsName(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::string &name) {
  std::size_t cursor = offset;
  std::size_t resume = offset;
  bool jumped = false;
  std::size_t jumps = 0;
  name.clear();
  while (cursor < size && jumps <= 32) {
    const std::uint8_t length = data[cursor++];
    if (length == 0) { offset = jumped ? resume : cursor; if (!name.empty()) name.push_back('.'); return true; }
    if ((length & 0xc0) == 0xc0) {
      if (cursor >= size) return false;
      const std::size_t pointer = ((length & 0x3f) << 8) | data[cursor++];
      if (pointer >= size) return false;
      if (!jumped) resume = cursor;
      jumped = true;
      cursor = pointer;
      ++jumps;
      continue;
    }
    if ((length & 0xc0) != 0 || length > 63 || cursor > size || size - cursor < length) return false;
    if (!name.empty()) name.push_back('.');
    name.append(reinterpret_cast<const char *>(data + cursor), length);
    cursor += length;
  }
  return false;
}

// Appends a DNS name without compression to an outgoing query.
bool AppendDnsName(std::vector<std::uint8_t> &packet, const std::string &name) {
  std::size_t begin = 0;
  const std::string normalized = name.empty() || name.back() == '.' ? name.substr(0, name.size() - (name.empty() ? 0 : 1)) : name;
  while (begin < normalized.size()) {
    const std::size_t end = normalized.find('.', begin);
    const std::size_t length = (end == std::string::npos ? normalized.size() : end) - begin;
    if (length == 0 || length > 63 || packet.size() > 512 - length - 1) return false;
    packet.push_back(static_cast<std::uint8_t>(length));
    packet.insert(packet.end(), normalized.begin() + begin, normalized.begin() + begin + length);
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  packet.push_back(0);
  return true;
}

// Converts an ASCII TXT key to its case-insensitive cache form.
std::string NormalizeTxtKey(std::string key) {
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return key;
}

}

// Creates an inactive persistent mDNS browser.
MvrXchangeMdnsDiscovery::MvrXchangeMdnsDiscovery() = default;

// Stops the browser before releasing its owned socket and thread.
MvrXchangeMdnsDiscovery::~MvrXchangeMdnsDiscovery() { Stop(); }

// Starts persistent multicast browsing on the selected interface.
bool MvrXchangeMdnsDiscovery::Start(const MvrXchangeSettings &settings, const std::string &localInstanceName,
                                      const std::string &localStationUuid, LogCallback logCallback) {
  Stop();
#ifndef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  (void)settings;
  (void)localInstanceName;
  (void)localStationUuid;
  if (logCallback) logCallback("MVR-xchange mDNS discovery is unavailable because this build was configured without the vcpkg mdns backend.");
  return false;
#else
  settings_ = settings;
  groupServiceName_ = mvr::xchange::BuildMvrXchangeGroupServiceName(settings.groupName);
  localInstanceName_ = localInstanceName;
  localStationUuid_ = CanonicalizeUuid(localStationUuid);
  logCallback_ = std::move(logCallback);
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
  networkInitialized_ = true;
#endif
  if (!OpenSocket()) {
#ifdef _WIN32
    WSACleanup();
    networkInitialized_ = false;
#endif
    return false;
  }
  running_ = true;
  queryRequested_ = true;
  worker_ = std::thread(&MvrXchangeMdnsDiscovery::Run, this);
  return true;
#endif
}

// Stops browsing and deterministically unblocks the receive loop.
void MvrXchangeMdnsDiscovery::Stop() {
  running_ = false;
  if (worker_.joinable()) worker_.join();
  const auto socketFd = socket_;
  socket_ = -1;
  if (socketFd >= 0) CloseSocket(socketFd);
#ifdef _WIN32
  if (networkInitialized_) WSACleanup();
  networkInitialized_ = false;
#endif
  std::lock_guard lock(mutex_);
  cache_.Clear();
}

// Requests an immediate query without replacing the persistent browser.
void MvrXchangeMdnsDiscovery::QueryNow() { queryRequested_ = true; }

// Returns currently resolved, non-expired stations from the record cache.
std::vector<MvrXchangeRemoteStation> MvrXchangeMdnsDiscovery::Snapshot() {
  std::lock_guard lock(mutex_);
  const auto now = MonotonicMilliseconds();
  cache_.Expire(now);
  auto stations = cache_.Resolve(groupServiceName_, now);
  stations.erase(std::remove_if(stations.begin(), stations.end(), [&](const auto &station) {
    return (!station.stationUuid.empty() && station.stationUuid == localStationUuid_) ||
           (!station.serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(station.serviceInstanceName, localInstanceName_));
  }), stations.end());
  return stations;
}

// Reports whether the persistent browser thread owns a live socket.
bool MvrXchangeMdnsDiscovery::IsRunning() const { return running_; }

// Opens, reuses, binds, and joins the IPv4 mDNS multicast socket.
bool MvrXchangeMdnsDiscovery::OpenSocket() {
#ifdef _WIN32
  SOCKET fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == INVALID_SOCKET) return false;
#else
  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) return false;
#endif
  int enabled = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&enabled), sizeof(enabled)) != 0) { CloseSocket(fd); return false; }
#if defined(SO_REUSEPORT) && !defined(_WIN32)
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enabled, sizeof(enabled)) != 0) { CloseSocket(fd); return false; }
#endif
  sockaddr_in bindAddress{};
  bindAddress.sin_family = AF_INET;
  bindAddress.sin_port = htons(kMdnsPort);
  bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, reinterpret_cast<sockaddr *>(&bindAddress), sizeof(bindAddress)) != 0) { CloseSocket(fd); return false; }
  const auto selected = SelectMvrXchangeNetworkInterface(settings_.selectedInterfaceId);
  ip_mreq membership{};
  inet_pton(AF_INET, "224.0.0.251", &membership.imr_multiaddr);
  inet_pton(AF_INET, selected.ipv4Address.c_str(), &membership.imr_interface);
  if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char *>(&membership), sizeof(membership)) != 0) { CloseSocket(fd); return false; }
  setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char *>(&membership.imr_interface), sizeof(membership.imr_interface));
  socket_ = static_cast<std::intptr_t>(fd);
  return true;
}

// Continuously receives announcements and sends periodic discovery queries.
void MvrXchangeMdnsDiscovery::Run() {
  auto nextQuery = std::chrono::steady_clock::now();
  bool initial = true;
  while (running_) {
    const auto now = std::chrono::steady_clock::now();
    if (queryRequested_.exchange(false) || now >= nextQuery) {
      SendQueries();
      nextQuery = now + std::chrono::seconds(initial ? kInitialQueryIntervalSeconds : kSteadyQueryIntervalSeconds);
      initial = false;
    }
    fd_set readSet;
    FD_ZERO(&readSet);
    const auto fd = socket_;
    if (fd < 0) break;
#ifdef _WIN32
    const SOCKET nativeFd = static_cast<SOCKET>(fd);
#else
    const int nativeFd = static_cast<int>(fd);
#endif
    FD_SET(nativeFd, &readSet);
    timeval timeout{};
    timeout.tv_usec = 200000;
    if (select(static_cast<int>(nativeFd) + 1, &readSet, nullptr, nullptr, &timeout) > 0 && FD_ISSET(nativeFd, &readSet)) ReceiveDatagram();
    std::lock_guard lock(mutex_);
    cache_.Expire(MonotonicMilliseconds());
  }
}

// Sends PTR questions for the official group and base service names.
void MvrXchangeMdnsDiscovery::SendQueries() {
  for (const std::string &name : {groupServiceName_, std::string(mvr::xchange::kMvrXchangeServiceType)}) {
    std::vector<std::uint8_t> packet(12, 0);
    packet[5] = 1;
    if (!AppendDnsName(packet, name)) continue;
    packet.push_back(0); packet.push_back(12);
    packet.push_back(0); packet.push_back(1);
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(kMdnsPort);
    inet_pton(AF_INET, "224.0.0.251", &destination.sin_addr);
#ifdef _WIN32
    const SOCKET nativeFd = static_cast<SOCKET>(socket_);
#else
    const int nativeFd = static_cast<int>(socket_);
#endif
    sendto(nativeFd, reinterpret_cast<const char *>(packet.data()), static_cast<int>(packet.size()), 0,
           reinterpret_cast<sockaddr *>(&destination), sizeof(destination));
  }
}

// Receives and parses one multicast response or unsolicited announcement.
void MvrXchangeMdnsDiscovery::ReceiveDatagram() {
  std::array<std::uint8_t, 9000> buffer{};
#ifdef _WIN32
  const SOCKET nativeFd = static_cast<SOCKET>(socket_);
#else
  const int nativeFd = static_cast<int>(socket_);
#endif
  const int received = static_cast<int>(recv(nativeFd, reinterpret_cast<char *>(buffer.data()), static_cast<int>(buffer.size()), 0));
  if (received > 0) ApplyDatagram(buffer.data(), static_cast<std::size_t>(received), 0);
}

// Parses one DNS datagram into bounded answer and additional records.
std::vector<mvr::xchange::DnsRecord> mvr::xchange::ParseMdnsRecords(const std::uint8_t *data, std::size_t size,
                                                                    std::uint32_t interfaceIndex, std::uint64_t nowMonotonicMs) {
  std::vector<mvr::xchange::DnsRecord> parsedRecords;
  if (size < 12) return parsedRecords;
  std::size_t offset = 4;
  std::uint16_t questions = 0, answers = 0, authorities = 0, additionals = 0;
  if (!ReadU16(data, size, offset, questions) || !ReadU16(data, size, offset, answers) ||
      !ReadU16(data, size, offset, authorities) || !ReadU16(data, size, offset, additionals)) return parsedRecords;
  for (std::uint16_t i = 0; i < questions; ++i) {
    std::string ignored;
    if (!ReadDnsName(data, size, offset, ignored) || offset > size || size - offset < 4) return {};
    offset += 4;
  }
  const std::uint32_t recordCount = static_cast<std::uint32_t>(answers) + authorities + additionals;
  for (std::uint32_t i = 0; i < recordCount; ++i) {
    mvr::xchange::DnsRecord record;
    if (!ReadDnsName(data, size, offset, record.owner)) return {};
    std::uint16_t type = 0, dnsClass = 0, length = 0;
    std::uint32_t ttl = 0;
    if (!ReadU16(data, size, offset, type) || !ReadU16(data, size, offset, dnsClass) ||
        !ReadU32(data, size, offset, ttl) || !ReadU16(data, size, offset, length) || offset > size || size - offset < length) return {};
    const std::size_t recordEnd = offset + length;
    record.ttlSeconds = ttl;
    record.interfaceIndex = interfaceIndex;
    record.lastSeenMonotonicMs = nowMonotonicMs;
    bool supported = true;
    if (type == 12) { record.type = mvr::xchange::DnsRecordType::Ptr; supported = ReadDnsName(data, size, offset, record.target); }
    else if (type == 33) {
      record.type = mvr::xchange::DnsRecordType::Srv;
      std::uint16_t ignored = 0;
      supported = ReadU16(data, size, offset, ignored) && ReadU16(data, size, offset, ignored) && ReadU16(data, size, offset, record.port) && ReadDnsName(data, size, offset, record.target);
    } else if (type == 16) {
      record.type = mvr::xchange::DnsRecordType::Txt;
      while (offset < recordEnd) {
        const std::size_t textLength = data[offset++];
        if (offset > recordEnd || recordEnd - offset < textLength) { supported = false; break; }
        const std::string item(reinterpret_cast<const char *>(data + offset), textLength);
        const auto separator = item.find('=');
        record.text[NormalizeTxtKey(item.substr(0, separator))] = separator == std::string::npos ? std::string{} : item.substr(separator + 1);
        offset += textLength;
      }
    } else if (type == 1 && length == 4) {
      record.type = mvr::xchange::DnsRecordType::A;
      char address[INET_ADDRSTRLEN]{};
      supported = inet_ntop(AF_INET, data + offset, address, sizeof(address)) != nullptr;
      record.address = address;
    } else if (type == 28 && length == 16) {
      record.type = mvr::xchange::DnsRecordType::Aaaa;
      char address[INET6_ADDRSTRLEN]{};
      supported = inet_ntop(AF_INET6, data + offset, address, sizeof(address)) != nullptr;
      record.address = address;
    } else supported = false;
    offset = recordEnd;
    if (supported) parsedRecords.push_back(std::move(record));
  }
  return parsedRecords;
}

// Applies one parsed DNS datagram to the persistent record cache.
void MvrXchangeMdnsDiscovery::ApplyDatagram(const std::uint8_t *data, std::size_t size, std::uint32_t interfaceIndex) {
  auto parsedRecords = mvr::xchange::ParseMdnsRecords(data, size, interfaceIndex, MonotonicMilliseconds());
  if (!parsedRecords.empty()) { std::lock_guard lock(mutex_); cache_.ApplyBatch(std::move(parsedRecords)); }
}
