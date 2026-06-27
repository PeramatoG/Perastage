#include "mvr_xchange_dns_names.h"
#include <cctype>

namespace mvr::xchange {

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
