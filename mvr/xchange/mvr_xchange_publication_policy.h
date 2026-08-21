#pragma once
#include "mvr_xchange_remote_station.h"
#include "mvr_xchange_station_registry.h"
#include <string>
#include <vector>

namespace mvr::xchange {

class PublicationSession {
public:
  explicit PublicationSession(const MvrXchangeStationRegistry &registry);
  const std::vector<MvrXchangeRemoteStation> &CommitDestinations() const;
  bool ShouldInitiateJoin(MvrXchangeStationRegistry &registry, const MvrXchangeRemoteStation &station) const;
  bool ShouldSendCommit(const MvrXchangeStationRegistry &registry, const std::string &stationUuid) const;

private:
  std::vector<MvrXchangeRemoteStation> commitDestinations_;
};

std::vector<MvrXchangeRemoteStation> CapturePublicationDestinations(const MvrXchangeStationRegistry &registry);

}
