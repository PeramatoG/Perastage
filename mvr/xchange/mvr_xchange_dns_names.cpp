#include "mvr_xchange_dns_names.h"
#include <cctype>
#include <algorithm>

namespace mvr::xchange {

// Normalizes a DNS name for case-insensitive identity comparison.
std::string NormalizeDnsName(const std::string &value) {
  std::string out = value;
  while (!out.empty() && out.back() == '.') out.pop_back();
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return out;
}

// Compares DNS names using RFC case-insensitive and trailing-dot semantics.
bool DnsNamesEqual(const std::string &left, const std::string &right) {
  return NormalizeDnsName(left) == NormalizeDnsName(right);
}

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

// Builds the group subservice name used by MVR-xchange DNS-SD.
std::string BuildMvrXchangeGroupServiceName(const std::string &groupName) {
  return SanitizeDnsLabel(groupName, "Default") + "." + kMvrXchangeServiceType;
}

// Builds the group-qualified DNS-SD instance name used by compatible MVR-xchange stations.
std::string BuildMvrXchangeServiceInstanceName(const std::string &stationName, const std::string &groupName) {
  return SanitizeDnsLabel(stationName, "Perastage") + "." + BuildMvrXchangeGroupServiceName(groupName);
}

}
