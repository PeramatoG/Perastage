#include "inspection/inspector_async_worker.h"

#include <utility>

namespace gui::inspection {

// Creates a cancellation view tied to worker-owned atomic state.
InspectorStopToken::InspectorStopToken(
    const std::atomic<bool> *shutdown,
    const std::atomic<std::uint64_t> *latest, std::uint64_t generation)
    : shutdown_(shutdown), latest_(latest), generation_(generation) {}

// Reports shutdown or replacement without relying on Core cancellation support.
bool InspectorStopToken::stop_requested() const {
  return shutdown_->load() || latest_->load() != generation_;
}

// Starts the single long-lived Inspector worker.
InspectorAsyncWorker::InspectorAsyncWorker(Completion completion)
    : completion_(std::move(completion)), thread_([this] { Run(); }) {}

// Stops and joins the managed thread before its callback state is released.
InspectorAsyncWorker::~InspectorAsyncWorker() {
  shutdown_.store(true);
  condition_.notify_one();
  if (thread_.joinable())
    thread_.join();
}

// Replaces pending work without waiting for currently executing Core work.
std::uint64_t InspectorAsyncWorker::Submit(Task task) {
  const auto generation = latestGeneration_.fetch_add(1) + 1;
  {
    std::lock_guard lock(mutex_);
    pending_ = std::move(task);
    pendingGeneration_ = generation;
  }
  condition_.notify_one();
  return generation;
}

// Invalidates active and pending work without blocking the caller.
void InspectorAsyncWorker::Cancel() {
  latestGeneration_.fetch_add(1);
  {
    std::lock_guard lock(mutex_);
    pending_ = {};
    pendingGeneration_ = 0;
  }
  condition_.notify_one();
}

// Returns the generation that alone is allowed to publish a result.
std::uint64_t InspectorAsyncWorker::LatestGeneration() const {
  return latestGeneration_.load();
}

// Executes queued work and independently rejects every stale completion.
void InspectorAsyncWorker::Run() {
  while (!shutdown_.load()) {
    Task task;
    std::uint64_t generation = 0;
    {
      std::unique_lock lock(mutex_);
      condition_.wait(lock, [this] { return shutdown_.load() || pending_; });
      if (shutdown_.load())
        return;
      task = std::move(pending_);
      generation = pendingGeneration_;
      pendingGeneration_ = 0;
    }
    Payload payload;
    try {
      payload = task(InspectorStopToken(&shutdown_, &latestGeneration_,
                                        generation));
    } catch (...) {
      // A malformed input must not terminate the managed worker or GUI process.
      continue;
    }
    if (!shutdown_.load() && latestGeneration_.load() == generation && payload)
      completion_(generation, std::move(payload));
  }
}

} // namespace gui::inspection
