#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace perastage::local_ipc {

inline constexpr std::uint16_t kDefaultPort = 49155;
inline constexpr std::size_t kMaximumMessageBytes = 64 * 1024;

using RequestHandler = std::function<std::string(const std::string &)>;

class Server {
public:
  Server() = default;
  ~Server();
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  // Starts a request server bound exclusively to the IPv4 loopback address.
  bool Start(std::uint16_t port, RequestHandler handler,
             std::string &error);
  // Stops accepting connections and waits for the worker to finish.
  void Stop();
  // Returns the bound port, including an ephemeral port assigned for zero.
  std::uint16_t Port() const { return port_; }

private:
  // Accepts and serves bounded newline-framed requests until stopped.
  void Run();

  std::intptr_t listenSocket_ = -1;
  std::atomic<bool> stopping_{false};
  std::uint16_t port_ = 0;
  RequestHandler handler_;
  std::thread worker_;
};

// Sends one bounded request to a loopback server and receives one response.
bool Exchange(std::uint16_t port, const std::string &request,
              std::string &response, std::string &error);

} // namespace perastage::local_ipc
