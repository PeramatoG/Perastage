#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_settings.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class MvrXchangeTcpServer {
public:
  using CommitResolver = std::function<std::optional<MvrXchangeCommit>(const std::string &)>;
  using CommitListProvider = std::function<std::vector<MvrXchangeCommit>()>;
  using LogCallback = std::function<void(const std::string &)>;

  MvrXchangeTcpServer();
  ~MvrXchangeTcpServer();
  bool Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback);
  void Stop();
  bool IsRunning() const;
  int Port() const;
  void BroadcastCommit(const MvrXchangeCommit &commit);

private:
  void Run();
  void HandleClient(int clientFd);
  bool SendJson(int fd, const std::string &json);
  bool SendPacket(int fd, const std::vector<uint8_t> &packet);
  void AddClient(int fd);
  void RemoveClient(int fd);

  MvrXchangeSettings settings_;
  CommitResolver resolver_;
  CommitListProvider commitListProvider_;
  LogCallback logCallback_;
  std::atomic<bool> running_{false};
  int listenFd_ = -1;
  int port_ = 0;
  std::thread thread_;
  mutable std::mutex clientsMutex_;
  std::vector<int> clientFds_;
  std::mutex clientThreadsMutex_;
  std::vector<std::thread> clientThreads_;
};
