#pragma once
#include "mvr_xchange_settings.h"
#include <string>

class MvrXchangeMdnsService {
public:
  bool Start(const MvrXchangeSettings &settings, int port);
  void Stop();
  bool IsRunning() const;
  std::string LastError() const;

private:
  bool running_ = false;
  std::string lastError_;
};
