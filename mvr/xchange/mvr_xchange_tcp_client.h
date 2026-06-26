#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_remote_station.h"
#include "mvr_xchange_settings.h"
#include <functional>
#include <string>
#include <vector>

class MvrXchangeTcpClient {
public:
  using LogCallback = std::function<void(const std::string &)>;

  bool SendJoin(const MvrXchangeRemoteStation &station, const MvrXchangeSettings &settings, const std::vector<MvrXchangeCommit> &localCommits, MvrXchangeRemoteStation &joinedStation, LogCallback logCallback);
  bool SendCommit(const MvrXchangeRemoteStation &station, const MvrXchangeCommit &commit, LogCallback logCallback);

private:
  bool Connect(const MvrXchangeRemoteStation &station, int &fd, LogCallback logCallback);
  bool SendJson(int fd, const std::string &json);
  bool ReceiveJson(int fd, std::string &json);
};
