#pragma once
#include "mvr_xchange_mdns_cache.h"
#include "mvr_xchange_remote_station.h"
#include "mvr_xchange_settings.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class MvrXchangeMdnsDiscovery {
public:
  using LogCallback = std::function<void(const std::string &)>;
  MvrXchangeMdnsDiscovery();
  ~MvrXchangeMdnsDiscovery();
  bool Start(const MvrXchangeSettings &settings, const std::string &localInstanceName,
             const std::string &localStationUuid, LogCallback logCallback);
  void Stop();
  void QueryNow();
  std::vector<MvrXchangeRemoteStation> Snapshot();
  bool IsRunning() const;

private:
  bool OpenSocket();
  void Run();
  void SendQueries();
  void ReceiveDatagram();
  void ApplyDatagram(const std::uint8_t *data, std::size_t size, std::uint32_t interfaceIndex);

  MvrXchangeSettings settings_;
  std::string groupServiceName_;
  std::string localInstanceName_;
  std::string localStationUuid_;
  LogCallback logCallback_;
  std::atomic<bool> running_{false};
  std::atomic<bool> queryRequested_{false};
  std::intptr_t socket_ = -1;
  std::thread worker_;
  bool networkInitialized_ = false;
  mutable std::mutex mutex_;
  mvr::xchange::MdnsRecordCache cache_;
};
