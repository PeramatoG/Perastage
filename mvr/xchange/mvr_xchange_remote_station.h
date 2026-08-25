#pragma once
#include "mvr_xchange_commit.h"
#include <string>
#include <vector>
#include <cstdint>

struct MvrXchangeRemoteStation {
  std::string stationUuid;
  std::string stationName;
  std::string provider;
  int verMajor = 0;
  int verMinor = 0;
  std::string serviceInstanceName;
  std::string normalizedDnsIdentity;
  std::string hostName;
  std::string mdnsResponderAddress;
  std::string ipAddress;
  int port = 0;
  bool discovered = false;
  bool incomingJoined = false;
  bool outgoingJoined = false;
  bool left = false;
  std::uint32_t ttlSeconds = 0;
  std::uint64_t lastSeenMonotonicMs = 0;
  bool inventorySpecified = false;
  std::vector<MvrXchangeCommit> commits;
};
