#pragma once
#include "mvr_xchange_settings.h"
#include <string>

class MvrXchangeMdnsService {
public:
  bool Start(const MvrXchangeSettings &settings, int port);
  void Stop();
  bool IsRunning() const;
  std::string LastError() const;
  std::string BackendName() const;
  std::string ServiceType() const;
  std::string GroupServiceName() const;

private:
  bool running_ = false;
  std::string lastError_;
  std::string groupServiceName_;
#ifdef PERASTAGE_MVR_XCHANGE_ENABLE_DNSSD
  void *serviceRef_ = nullptr;
#endif
};
