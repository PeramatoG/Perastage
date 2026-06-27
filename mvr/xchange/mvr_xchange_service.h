#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_mdns_discovery.h"
#include "mvr_xchange_mdns_service.h"
#include "mvr_xchange_settings.h"
#include "mvr_xchange_station_registry.h"
#include "mvr_xchange_tcp_client.h"
#include "mvr_xchange_tcp_server.h"
#include <atomic>
#include <functional>
#include <thread>
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
  std::vector<MvrXchangeRemoteStation> GetKnownStations() const;
  void DiscoverNow();
  void SetLogCallback(LogCallback callback);

private:
  std::optional<MvrXchangeCommit> ResolveRequest(const std::string &fileUuid) const;
  void HandleIncomingJoin(const MvrXchangeRemoteStation &station);
  void DiscoverStationsOnce();
  void DiscoveryLoop();
  void TryOutgoingJoin(const MvrXchangeRemoteStation &station);
  void LogStationCounts() const;
  void SendCommitToJoinedStations(const MvrXchangeCommit &commit);
  void Log(const std::string &message) const;

  MvrXchangeSettings settings_;
  MvrXchangeCommitStore commits_{5};
  MvrXchangeTcpServer tcpServer_;
  MvrXchangeMdnsService mdnsService_;
  MvrXchangeMdnsDiscovery mdnsDiscovery_;
  MvrXchangeStationRegistry stationRegistry_;
  MvrXchangeTcpClient tcpClient_;
  std::atomic<bool> discoveryStopRequested_{false};
  std::thread discoveryThread_;
  LogCallback logCallback_;
  mutable std::mutex mutex_;
  std::mutex discoveryMutex_;
};
