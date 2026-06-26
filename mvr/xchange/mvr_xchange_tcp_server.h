#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_settings.h"
#include "mvr_xchange_remote_station.h"
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
  using JoinCallback = std::function<void(const MvrXchangeRemoteStation &)>;

  MvrXchangeTcpServer();
  ~MvrXchangeTcpServer();
  bool Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback, JoinCallback joinCallback = {});
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
  void MarkClientJoined(int fd);

  MvrXchangeSettings settings_;
  CommitResolver resolver_;
  CommitListProvider commitListProvider_;
  LogCallback logCallback_;
  JoinCallback joinCallback_;
  std::atomic<bool> running_{false};
  int listenFd_ = -1;
  int port_ = 0;
  std::thread thread_;
  mutable std::mutex clientsMutex_;
  std::vector<int> clientFds_;
  std::vector<int> joinedClientFds_;
  std::mutex clientThreadsMutex_;
  std::vector<std::thread> clientThreads_;
};
