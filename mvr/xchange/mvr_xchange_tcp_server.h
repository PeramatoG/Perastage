#pragma once
#include "mvr_xchange_commit.h"
#include "mvr_xchange_settings.h"
#include "mvr_xchange_remote_station.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>

class MvrXchangeTcpServer {
public:
  using CommitResolver = std::function<std::optional<MvrXchangeCommit>(const std::string &)>;
  using CommitListProvider = std::function<std::vector<MvrXchangeCommit>()>;
  using LogCallback = std::function<void(const std::string &)>;
  using JoinCallback = std::function<void(const MvrXchangeRemoteStation &)>;
  using LeaveCallback = std::function<std::string(const std::string &)>;
  using CommitCallback = std::function<std::string(const MvrXchangeCommit &)>;

  MvrXchangeTcpServer();
  ~MvrXchangeTcpServer();
  bool Start(const MvrXchangeSettings &settings, CommitResolver resolver, CommitListProvider commitListProvider, LogCallback logCallback, JoinCallback joinCallback = {}, LeaveCallback leaveCallback = {}, CommitCallback commitCallback = {});
  void Stop();
  bool IsRunning() const;
  int Port() const;

private:
  void Run();
  void HandleClient(std::intptr_t clientFd);
  bool SendJson(std::intptr_t fd, const std::string &json);
  bool SendPacket(std::intptr_t fd, const std::vector<uint8_t> &packet);
  void RemoveClient(std::intptr_t fd);
  void ReapCompletedTransactions();

  struct TransactionThread {
    std::thread thread;
    std::shared_ptr<std::atomic<bool>> complete;
  };

  MvrXchangeSettings settings_;
  CommitResolver resolver_;
  CommitListProvider commitListProvider_;
  LogCallback logCallback_;
  JoinCallback joinCallback_;
  LeaveCallback leaveCallback_;
  CommitCallback commitCallback_;
  std::atomic<bool> running_{false};
  std::intptr_t listenFd_ = -1;
  int port_ = 0;
  bool networkInitialized_ = false;
  std::thread thread_;
  mutable std::mutex clientsMutex_;
  std::vector<std::intptr_t> clientFds_;
  std::mutex clientThreadsMutex_;
  std::vector<TransactionThread> clientThreads_;
};
