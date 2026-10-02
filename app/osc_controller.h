#pragma once

#include "osc/osc_udp_transport.h"

#include <atomic>
#include <memory>
#include <string>

class MainWindow;

class OscController {
public:
  explicit OscController(MainWindow &window);
  ~OscController();

  // Starts the OSC endpoint on its dedicated application port.
  bool Start(std::string &error);
  // Stops the endpoint before application services are torn down.
  void Stop();

private:
  struct DispatchState;

  // Parses a datagram and schedules supported commands on the GUI thread.
  void ReceivePacket(std::span<const std::uint8_t> packet);
  // Executes a parsed OSC message against the active project.
  void HandleOnMainThread(perastage::osc::Message message);

  MainWindow &window_;
  std::shared_ptr<DispatchState> dispatchState_;
  perastage::osc::UdpServer server_;
};
