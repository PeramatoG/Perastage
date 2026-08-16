#pragma once
#include "mvr_xchange_remote_station.h"
#include <string>
#include <vector>
#include <cstdint>

class MvrXchangeStationRegistry {
public:
  void SetLocalIdentity(const std::string &stationUuid, const std::string &serviceInstanceName, int localPort);
  bool UpsertDiscovered(MvrXchangeRemoteStation station);
  bool UpsertIncomingJoin(MvrXchangeRemoteStation station);
  bool MarkOutgoingJoined(const std::string &stationUuid, const std::string &ipAddress, int port);
  bool MarkLeft(const std::string &stationUuid);
  void ExpireDiscovered(std::uint64_t nowMonotonicMs);
  std::vector<MvrXchangeRemoteStation> List() const;
  std::vector<MvrXchangeRemoteStation> JoinedStations() const;
  bool IsOwnStation(const MvrXchangeRemoteStation &station) const;

private:
  std::vector<MvrXchangeRemoteStation>::iterator FindStation(const MvrXchangeRemoteStation &station);
  std::string localStationUuid_;
  std::string localServiceInstanceName_;
  int localPort_ = 0;
  std::vector<MvrXchangeRemoteStation> stations_;
};
