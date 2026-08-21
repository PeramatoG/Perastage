#include "mvr_xchange_publication_policy.h"
#include <algorithm>

namespace mvr::xchange {

// Freezes memberships before a new revision becomes visible in JOIN inventory.
PublicationSession::PublicationSession(const MvrXchangeStationRegistry &registry)
    : commitDestinations_(registry.JoinedStations()) {}

// Returns the stations eligible for the publication's COMMIT path.
const std::vector<MvrXchangeRemoteStation> &PublicationSession::CommitDestinations() const { return commitDestinations_; }

// Delegates discovery JOIN decisions while respecting current membership and LEAVE.
bool PublicationSession::ShouldInitiateJoin(MvrXchangeStationRegistry &registry, const MvrXchangeRemoteStation &station) const {
  return registry.ShouldInitiateOutgoingJoin(station);
}

// Allows COMMIT only for a frozen destination whose membership remains active.
bool PublicationSession::ShouldSendCommit(const MvrXchangeStationRegistry &registry, const std::string &stationUuid) const {
  return std::any_of(commitDestinations_.begin(), commitDestinations_.end(), [&](const auto &station) { return station.stationUuid == stationUuid; }) &&
         registry.CanSendCommitTo(stationUuid);
}

// Captures only memberships established before a new revision enters JOIN inventory.
std::vector<MvrXchangeRemoteStation> CapturePublicationDestinations(const MvrXchangeStationRegistry &registry) {
  return registry.JoinedStations();
}

}
