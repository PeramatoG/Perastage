#pragma once

#include <atomic>
#include <functional>
#include <thread>

// Exposes cooperative cancellation without requiring stop_token on legacy
// libc++.
class RiderFixtureResolutionStopToken {
public:
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  explicit RiderFixtureResolutionStopToken(
      const std::atomic<bool> *stopRequested);
#else
  explicit RiderFixtureResolutionStopToken(std::stop_token stopToken);
#endif

  bool stop_requested() const;

private:
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  const std::atomic<bool> *stopRequested_;
#else
  std::stop_token stopToken_;
#endif
};

// Owns one replaceable fixture-resolution task and always joins it on shutdown.
class RiderFixtureResolutionWorker {
public:
  using Task = std::function<void(RiderFixtureResolutionStopToken)>;

  RiderFixtureResolutionWorker() = default;
  ~RiderFixtureResolutionWorker();

  RiderFixtureResolutionWorker(const RiderFixtureResolutionWorker &) = delete;
  RiderFixtureResolutionWorker &
  operator=(const RiderFixtureResolutionWorker &) = delete;

  void Start(Task task);
  void RequestStop();
  void Join();
  bool Joinable() const;

private:
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  std::atomic<bool> stopRequested_{false};
  std::thread thread_;
#else
  std::jthread thread_;
#endif
};
