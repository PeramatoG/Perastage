#include "mvr_xchange_mdns_discovery.h"
#include "mvr_xchange_dns_names.h"
#include "mvr_xchange_network_interfaces.h"
#include "../../core/uuidutils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <thread>
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
#include <mdns.h>
#endif
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
constexpr int kReceiveWindowMs = 250;

struct DiscoveryContext {
  std::string groupServiceName;
  std::map<std::string, MvrXchangeRemoteStation> byInstance;
  std::map<std::string, std::string> hostAddresses;
};

// Converts an mdns_string_t view to a std::string.
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
std::string ToString(mdns_string_t value) { return {value.str, value.length}; }

// Extracts the record owner name from an mDNS callback.
std::string ExtractRecordName(const void *data, size_t size, size_t nameOffset) {
  char buffer[512]{};
  size_t offset = nameOffset;
  return ToString(mdns_string_extract(data, size, &offset, buffer, sizeof(buffer)));
}

// Converts a sockaddr_in address into a dotted IPv4 address string.
std::string Ipv4ToString(const sockaddr_in &addr) {
  char buffer[INET_ADDRSTRLEN]{};
  inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
  return buffer;
}

// Returns a mutable station entry for an mDNS service instance.
MvrXchangeRemoteStation &StationForInstance(DiscoveryContext &context, const std::string &instanceName) {
  auto &station = context.byInstance[instanceName];
  station.serviceInstanceName = instanceName;
  station.discovered = true;
  return station;
}

// Updates station state from one TXT record key/value pair.
void ApplyTxtRecord(MvrXchangeRemoteStation &station, const mdns_record_txt_t &record) {
  const std::string key(record.key.str, record.key.length);
  const std::string value(record.value.str, record.value.length);
  if (key == "StationName") station.stationName = value;
  else if (key == "StationUUID") station.stationUuid = CanonicalizeUuid(value);
}

// Handles records returned by mdns_query_recv for service discovery.
static int DiscoveryCallback(int, const sockaddr *, size_t, mdns_entry_type_t entry, uint16_t, uint16_t rtype, uint16_t, uint32_t, const void *data, size_t size, size_t nameOffset, size_t, size_t recordOffset, size_t recordLength, void *userData) {
  if (entry != MDNS_ENTRYTYPE_ANSWER && entry != MDNS_ENTRYTYPE_ADDITIONAL) return 0;
  auto *context = static_cast<DiscoveryContext *>(userData);
  const std::string recordName = ExtractRecordName(data, size, nameOffset);
  if (rtype == MDNS_RECORDTYPE_PTR && (recordName == context->groupServiceName || recordName == mvr::xchange::kMvrXchangeServiceType)) {
    char ptrBuffer[512]{};
    const std::string instanceName = ToString(mdns_record_parse_ptr(data, size, recordOffset, recordLength, ptrBuffer, sizeof(ptrBuffer)));
    if (!instanceName.empty()) StationForInstance(*context, instanceName);
  } else if (rtype == MDNS_RECORDTYPE_SRV) {
    char srvBuffer[512]{};
    const mdns_record_srv_t srv = mdns_record_parse_srv(data, size, recordOffset, recordLength, srvBuffer, sizeof(srvBuffer));
    auto &station = StationForInstance(*context, recordName);
    station.hostName = ToString(srv.name);
    station.port = srv.port;
  } else if (rtype == MDNS_RECORDTYPE_TXT) {
    auto &station = StationForInstance(*context, recordName);
    std::array<mdns_record_txt_t, 8> txtRecords{};
    const size_t count = mdns_record_parse_txt(data, size, recordOffset, recordLength, txtRecords.data(), txtRecords.size());
    for (size_t i = 0; i < count; ++i) ApplyTxtRecord(station, txtRecords[i]);
  } else if (rtype == MDNS_RECORDTYPE_A) {
    sockaddr_in address{};
    if (mdns_record_parse_a(data, size, recordOffset, recordLength, &address)) context->hostAddresses[recordName] = Ipv4ToString(address);
  }
  return 0;
}

// Receives mDNS responses for a short bounded interval.
void ReceiveResponses(int socketFd, DiscoveryContext &context) {
  std::array<char, 4096> buffer{};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReceiveWindowMs);
  while (std::chrono::steady_clock::now() < deadline) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socketFd, &readSet);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if (select(socketFd + 1, &readSet, nullptr, nullptr, &timeout) > 0 && FD_ISSET(socketFd, &readSet))
      mdns_query_recv(socketFd, buffer.data(), buffer.size(), DiscoveryCallback, &context, 0);
  }
}

// Sends one mDNS query and receives responses for a bounded interval.
void QueryAndReceive(int socketFd, mdns_record_type_t type, const std::string &name, DiscoveryContext &context) {
  std::array<char, 2048> buffer{};
  mdns_query_send(socketFd, type, name.c_str(), name.size(), buffer.data(), buffer.size(), 0);
  ReceiveResponses(socketFd, context);
}

// Opens a query socket for the selected MVR-xchange interface.
int OpenQuerySocket(const MvrXchangeSettings &settings) {
  const auto selected = SelectMvrXchangeNetworkInterface(settings.selectedInterfaceId);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = 0;
  inet_pton(AF_INET, selected.ipv4Address.c_str(), &address.sin_addr);
  return mdns_socket_open_ipv4(&address);
}
#endif

// Returns true if a station matches this Perastage instance.
bool IsSelfStation(const MvrXchangeRemoteStation &station, const std::string &localInstanceName, const std::string &localStationUuid, const std::string &localIpAddress, int localPort) {
  if (!station.stationUuid.empty() && station.stationUuid == localStationUuid) return true;
  if (!station.serviceInstanceName.empty() && station.serviceInstanceName == localInstanceName) return true;
  return station.port > 0 && station.port == localPort && station.ipAddress == localIpAddress;
}
}

// Discovers MVR-xchange stations registered in the selected group subservice.
std::vector<MvrXchangeRemoteStation> MvrXchangeMdnsDiscovery::DiscoverStations(const MvrXchangeSettings &settings,
                                                                                const std::string &localInstanceName,
                                                                                const std::string &localStationUuid,
                                                                                const std::string &localIpAddress,
                                                                                int localPort,
                                                                                LogCallback logCallback) {
  const std::string groupServiceName = mvr::xchange::BuildMvrXchangeGroupServiceName(settings.groupName);
  if (logCallback) logCallback("MVR-xchange querying group service: " + groupServiceName);
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  const int socketFd = OpenQuerySocket(settings);
  if (socketFd < 0) {
    if (logCallback) logCallback("MVR-xchange mDNS discovery could not open a query socket.");
    return {};
  }
  DiscoveryContext context;
  context.groupServiceName = groupServiceName;
  QueryAndReceive(socketFd, MDNS_RECORDTYPE_PTR, groupServiceName, context);
  std::set<std::string> queriedHosts;
  for (const auto &entry : context.byInstance) {
    QueryAndReceive(socketFd, MDNS_RECORDTYPE_SRV, entry.first, context);
    QueryAndReceive(socketFd, MDNS_RECORDTYPE_TXT, entry.first, context);
  }
  for (const auto &entry : context.byInstance) {
    if (!entry.second.hostName.empty() && queriedHosts.insert(entry.second.hostName).second)
      QueryAndReceive(socketFd, MDNS_RECORDTYPE_A, entry.second.hostName, context);
  }
  mdns_socket_close(socketFd);
  std::vector<MvrXchangeRemoteStation> stations;
  const std::string canonicalLocalUuid = CanonicalizeUuid(localStationUuid);
  for (auto &[instanceName, station] : context.byInstance) {
    if (station.ipAddress.empty() && !station.hostName.empty()) station.ipAddress = context.hostAddresses[station.hostName];
    station.stationUuid = CanonicalizeUuid(station.stationUuid);
    if (station.stationName.empty()) station.stationName = station.serviceInstanceName.substr(0, station.serviceInstanceName.find('.'));
    if (station.ipAddress.empty() || station.port <= 0) {
      if (logCallback) logCallback("MVR-xchange discovery ignored incomplete station " + station.serviceInstanceName + ".");
      continue;
    }
    if (IsSelfStation(station, localInstanceName, canonicalLocalUuid, localIpAddress, localPort)) continue;
    if (logCallback) logCallback("MVR-xchange discovered station:\n  instance=" + station.serviceInstanceName + "\n  station=" + station.stationName + "\n  uuid=" + station.stationUuid + "\n  host=" + station.hostName + "\n  ip=" + station.ipAddress + "\n  port=" + std::to_string(station.port));
    stations.push_back(std::move(station));
  }
  return stations;
#else
  if (logCallback) logCallback("MVR-xchange mDNS discovery is unavailable because this build was configured without the vcpkg mdns backend.");
  return {};
#endif
}
