#include "mvr_xchange_publication_policy.h"

namespace mvr::xchange {

// Captures only memberships established before a new revision enters JOIN inventory.
std::vector<MvrXchangeRemoteStation> CapturePublicationDestinations(const MvrXchangeStationRegistry &registry) {
  return registry.JoinedStations();
}

}
