#pragma once
#include "mvr_xchange_remote_station.h"
#include <string>
#include <vector>
#include <cstdint>

class MvrXchangeStationRegistry {
public:
  void SetLocalIdentity(const std::string &stationUuid, const std::string &serviceInstanceName, int localPort);
  bool UpsertDiscovered(MvrXchangeRemoteStation station);
  void ReconcileDiscovered(const std::vector<MvrXchangeRemoteStation> &stations);
  bool UpsertIncomingJoin(MvrXchangeRemoteStation station);
  bool UpsertOutgoingJoin(MvrXchangeRemoteStation station);
  bool MarkOutgoingJoined(const std::string &stationUuid, const std::string &ipAddress, int port);
  bool ShouldInitiateOutgoingJoin(const MvrXchangeRemoteStation &station);
  bool MarkLeft(const std::string &stationUuid);
  bool ApplyCommit(const MvrXchangeCommit &commit);
  std::vector<MvrXchangeRemoteStation> List() const;
  std::vector<MvrXchangeRemoteStation> JoinedStations() const;
  bool CanSendCommitTo(const std::string &stationUuid) const;
  bool IsOwnStation(const MvrXchangeRemoteStation &station) const;

private:
  std::vector<MvrXchangeRemoteStation>::iterator FindStation(const MvrXchangeRemoteStation &station);
  void MergeProvisionalDuplicates(std::string stationUuid);
  std::string localStationUuid_;
  std::string localServiceInstanceName_;
  std::vector<MvrXchangeRemoteStation> stations_;
};
