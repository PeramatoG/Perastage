#pragma once
#include <string>

namespace mvr::xchange {

constexpr const char *kMvrXchangeServiceType = "_mvrxchange._tcp.local.";
constexpr const char *kMvrXchangeDiscoveryService = "_services._dns-sd._udp.local.";

std::string SanitizeDnsLabel(const std::string &value, const std::string &fallback);
std::string NormalizeDnsName(const std::string &value);
bool DnsNamesEqual(const std::string &left, const std::string &right);
std::string BuildMvrXchangeGroupServiceName(const std::string &groupName);
std::string BuildMvrXchangeServiceInstanceName(const std::string &groupName);
std::string BuildMvrXchangeHostName(const std::string &machineName, const std::string &stationUuid);

}
