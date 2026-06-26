#include "mvr_xchange_network_interfaces.h"
#include <algorithm>
#include <sstream>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace {
// Returns true when the address is an IPv4 loopback address.
bool IsLoopbackAddress(const std::string &address) { return address.rfind("127.", 0) == 0; }

// Adds an explicit loopback option if the platform enumeration did not return one.

#ifdef _WIN32
// Converts a Windows wide string to UTF-8 for display.
std::string WideToUtf8(const wchar_t *value) {
  if (!value) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string out(static_cast<std::size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
  return out;
}
#endif

void EnsureLoopback(std::vector<MvrXchangeNetworkInterface> &interfaces) {
  const auto found = std::any_of(interfaces.begin(), interfaces.end(), [](const auto &iface) { return iface.ipv4Address == "127.0.0.1"; });
  if (!found) interfaces.push_back({"127.0.0.1", "Loopback", "127.0.0.1", true, true});
}
}

// Enumerates IPv4 network interfaces suitable for MVR-xchange discovery.
std::vector<MvrXchangeNetworkInterface> ListMvrXchangeNetworkInterfaces() {
  std::vector<MvrXchangeNetworkInterface> interfaces;
#ifdef _WIN32
  ULONG size = 0;
  GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size);
  std::vector<unsigned char> buffer(size);
  auto *addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
  if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, addresses, &size) == NO_ERROR) {
    for (auto *adapter = addresses; adapter; adapter = adapter->Next) {
      const bool isUp = adapter->OperStatus == IfOperStatusUp;
      for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
        if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
        char ip[INET_ADDRSTRLEN]{};
        auto *addr = reinterpret_cast<sockaddr_in *>(unicast->Address.lpSockaddr);
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        std::string name = WideToUtf8(adapter->FriendlyName);
        if (name.empty()) name = adapter->AdapterName ? adapter->AdapterName : ip;
        interfaces.push_back({ip, name, ip, IsLoopbackAddress(ip), isUp});
      }
    }
  }
#else
  ifaddrs *ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == 0) {
    for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
      char ip[INET_ADDRSTRLEN]{};
      auto *addr = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
      inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
      const bool isUp = (ifa->ifa_flags & IFF_UP) != 0;
      interfaces.push_back({ip, ifa->ifa_name ? ifa->ifa_name : ip, ip, (ifa->ifa_flags & IFF_LOOPBACK) != 0 || IsLoopbackAddress(ip), isUp});
    }
    freeifaddrs(ifaddr);
  }
#endif
  EnsureLoopback(interfaces);
  std::sort(interfaces.begin(), interfaces.end(), [](const auto &a, const auto &b) {
    if (a.isLoopback != b.isLoopback) return !a.isLoopback;
    return a.displayName < b.displayName;
  });
  return interfaces;
}

// Selects a configured interface or returns the first suitable automatic choice.
MvrXchangeNetworkInterface SelectMvrXchangeNetworkInterface(const std::string &selectedId) {
  const auto interfaces = ListMvrXchangeNetworkInterfaces();
  if (!selectedId.empty()) {
    for (const auto &iface : interfaces) {
      if (iface.id == selectedId || iface.ipv4Address == selectedId) return iface;
    }
  }
  for (const auto &iface : interfaces) {
    if (iface.isUp && !iface.isLoopback) return iface;
  }
  return interfaces.empty() ? MvrXchangeNetworkInterface{"127.0.0.1", "Loopback", "127.0.0.1", true, true} : interfaces.front();
}

// Formats an interface for logs and GUI choices.
std::string FormatMvrXchangeNetworkInterface(const MvrXchangeNetworkInterface &iface) {
  std::ostringstream out;
  out << iface.displayName << " " << iface.ipv4Address;
  if (iface.isLoopback) out << " (loopback)";
  if (!iface.isUp) out << " (down)";
  return out.str();
}
