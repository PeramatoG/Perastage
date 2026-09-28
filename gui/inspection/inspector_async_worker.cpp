#include "inspection/inspector_async_worker.h"

#include <exception>
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
  const auto generation = latestSourceGeneration_.fetch_add(1) + 1;
  latestPreviewGeneration_.fetch_add(1);
  {
    std::lock_guard lock(mutex_);
    pendingSource_ = std::move(task);
    pendingSourceGeneration_ = generation;
    pendingPreview_ = {};
    pendingPreviewGeneration_ = 0;
  }
  condition_.notify_one();
  return generation;
}

// Replaces preview work without changing current source authority.
std::uint64_t InspectorAsyncWorker::SubmitPreview(Task task) {
  const auto generation = latestPreviewGeneration_.fetch_add(1) + 1;
  {
    std::lock_guard lock(mutex_);
    pendingPreview_ = std::move(task);
    pendingPreviewGeneration_ = generation;
  }
  condition_.notify_one();
  return generation;
}

// Invalidates active and pending work without blocking the caller.
void InspectorAsyncWorker::Cancel(InspectorTaskDomain domain) {
  {
    std::lock_guard lock(mutex_);
    if (domain == InspectorTaskDomain::Source) {
      latestSourceGeneration_.fetch_add(1);
      latestPreviewGeneration_.fetch_add(1);
      pendingSource_ = {};
      pendingSourceGeneration_ = 0;
      pendingPreview_ = {};
      pendingPreviewGeneration_ = 0;
    } else {
      latestPreviewGeneration_.fetch_add(1);
      pendingPreview_ = {};
      pendingPreviewGeneration_ = 0;
    }
  }
  condition_.notify_one();
}

// Returns the generation that alone is allowed to publish a result.
std::uint64_t InspectorAsyncWorker::LatestGeneration(
    InspectorTaskDomain domain) const {
  return domain == InspectorTaskDomain::Source
             ? latestSourceGeneration_.load()
             : latestPreviewGeneration_.load();
}

// Executes queued work and independently rejects every stale completion.
void InspectorAsyncWorker::Run() {
  while (!shutdown_.load()) {
    Task task;
    std::uint64_t generation = 0;
    InspectorTaskDomain domain = InspectorTaskDomain::Source;
    {
      std::unique_lock lock(mutex_);
      condition_.wait(lock, [this] {
        return shutdown_.load() || pendingSource_ || pendingPreview_;
      });
      if (shutdown_.load())
        return;
      if (pendingSource_) {
        task = std::move(pendingSource_);
        generation = pendingSourceGeneration_;
        pendingSourceGeneration_ = 0;
      } else {
        domain = InspectorTaskDomain::Preview;
        task = std::move(pendingPreview_);
        generation = pendingPreviewGeneration_;
        pendingPreviewGeneration_ = 0;
      }
    }
    const auto *latest = domain == InspectorTaskDomain::Source
                             ? &latestSourceGeneration_
                             : &latestPreviewGeneration_;
    Result result;
    try {
      result.payload =
          task(InspectorStopToken(&shutdown_, latest, generation));
    } catch (const std::exception &error) {
      result.error = error.what();
    } catch (...) {
      result.error = "Unknown Inspector worker failure.";
    }
    if (!shutdown_.load() && latest->load() == generation &&
        (result.payload || !result.error.empty()))
      completion_(domain, generation, std::move(result));
  }
}

} // namespace gui::inspection
