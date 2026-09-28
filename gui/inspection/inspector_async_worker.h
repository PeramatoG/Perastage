#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace gui::inspection {

// Exposes cancellation and supersession state to one immutable Inspector task.
class InspectorStopToken final {
public:
  bool stop_requested() const;

private:
  friend class InspectorAsyncWorker;
  InspectorStopToken(const std::atomic<bool> *shutdown,
                     const std::atomic<std::uint64_t> *latest,
                     std::uint64_t generation);

  const std::atomic<bool> *shutdown_;
  const std::atomic<std::uint64_t> *latest_;
  std::uint64_t generation_;
};

// Provides an opaque, immutable payload boundary between Core work and GUI code.
struct InspectorAsyncPayload {
  virtual ~InspectorAsyncPayload() = default;
};

// Runs only the latest submitted Inspector task on one managed background thread.
class InspectorAsyncWorker final {
public:
  using Payload = std::shared_ptr<const InspectorAsyncPayload>;
  using Task = std::function<Payload(InspectorStopToken)>;
  using Completion = std::function<void(std::uint64_t, Payload)>;

  explicit InspectorAsyncWorker(Completion completion);
  ~InspectorAsyncWorker();

  InspectorAsyncWorker(const InspectorAsyncWorker &) = delete;
  InspectorAsyncWorker &operator=(const InspectorAsyncWorker &) = delete;

  std::uint64_t Submit(Task task);
  void Cancel();
  std::uint64_t LatestGeneration() const;

private:
  void Run();

  Completion completion_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  Task pending_;
  std::uint64_t pendingGeneration_ = 0;
  std::atomic<std::uint64_t> latestGeneration_{0};
  std::atomic<bool> shutdown_{false};
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  std::thread thread_;
#else
  std::jthread thread_;
#endif
};

} // namespace gui::inspection
