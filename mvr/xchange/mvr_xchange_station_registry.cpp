#include "mvr_xchange_station_registry.h"
#include <algorithm>

// Stores the local station identity so registry updates can ignore self records.
void MvrXchangeStationRegistry::SetLocalIdentity(const std::string &stationUuid, const std::string &serviceInstanceName, int localPort) {
  localStationUuid_ = stationUuid;
  localServiceInstanceName_ = serviceInstanceName;
  localPort_ = localPort;
}

// Inserts or updates a station discovered through mDNS.
bool MvrXchangeStationRegistry::UpsertDiscovered(MvrXchangeRemoteStation station) {
  station.discovered = true;
  if (IsOwnStation(station)) return false;
  auto it = FindStation(station);
  if (it == stations_.end()) { stations_.push_back(std::move(station)); return true; }
  it->discovered = true;
  if (!station.stationUuid.empty()) it->stationUuid = station.stationUuid;
  if (!station.stationName.empty()) it->stationName = station.stationName;
  if (!station.provider.empty()) it->provider = station.provider;
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  return true;
}

// Inserts or updates a station that sent an incoming MVR_JOIN.
bool MvrXchangeStationRegistry::UpsertIncomingJoin(MvrXchangeRemoteStation station) {
  station.incomingJoined = true;
  if (IsOwnStation(station)) return false;
  auto it = FindStation(station);
  if (it == stations_.end()) { stations_.push_back(std::move(station)); return true; }
  it->incomingJoined = true;
  if (!station.stationUuid.empty()) it->stationUuid = station.stationUuid;
  if (!station.stationName.empty()) it->stationName = station.stationName;
  if (!station.provider.empty()) it->provider = station.provider;
  it->verMajor = station.verMajor;
  it->verMinor = station.verMinor;
  it->commits = station.commits;
  if (!station.ipAddress.empty()) it->ipAddress = station.ipAddress;
  if (station.port > 0) it->port = station.port;
  return true;
}

// Marks a known station as successfully joined by Perastage.
bool MvrXchangeStationRegistry::MarkOutgoingJoined(const std::string &stationUuid, const std::string &ipAddress, int port) {
  MvrXchangeRemoteStation key;
  key.stationUuid = stationUuid;
  key.ipAddress = ipAddress;
  key.port = port;
  auto it = FindStation(key);
  if (it == stations_.end()) return false;
  it->outgoingJoined = true;
  return true;
}

// Returns a copy of all known remote stations.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::List() const { return stations_; }

// Returns stations with a successful incoming or outgoing join.
std::vector<MvrXchangeRemoteStation> MvrXchangeStationRegistry::JoinedStations() const {
  std::vector<MvrXchangeRemoteStation> joined;
  for (const auto &station : stations_) if (station.incomingJoined || station.outgoingJoined) joined.push_back(station);
  return joined;
}

// Returns true if a station appears to be this Perastage instance.
bool MvrXchangeStationRegistry::IsOwnStation(const MvrXchangeRemoteStation &station) const {
  if (!station.stationUuid.empty() && station.stationUuid == localStationUuid_) return true;
  if (!station.serviceInstanceName.empty() && station.serviceInstanceName == localServiceInstanceName_) return true;
  return station.port > 0 && station.port == localPort_ && (station.ipAddress == "127.0.0.1" || station.ipAddress == "localhost");
}

// Finds a station by UUID when known or by endpoint before UUID is known.
std::vector<MvrXchangeRemoteStation>::iterator MvrXchangeStationRegistry::FindStation(const MvrXchangeRemoteStation &station) {
  if (!station.stationUuid.empty()) {
    auto byUuid = std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) { return existing.stationUuid == station.stationUuid; });
    if (byUuid != stations_.end()) return byUuid;
  }
  return std::find_if(stations_.begin(), stations_.end(), [&](const auto &existing) {
    return (!station.serviceInstanceName.empty() && existing.serviceInstanceName == station.serviceInstanceName) ||
           (!station.ipAddress.empty() && station.port > 0 && existing.ipAddress == station.ipAddress && existing.port == station.port);
  });
}
