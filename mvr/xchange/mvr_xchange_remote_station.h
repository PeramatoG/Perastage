#pragma once
#include "mvr_xchange_commit.h"
#include <string>
#include <vector>

struct MvrXchangeRemoteStation {
  std::string stationUuid;
  std::string stationName;
  std::string provider;
  int verMajor = 0;
  int verMinor = 0;
  std::string serviceInstanceName;
  std::string hostName;
  std::string ipAddress;
  int port = 0;
  bool discovered = false;
  bool incomingJoined = false;
  bool outgoingJoined = false;
  std::vector<MvrXchangeCommit> commits;
};
