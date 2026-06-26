#include "mvr_xchange_mdns_service.h"
#include <sstream>
#include <vector>
#include <wx/log.h>
#include <wx/string.h>
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_DNSSD
#include <dns_sd.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
constexpr const char *kServiceType = "_mvrxchange._tcp";
constexpr const char *kServiceDomain = "local.";

// Returns local address diagnostics for mDNS troubleshooting logs.
std::string LocalAddressSummary() {
  char hostname[256]{};
  if (gethostname(hostname, sizeof(hostname)) != 0) return "unknown interfaces";
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *result = nullptr;
  if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) return std::string(hostname);
  std::vector<std::string> addresses;
  for (addrinfo *it = result; it; it = it->ai_next) {
    char host[NI_MAXHOST]{};
    if (getnameinfo(it->ai_addr, static_cast<socklen_t>(it->ai_addrlen), host, sizeof(host), nullptr, 0, NI_NUMERICHOST) == 0)
      addresses.emplace_back(host);
  }
  freeaddrinfo(result);
  std::ostringstream out;
  out << hostname;
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

// Appends one DNS-SD TXT key/value entry to a TXT buffer.
bool AppendTxtRecord(std::vector<unsigned char> &txt, const std::string &key, const std::string &value) {
  const std::string entry = key + "=" + value;
  if (entry.size() > 255) return false;
  txt.push_back(static_cast<unsigned char>(entry.size()));
  txt.insert(txt.end(), entry.begin(), entry.end());
  return true;
}
}

// Starts DNS-SD advertisement for the official MVR-xchange TCP service.
bool MvrXchangeMdnsService::Start(const MvrXchangeSettings &settings, int port) {
  Stop();
  lastError_.clear();
  groupServiceName_ = settings.groupName + "." + kServiceType + "." + kServiceDomain;
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_DNSSD
  const std::string regType = std::string(kServiceType) + "," + settings.groupName;
  std::vector<unsigned char> txt;
  if (!AppendTxtRecord(txt, "StationName", settings.stationName) || !AppendTxtRecord(txt, "StationUUID", settings.stationUuid)) {
    lastError_ = "Station TXT record is too long for DNS-SD.";
    return false;
  }
  DNSServiceRef ref = nullptr;
  const DNSServiceErrorType err = DNSServiceRegister(&ref, 0, 0, settings.stationName.c_str(), regType.c_str(), kServiceDomain, nullptr, static_cast<uint16_t>(port), static_cast<uint16_t>(txt.size()), txt.data(), nullptr, nullptr);
  if (err != kDNSServiceErr_NoError) {
    lastError_ = "DNSServiceRegister failed with error " + std::to_string(err) + ". Ensure Bonjour/DNS-SD is installed and running.";
    return false;
  }
  serviceRef_ = ref;
  running_ = true;
  wxLogMessage("MVR-xchange mDNS advertised via %s: service=%s.%s subtype=%s group=%s station=%s uuid=%s port=%d interfaces=%s",
               wxString::FromUTF8(BackendName()), wxString::FromUTF8(kServiceType), wxString::FromUTF8(kServiceDomain),
               wxString::FromUTF8(regType), wxString::FromUTF8(groupServiceName_), wxString::FromUTF8(settings.stationName),
               wxString::FromUTF8(settings.stationUuid), port, wxString::FromUTF8(LocalAddressSummary()));
  return true;
#else
  lastError_ = "No DNS-SD backend is available in this build. Install Bonjour/DNS-SD development libraries and rebuild to advertise _mvrxchange._tcp.local.";
  wxLogError("MVR-xchange mDNS advertisement unavailable: %s", wxString::FromUTF8(lastError_));
  return false;
#endif
}

// Stops the active DNS-SD advertisement when one is running.
void MvrXchangeMdnsService::Stop() {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_DNSSD
  if (serviceRef_) {
    DNSServiceRefDeallocate(static_cast<DNSServiceRef>(serviceRef_));
    serviceRef_ = nullptr;
  }
#endif
  running_ = false;
}

// Returns whether the advertisement backend is currently active.
bool MvrXchangeMdnsService::IsRunning() const { return running_; }

// Returns the latest advertisement backend error text.
std::string MvrXchangeMdnsService::LastError() const { return lastError_; }

// Returns the compiled mDNS/DNS-SD backend name.
std::string MvrXchangeMdnsService::BackendName() const {
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_DNSSD
  return "Bonjour/DNS-SD";
#else
  return "disabled";
#endif
}

// Returns the official MVR-xchange service type advertised by this module.
std::string MvrXchangeMdnsService::ServiceType() const { return std::string(kServiceType) + "." + kServiceDomain; }

// Returns the group subservice name used for diagnostics.
std::string MvrXchangeMdnsService::GroupServiceName() const { return groupServiceName_; }
