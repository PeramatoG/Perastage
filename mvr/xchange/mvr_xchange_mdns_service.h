#pragma once
#include "mvr_xchange_settings.h"
#include <atomic>
#include <string>
#include <thread>

class MvrXchangeMdnsService {
public:
  bool Start(const MvrXchangeSettings &settings, int port);
  void Stop();
  bool IsRunning() const;
  std::string LastError() const;
  std::string BackendName() const;
  std::string ServiceType() const;
  const std::string &GroupServiceName() const;
  const std::string &ServiceInstanceName() const;
  const std::string &QualifiedHostName() const;
  const std::string &StationName() const;
  const std::string &StationUuid() const;
  int Port() const;

private:
  void Run();
  bool OpenSocket();
  void Announce(bool goodbye);

  bool running_ = false;
  std::atomic<bool> stopRequested_{false};
  std::string lastError_;
  std::string groupServiceName_;
  std::string serviceName_;
  std::string serviceInstanceName_;
  std::string hostName_;
  std::string qualifiedHostName_;
  std::string stationUuid_;
  int port_ = 0;
  int socket_ = -1;
  std::thread worker_;
};
