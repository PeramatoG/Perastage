#pragma once

#include "local_ipc/local_ipc_transport.h"

#include <memory>
#include <string>

class MainWindow;

class LocalLiveController {
public:
  explicit LocalLiveController(MainWindow &window);
  ~LocalLiveController();

  // Starts the local live endpoint on the fixed application port.
  bool Start(std::string &error);
  // Stops the endpoint before application services are torn down.
  void Stop();

private:
  struct DispatchState;

  // Marshals a transport request to the GUI thread for active-project access.
  std::string DispatchOnMainThread(const std::string &wireRequest);
  // Executes one validated request against the active project.
  std::string HandleOnMainThread(const std::string &wireRequest);

  MainWindow &window_;
  std::shared_ptr<DispatchState> dispatchState_;
  perastage::local_ipc::Server server_;
};
