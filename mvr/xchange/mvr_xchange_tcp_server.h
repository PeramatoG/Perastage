#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_settings.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class MvrXchangeTcpServer {
public:
  using CommitResolver = std::function<std::optional<MvrXchangeCommit>(const std::string &)>;
  using LogCallback = std::function<void(const std::string &)>;

  MvrXchangeTcpServer();
  ~MvrXchangeTcpServer();
  bool Start(const MvrXchangeSettings &settings, CommitResolver resolver, LogCallback logCallback);
  void Stop();
  bool IsRunning() const;
  int Port() const;
  void BroadcastCommit(const MvrXchangeCommit &commit);

private:
  void Run();
  void HandleClient(int clientFd);
  bool SendJson(int fd, const std::string &json);

  MvrXchangeSettings settings_;
  CommitResolver resolver_;
  LogCallback logCallback_;
  std::atomic<bool> running_{false};
  int listenFd_ = -1;
  int port_ = 0;
  std::thread thread_;
  mutable std::mutex mutex_;
};
