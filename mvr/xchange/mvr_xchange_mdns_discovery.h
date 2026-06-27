#pragma once
#include "mvr_xchange_remote_station.h"
#include "mvr_xchange_settings.h"
#include <functional>
#include <string>
#include <vector>

class MvrXchangeMdnsDiscovery {
public:
  using LogCallback = std::function<void(const std::string &)>;

  std::vector<MvrXchangeRemoteStation> DiscoverStations(const MvrXchangeSettings &settings,
                                                        const std::string &localInstanceName,
                                                        const std::string &localStationUuid,
                                                        const std::string &localIpAddress,
                                                        int localPort,
                                                        LogCallback logCallback);
};
