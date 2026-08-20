#pragma once
#include "mvr_xchange_remote_station.h"
#include "mvr_xchange_station_registry.h"
#include <vector>

namespace mvr::xchange {

std::vector<MvrXchangeRemoteStation> CapturePublicationDestinations(const MvrXchangeStationRegistry &registry);

}
