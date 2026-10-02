#pragma once

#include "osc/osc_message.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <thread>

namespace perastage::osc {

inline constexpr std::uint16_t kDefaultPort = 49156;
using PacketHandler = std::function<void(std::span<const std::uint8_t>)>;

class UdpServer {
public:
  UdpServer() = default;
  ~UdpServer();
  UdpServer(const UdpServer &) = delete;
  UdpServer &operator=(const UdpServer &) = delete;

  // Starts a UDP listener bound exclusively to IPv4 loopback.
  bool Start(std::uint16_t port, PacketHandler handler, std::string &error);
  // Stops the listener and waits for its worker.
  void Stop();
  // Returns the active bound port, including an ephemeral test port.
  std::uint16_t Port() const { return port_; }

private:
  // Receives bounded datagrams until shutdown.
  void Run();

  std::intptr_t socket_ = -1;
  std::atomic<bool> stopping_{false};
  std::uint16_t port_ = 0;
  PacketHandler handler_;
  std::thread worker_;
};

} // namespace perastage::osc
