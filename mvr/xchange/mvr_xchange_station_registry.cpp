#include "mvr_xchange_station_registry.h"
#include "../../core/uuidutils.h"
#include <algorithm>
#include "mvr_xchange_dns_names.h"

// Stores the local station identity so registry updates can ignore self records.
void MvrXchangeStationRegistry::SetLocalIdentity(const std::string &stationUuid, const std::string &serviceInstanceName,
                                                 const std::string &localIpAddress, int localPort) {
  localStationUuid_ = CanonicalizeUuid(stationUuid);
  localServiceInstanceName_ = serviceInstanceName;
  localIpAddress_ = localIpAddress;
  localPort_ = localPort;
}

// Inserts or updates a station discovered through mDNS.
bool MvrXchangeStationRegistry::UpsertDiscovered(MvrXchangeRemoteStation station) {
  station.discovered = true;
  station.normalizedDnsIdentity = mvr::xchange::NormalizeDnsName(station.serviceInstanceName);
  station.stationUuid = CanonicalizeUuid(station.stationUuid);
  if (IsOwnStation(station)) return false;
  auto it = FindStation(station);
  if (it == stations_.end()) { stations_.push_back(std::move(station)); return true; }
  it->discovered = true;
  if (!station.stationUuid.empty()) it->stationUuid = station.stationUuid;
  if (!station.stationName.empty()) it->stationName = station.stationName;
  if (!station.provider.empty()) it->provider = station.provider;
  if (!station.serviceInstanceName.empty()) { it->serviceInstanceName = station.serviceInstanceName; it->normalizedDnsIdentity = station.normalizedDnsIdentity; }
  if (!station.hostName.empty()) it->hostName = station.hostName;
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  if (station.ttlSeconds > 0) it->ttlSeconds = station.ttlSeconds;
  if (station.lastSeenMonotonicMs > 0) it->lastSeenMonotonicMs = station.lastSeenMonotonicMs;
  if (!it->stationUuid.empty()) MergeProvisionalDuplicates(it->stationUuid);
  return true;
}

// Reconciles discovery presence and clears handshakes for TTL-expired stations.
void MvrXchangeStationRegistry::ReconcileDiscovered(const std::vector<MvrXchangeRemoteStation> &stations) {
  std::vector<std::string> previouslyDiscovered;
  for (auto &known : stations_) {
    if (known.discovered) previouslyDiscovered.push_back(!known.stationUuid.empty() ? known.stationUuid : known.normalizedDnsIdentity);
    known.discovered = false;
  }
  for (const auto &station : stations) UpsertDiscovered(station);
  for (auto &known : stations_) {
    const std::string identity = !known.stationUuid.empty() ? known.stationUuid : known.normalizedDnsIdentity;
    if (!known.discovered && std::find(previouslyDiscovered.begin(), previouslyDiscovered.end(), identity) != previouslyDiscovered.end()) {
      known.incomingJoined = false;
      known.outgoingJoined = false;
    }
  }
}

// Inserts or updates a station that sent an incoming MVR_JOIN.
bool MvrXchangeStationRegistry::UpsertIncomingJoin(MvrXchangeRemoteStation station) {
  station.incomingJoined = true;
  station.left = false;
  station.stationUuid = CanonicalizeUuid(station.stationUuid);
  if (IsOwnStation(station)) return false;
  auto it = FindStation(station);
  if (it == stations_.end()) { stations_.push_back(std::move(station)); return true; }
  it->incomingJoined = true;
  if (!station.stationUuid.empty()) it->stationUuid = station.stationUuid;
  if (!station.stationName.empty()) it->stationName = station.stationName;
  if (!station.provider.empty()) it->provider = station.provider;
  it->verMajor = station.verMajor;
  it->verMinor = station.verMinor;
  it->left = false;
  if (station.inventorySpecified) it->commits = station.commits;
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  if (!it->stationUuid.empty()) MergeProvisionalDuplicates(it->stationUuid);
  return true;
}

// Merges provisional DNS records after a canonical StationUUID becomes known.
void MvrXchangeStationRegistry::MergeProvisionalDuplicates(std::string stationUuid) {
  auto canonical = std::find_if(stations_.begin(), stations_.end(), [&](const auto &station) { return station.stationUuid == stationUuid; });
  if (canonical == stations_.end()) return;
  for (auto it = stations_.begin(); it != stations_.end();) {
    const bool sameDnsIdentity = !it->serviceInstanceName.empty() && !canonical->serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(it->serviceInstanceName, canonical->serviceInstanceName);
    if (it == canonical || !it->stationUuid.empty() ||
        (!sameDnsIdentity &&
         (it->ipAddress.empty() || canonical->ipAddress.empty() || it->ipAddress != canonical->ipAddress || it->port != canonical->port))) { ++it; continue; }
    canonical->discovered = canonical->discovered || it->discovered;
    if (canonical->serviceInstanceName.empty()) { canonical->serviceInstanceName = it->serviceInstanceName; canonical->normalizedDnsIdentity = it->normalizedDnsIdentity; }
    if (canonical->hostName.empty()) canonical->hostName = it->hostName;
    if (canonical->ipAddress.empty()) canonical->ipAddress = it->ipAddress;
    if (canonical->port <= 0) canonical->port = it->port;
    it = stations_.erase(it);
    canonical = std::find_if(stations_.begin(), stations_.end(), [&](const auto &station) { return station.stationUuid == stationUuid; });
    if (canonical == stations_.end()) return;
  }
}

// Applies identity and inventory returned by a successful outgoing JOIN.
bool MvrXchangeStationRegistry::UpsertOutgoingJoin(MvrXchangeRemoteStation station) {
  station.stationUuid = CanonicalizeUuid(station.stationUuid);
  if (IsOwnStation(station)) return false;
  auto it = FindStation(station);
  if (it == stations_.end()) { station.outgoingJoined = true; station.left = false; stations_.push_back(std::move(station)); return true; }
  if (it->left) return false;
  it->outgoingJoined = true;
  it->left = false;
  if (!station.stationUuid.empty()) it->stationUuid = station.stationUuid;
  if (!station.stationName.empty()) it->stationName = station.stationName;
  if (!station.provider.empty()) it->provider = station.provider;
  it->verMajor = station.verMajor;
  it->verMinor = station.verMinor;
  if (station.inventorySpecified) it->commits = station.commits;
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  return true;
}

// Marks a known station as successfully joined by Perastage.
bool MvrXchangeStationRegistry::MarkOutgoingJoined(const std::string &stationUuid, const std::string &ipAddress, int port) {
  MvrXchangeRemoteStation key;
  key.stationUuid = CanonicalizeUuid(stationUuid);
  key.ipAddress = ipAddress;
  key.port = port;
  auto it = FindStation(key);
  if (it == stations_.end()) return false;
  if (it->left) return false;
  it->outgoingJoined = true;
  it->left = false;
  return true;
}

// Returns whether discovery may initiate a JOIN without overriding explicit LEAVE.
bool MvrXchangeStationRegistry::ShouldInitiateOutgoingJoin(const MvrXchangeRemoteStation &station) {
  auto key = station;
  key.stationUuid = CanonicalizeUuid(key.stationUuid);
  if (IsOwnStation(key)) return false;
  auto it = FindStation(key);
  return it == stations_.end() || (!it->left && !it->incomingJoined && !it->outgoingJoined);
}

// Marks a station as explicitly departed until a later JOIN succeeds.
bool MvrXchangeStationRegistry::MarkLeft(const std::string &stationUuid) {
  const std::string canonical = CanonicalizeUuid(stationUuid);
  auto it = std::find_if(stations_.begin(), stations_.end(), [&](const auto &station) { return station.stationUuid == canonical; });
  if (it == stations_.end()) return false;
  it->left = true;
  it->incomingJoined = false;
  it->outgoingJoined = false;
  return true;
}

// Adds or replaces one incoming commit in the associated station inventory.
bool MvrXchangeStationRegistry::ApplyCommit(const MvrXchangeCommit &commit) {
  auto station = std::find_if(stations_.begin(), stations_.end(), [&](const auto &known) {
    return known.stationUuid == CanonicalizeUuid(commit.stationUuid) && !known.left && (known.incomingJoined || known.outgoingJoined);
  });
  if (station == stations_.end()) return false;
  auto existing = std::find_if(station->commits.begin(), station->commits.end(), [&](const auto &known) {
    return known.fileUuid == CanonicalizeUuid(commit.fileUuid) && known.stationUuid == CanonicalizeUuid(commit.stationUuid);
  });
  if (existing == station->commits.end()) station->commits.push_back(commit);
  else *existing = commit;
  station->inventorySpecified = true;
  return true;
}

// Returns a copy of all known remote stations.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::List() const { return stations_; }

// Returns stations with a successful incoming or outgoing join.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::JoinedStations() const {
  std::vector<MvrXchangeRemoteStation> joined;
  for (const auto &station : stations_) if (!station.left && (station.incomingJoined || station.outgoingJoined)) joined.push_back(station);
  return joined;
}

// Returns whether current membership permits a commit transaction.
bool MvrXchangeStationRegistry::CanSendCommitTo(const std::string &stationUuid) const {
  const std::string canonical = CanonicalizeUuid(stationUuid);
  return std::any_of(stations_.begin(), stations_.end(), [&](const auto &station) {
    return station.stationUuid == canonical && !station.left && (station.incomingJoined || station.outgoingJoined);
  });
}

// Returns true if a station appears to be this Perastage instance.
bool MvrXchangeStationRegistry::IsOwnStation(const MvrXchangeRemoteStation &station) const {
  if (!station.stationUuid.empty() && station.stationUuid == localStationUuid_) return true;
  if (!station.serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(station.serviceInstanceName, localServiceInstanceName_)) return true;
  if (!localIpAddress_.empty() && localPort_ > 0 && station.ipAddress == localIpAddress_ && station.port == localPort_) return true;
  return false;
}

// Finds a station by UUID when known or by endpoint before UUID is known.
std::vector<MvrXchangeRemoteStation>::iterator MvrXchangeStationRegistry::FindStation(const MvrXchangeRemoteStation &station) {
  if (!station.stationUuid.empty()) {
    auto byUuid = std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) { return existing.stationUuid == station.stationUuid; });
    if (byUuid != stations_.end()) return byUuid;
  }
  return std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) {
    const bool identitiesCompatible = station.stationUuid.empty() || existing.stationUuid.empty() || station.stationUuid == existing.stationUuid;
    return identitiesCompatible && ((!station.serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(existing.serviceInstanceName, station.serviceInstanceName)) ||
           (!station.ipAddress.empty() && station.port > 0 && existing.ipAddress == station.ipAddress && existing.port == station.port));
  });
}
