#include "mvr_xchange_station_registry.h"
#include "../../core/uuidutils.h"
#include <algorithm>
#include "mvr_xchange_dns_names.h"

// Stores the local station identity so registry updates can ignore self records.
void MvrXchangeStationRegistry::SetLocalIdentity(const std::string &stationUuid, const std::string &serviceInstanceName, int localPort) {
  localStationUuid_ = CanonicalizeUuid(stationUuid);
  localServiceInstanceName_ = serviceInstanceName;
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
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  if (station.ttlSeconds > 0) it->ttlSeconds = station.ttlSeconds;
  if (station.lastSeenMonotonicMs > 0) it->lastSeenMonotonicMs = station.lastSeenMonotonicMs;
  return true;
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
  if (station.inventorySpecified || !station.commits.empty()) it->commits = station.commits;
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
  it->outgoingJoined = true;
  it->left = false;
  return true;
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

// Expires discovery and handshake state after the advertised TTL elapses.
void MvrXchangeStationRegistry::ExpireDiscovered(std::uint64_t nowMonotonicMs) {
  for (auto &station : stations_) {
    if (station.ttlSeconds == 0 || station.lastSeenMonotonicMs == 0) continue;
    if (nowMonotonicMs - station.lastSeenMonotonicMs > static_cast<std::uint64_t>(station.ttlSeconds) * 1000) {
      station.discovered = false;
      station.incomingJoined = false;
      station.outgoingJoined = false;
    }
  }
}

// Returns a copy of all known remote stations.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::List() const { return stations_; }

// Returns stations with a successful incoming or outgoing join.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::JoinedStations() const {
  std::vector<MvrXchangeRemoteStation> joined;
  for (const auto &station : stations_) if (!station.left && (station.incomingJoined || station.outgoingJoined)) joined.push_back(station);
  return joined;
}

// Returns true if a station appears to be this Perastage instance.
bool MvrXchangeStationRegistry::IsOwnStation(const MvrXchangeRemoteStation &station) const {
  if (!station.stationUuid.empty() && station.stationUuid == localStationUuid_) return true;
  if (!station.serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(station.serviceInstanceName, localServiceInstanceName_)) return true;
  return false;
}

// Finds a station by UUID when known or by endpoint before UUID is known.
std::vector<MvrXchangeRemoteStation>::iterator MvrXchangeStationRegistry::FindStation(const MvrXchangeRemoteStation &station) {
  if (!station.stationUuid.empty()) {
    auto byUuid = std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) { return existing.stationUuid == station.stationUuid; });
    if (byUuid != stations_.end()) return byUuid;
  }
  return std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) {
    return (!station.serviceInstanceName.empty() && mvr::xchange::DnsNamesEqual(existing.serviceInstanceName, station.serviceInstanceName)) ||
           (!station.ipAddress.empty() && station.port > 0 && existing.ipAddress == station.ipAddress && existing.port == station.port);
  });
}
