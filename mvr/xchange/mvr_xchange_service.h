#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_mdns_service.h"
#include "mvr_xchange_settings.h"
#include "mvr_xchange_tcp_server.h"
#include <functional>
#include <mutex>
#include <optional>
#include <string>

class MvrXchangeService {
public:
  using LogCallback = std::function<void(const std::string &)>;
  bool Start(const MvrXchangeSettings &settings);
  void Stop();
  bool IsRunning() const;
  int Port() const;
  bool PublishCurrentScene(const std::string &comment);
  std::vector<MvrXchangeCommit> GetLocalCommits() const;
  void SetLogCallback(LogCallback callback);

private:
  std::optional<MvrXchangeCommit> ResolveRequest(const std::string &fileUuid) const;
  void Log(const std::string &message) const;

  MvrXchangeSettings settings_;
  MvrXchangeCommitStore commits_{5};
  MvrXchangeTcpServer tcpServer_;
  MvrXchangeMdnsService mdnsService_;
  LogCallback logCallback_;
  mutable std::mutex mutex_;
};
