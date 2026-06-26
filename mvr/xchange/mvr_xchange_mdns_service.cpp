#include "mvr_xchange_mdns_service.h"
#include "mvr_xchange_network_interfaces.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>
#include <wx/log.h>
#include <wx/string.h>
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
#include <mdns.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
constexpr const char *kServiceType = "_mvrxchange._tcp.local.";
constexpr const char *kDiscoveryService = "_services._dns-sd._udp.local.";

// Converts a std::string into an mdns_string_t view.
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
mdns_string_t MdnsString(const std::string &value) { return {value.c_str(), value.size()}; }

// Converts a null-terminated string literal into an mdns_string_t view.
mdns_string_t MdnsStringLiteral(const char *value) { return {value, std::strlen(value)}; }
#endif

// Sanitizes one DNS label while keeping user-visible names recognizable.
std::string SanitizeDnsLabel(const std::string &value, const std::string &fallback) {
  std::string out;
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-') out.push_back(static_cast<char>(ch));
    else if (ch == ' ' || ch == '_' || ch == '.') out.push_back('-');
  }
  while (!out.empty() && out.front() == '-') out.erase(out.begin());
  while (!out.empty() && out.back() == '-') out.pop_back();
  if (out.empty()) out = fallback;
  if (out.size() > 63) out.resize(63);
  return out;
}

// Returns the local host name without relying on platform-specific GUI APIs.
std::string LocalHostName() {
  char hostname[256]{};
  if (gethostname(hostname, sizeof(hostname)) != 0 || hostname[0] == '\0') return "perastage";
  return SanitizeDnsLabel(hostname, "perastage");
}

// Returns the first available IPv4 address for A-record responses.
sockaddr_in Ipv4AddressFromString(const std::string &ipAddress) {
  sockaddr_in fallback{};
  fallback.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &fallback.sin_addr);
  sockaddr_in selected{};
  selected.sin_family = AF_INET;
  if (inet_pton(AF_INET, ipAddress.c_str(), &selected.sin_addr) == 1) return selected;
  return fallback;
}

// Returns local address diagnostics for mDNS troubleshooting logs.
std::string LocalAddressSummary() {
  const std::string host = LocalHostName();
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *result = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) return host;
  std::vector<std::string> addresses;
  for (addrinfo *it = result; it; it = it->ai_next) {
    char address[NI_MAXHOST]{};
    if (getnameinfo(it->ai_addr, static_cast<socklen_t>(it->ai_addrlen), address, sizeof(address), nullptr, 0, NI_NUMERICHOST) == 0)
      addresses.emplace_back(address);
  }
  freeaddrinfo(result);
  std::ostringstream out;
  out << host;
  if (!addresses.empty()) {
    out << " [";
    for (std::size_t i = 0; i < addresses.size(); ++i) {
      if (i > 0) out << ", ";
      out << addresses[i];
    }
    out << "]";
  }
  return out.str();
}

#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
struct MdnsServiceRecords {
  mdns_record_t discoveryPtr{};
  mdns_record_t ptr{};
  mdns_record_t groupPtr{};
  mdns_record_t srv{};
  mdns_record_t txtName{};
  mdns_record_t txtUuid{};
  mdns_record_t a{};
  std::array<mdns_record_t, 4> additional{};
};

// Builds mDNS records for answering service, group, instance, and host queries.
MdnsServiceRecords BuildRecords(const MvrXchangeMdnsService *service, const sockaddr_in &address) {
  MdnsServiceRecords records;
  records.discoveryPtr.name = MdnsStringLiteral(kDiscoveryService);
  records.discoveryPtr.type = MDNS_RECORDTYPE_PTR;
  records.discoveryPtr.data.ptr.name = MdnsStringLiteral(kServiceType);
  records.ptr.name = MdnsStringLiteral(kServiceType);
  records.ptr.type = MDNS_RECORDTYPE_PTR;
  records.ptr.data.ptr.name = MdnsString(service->ServiceInstanceName());
  records.groupPtr.name = MdnsString(service->GroupServiceName());
  records.groupPtr.type = MDNS_RECORDTYPE_PTR;
  records.groupPtr.data.ptr.name = MdnsString(service->ServiceInstanceName());
  records.srv.name = MdnsString(service->ServiceInstanceName());
  records.srv.type = MDNS_RECORDTYPE_SRV;
  records.srv.data.srv.priority = 0;
  records.srv.data.srv.weight = 0;
  records.srv.data.srv.port = static_cast<uint16_t>(service->Port());
  records.srv.data.srv.name = MdnsString(service->QualifiedHostName());
  records.txtName.name = MdnsString(service->ServiceInstanceName());
  records.txtName.type = MDNS_RECORDTYPE_TXT;
  records.txtName.data.txt.key = {"StationName", 11};
  records.txtName.data.txt.value = MdnsString(service->StationName());
  records.txtUuid.name = MdnsString(service->ServiceInstanceName());
  records.txtUuid.type = MDNS_RECORDTYPE_TXT;
  records.txtUuid.data.txt.key = {"StationUUID", 11};
  records.txtUuid.data.txt.value = MdnsString(service->StationUuid());
  records.a.name = MdnsString(service->QualifiedHostName());
  records.a.type = MDNS_RECORDTYPE_A;
  records.a.data.a.addr = address;
  records.additional = {records.srv, records.txtName, records.txtUuid, records.a};
  return records;
}

// Sends either unicast or multicast answers for a matching mDNS question.
void SendAnswer(int sock, const sockaddr *from, size_t addrlen, uint16_t queryId, uint16_t rtype, uint16_t rclass, const char *name, size_t nameLength, mdns_record_t answer, const mdns_record_t *additional, size_t additionalCount) {
  std::array<char, 2048> buffer{};
  if (rclass & MDNS_UNICAST_RESPONSE)
    mdns_query_answer_unicast(sock, from, addrlen, buffer.data(), buffer.size(), queryId, static_cast<mdns_record_type_t>(rtype), name, nameLength, answer, nullptr, 0, additional, additionalCount);
  else
    mdns_query_answer_multicast(sock, buffer.data(), buffer.size(), answer, nullptr, 0, additional, additionalCount);
}
#endif
}

#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
// Handles incoming mDNS questions for the advertised MVR-xchange service.
static int MdnsCallback(int sock, const sockaddr *from, size_t addrlen, mdns_entry_type_t entry, uint16_t queryId, uint16_t rtype, uint16_t rclass, uint32_t, const void *data, size_t size, size_t nameOffset, size_t, size_t, size_t, void *userData) {
  if (entry != MDNS_ENTRYTYPE_QUESTION) return 0;
  auto *service = static_cast<MvrXchangeMdnsService *>(userData);
  char nameBuffer[512]{};
  size_t offset = nameOffset;
  const mdns_string_t queryName = mdns_string_extract(data, size, &offset, nameBuffer, sizeof(nameBuffer));
  const std::string query(queryName.str, queryName.length);
  const auto records = BuildRecords(service, Ipv4AddressFromString(service->AdvertisedIpAddress()));
  const bool any = rtype == MDNS_RECORDTYPE_ANY;
  if (query == kDiscoveryService && (rtype == MDNS_RECORDTYPE_PTR || any)) {
    SendAnswer(sock, from, addrlen, queryId, rtype, rclass, queryName.str, queryName.length, records.discoveryPtr, nullptr, 0);
  } else if ((query == service->ServiceType() || query == service->GroupServiceName()) && (rtype == MDNS_RECORDTYPE_PTR || any)) {
    const mdns_record_t answer = query == service->GroupServiceName() ? records.groupPtr : records.ptr;
    SendAnswer(sock, from, addrlen, queryId, rtype, rclass, queryName.str, queryName.length, answer, records.additional.data(), records.additional.size());
  } else if (query == service->ServiceInstanceName() && (rtype == MDNS_RECORDTYPE_TXT)) {
    SendAnswer(sock, from, addrlen, queryId, rtype, rclass, queryName.str, queryName.length, records.txtName, &records.txtUuid, 1);
  } else if (query == service->ServiceInstanceName() && (rtype == MDNS_RECORDTYPE_SRV || any)) {
    SendAnswer(sock, from, addrlen, queryId, rtype, rclass, queryName.str, queryName.length, records.srv, records.additional.data() + 1, 3);
  } else if (query == service->QualifiedHostName() && (rtype == MDNS_RECORDTYPE_A || any)) {
    SendAnswer(sock, from, addrlen, queryId, rtype, rclass, queryName.str, queryName.length, records.a, nullptr, 0);
  }
  return 0;
}
#endif

// Starts mDNS advertisement for the official MVR-xchange TCP service.
bool MvrXchangeMdnsService::Start(const MvrXchangeSettings &settings, int port) {
  Stop();
  lastError_.clear();
  serviceName_ = settings.stationName.empty() ? "Perastage" : settings.stationName;
  stationUuid_ = settings.stationUuid;
  port_ = port;
  hostName_ = LocalHostName();
  qualifiedHostName_ = hostName_ + ".local.";
  const auto selectedInterface = SelectMvrXchangeNetworkInterface(settings.selectedInterfaceId);
  advertisedIpAddress_ = selectedInterface.ipv4Address;
  selectedInterfaceDescription_ = FormatMvrXchangeNetworkInterface(selectedInterface);
  groupServiceName_ = SanitizeDnsLabel(settings.groupName, "Default") + "." + kServiceType;
  serviceInstanceName_ = SanitizeDnsLabel(serviceName_, "Perastage") + "." + kServiceType;
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  if (!OpenSocket()) return false;
  stopRequested_ = false;
  running_ = true;
  Announce(false);
  worker_ = std::thread(&MvrXchangeMdnsService::Run, this);
  wxLogMessage("MVR-xchange mDNS advertised via mdns: service=%s group=%s station=%s uuid=%s port=%d host=%s selectedInterface=%s advertisedA=%s candidates=%s",
               wxString::FromUTF8(ServiceType()), wxString::FromUTF8(GroupServiceName()),
               wxString::FromUTF8(serviceName_), wxString::FromUTF8(stationUuid_), port,
               wxString::FromUTF8(qualifiedHostName_), wxString::FromUTF8(selectedInterfaceDescription_),
               wxString::FromUTF8(advertisedIpAddress_), wxString::FromUTF8(LocalAddressSummary()));
  return true;
#else
  lastError_ = "MVR-xchange mDNS advertisement failed because the vcpkg mdns backend is not available in this build. Install the vcpkg mdns port and rebuild Perastage.";
  wxLogError("%s", wxString::FromUTF8(lastError_));
  return false;
#endif
}

// Stops the active mDNS advertisement and worker thread.
void MvrXchangeMdnsService::Stop() {
  if (!running_ && socket_ < 0) return;
  stopRequested_ = true;
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  if (running_ && socket_ >= 0) Announce(true);
#endif
  running_ = false;
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  if (socket_ >= 0) {
    mdns_socket_close(socket_);
    socket_ = -1;
  }
#endif
  if (worker_.joinable()) worker_.join();
}

// Returns whether the advertisement backend is currently active.
bool MvrXchangeMdnsService::IsRunning() const { return running_; }

// Returns the latest advertisement backend error text.
std::string MvrXchangeMdnsService::LastError() const { return lastError_; }

// Returns the compiled mDNS backend name.
std::string MvrXchangeMdnsService::BackendName() const {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  return "mdns";
#else
  return "disabled";
#endif
}

// Returns the official MVR-xchange service type advertised by this module.
std::string MvrXchangeMdnsService::ServiceType() const { return kServiceType; }

// Returns the group subservice name used for discovery and diagnostics.
const std::string &MvrXchangeMdnsService::GroupServiceName() const { return groupServiceName_; }


// Returns the fully qualified DNS-SD service instance name.
const std::string &MvrXchangeMdnsService::ServiceInstanceName() const { return serviceInstanceName_; }

// Returns the qualified local host name used by the SRV record.
const std::string &MvrXchangeMdnsService::QualifiedHostName() const { return qualifiedHostName_; }

// Returns the advertised station name.
const std::string &MvrXchangeMdnsService::StationName() const { return serviceName_; }

// Returns the advertised station UUID.
const std::string &MvrXchangeMdnsService::StationUuid() const { return stationUuid_; }

// Returns the IPv4 address advertised in the mDNS A record.
std::string MvrXchangeMdnsService::AdvertisedIpAddress() const { return advertisedIpAddress_; }

// Returns the selected interface description used for diagnostics.
std::string MvrXchangeMdnsService::SelectedInterfaceDescription() const { return selectedInterfaceDescription_; }

// Returns the advertised TCP port.
int MvrXchangeMdnsService::Port() const { return port_; }

// Runs the mDNS query response loop on the backend-owned worker thread.
void MvrXchangeMdnsService::Run() {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  std::array<char, 2048> buffer{};
  while (!stopRequested_) {
    const int sock = socket_;
    if (sock < 0) break;
    fd_set readfs;
    FD_ZERO(&readfs);
    FD_SET(sock, &readfs);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    if (select(sock + 1, &readfs, nullptr, nullptr, &timeout) > 0 && FD_ISSET(sock, &readfs))
      mdns_socket_listen(sock, buffer.data(), buffer.size(), MdnsCallback, this);
  }
#endif
}

// Opens the IPv4 mDNS socket on all available interfaces.
bool MvrXchangeMdnsService::OpenSocket() {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(MDNS_PORT);
  socket_ = mdns_socket_open_ipv4(&address);
  if (socket_ < 0) {
    lastError_ = "The vcpkg mdns backend could not open UDP port 5353 for _mvrxchange._tcp.local.";
    return false;
  }
  return true;
#else
  return false;
#endif
}

// Sends a multicast announcement or goodbye for the advertised service.
void MvrXchangeMdnsService::Announce(bool goodbye) {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_MDNS
  if (socket_ < 0) return;
  const auto records = BuildRecords(this, Ipv4AddressFromString(advertisedIpAddress_));
  std::array<char, 2048> buffer{};
  if (goodbye) {
    mdns_goodbye_multicast(socket_, buffer.data(), buffer.size(), records.ptr, nullptr, 0, records.additional.data(), records.additional.size());
    mdns_goodbye_multicast(socket_, buffer.data(), buffer.size(), records.groupPtr, nullptr, 0, records.additional.data(), records.additional.size());
  } else {
    mdns_announce_multicast(socket_, buffer.data(), buffer.size(), records.ptr, nullptr, 0, records.additional.data(), records.additional.size());
    mdns_announce_multicast(socket_, buffer.data(), buffer.size(), records.groupPtr, nullptr, 0, records.additional.data(), records.additional.size());
  }
#endif
}
