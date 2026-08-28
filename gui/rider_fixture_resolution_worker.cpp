#include "rider_fixture_resolution_worker.h"

#include <utility>

#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
// Creates a legacy cancellation view over worker-owned atomic state.
RiderFixtureResolutionStopToken::RiderFixtureResolutionStopToken(
    const std::atomic<bool> *stopRequested)
    : stopRequested_(stopRequested) {}
#else
// Creates a cancellation view over the managed C++20 stop token.
RiderFixtureResolutionStopToken::RiderFixtureResolutionStopToken(
    std::stop_token stopToken)
    : stopToken_(stopToken) {}
#endif

// Reports whether cooperative cancellation has been requested.
bool RiderFixtureResolutionStopToken::stop_requested() const {
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  return stopRequested_->load();
#else
  return stopToken_.stop_requested();
#endif
}

// Stops and joins active work before releasing the owned thread.
RiderFixtureResolutionWorker::~RiderFixtureResolutionWorker() {
  RequestStop();
  Join();
}

// Replaces any previous task with newly owned asynchronous work.
void RiderFixtureResolutionWorker::Start(Task task) {
  RequestStop();
  Join();
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  stopRequested_.store(false);
  thread_ = std::thread([this, task = std::move(task)]() mutable {
    task(RiderFixtureResolutionStopToken(&stopRequested_));
  });
#else
  thread_ =
      std::jthread([task = std::move(task)](std::stop_token stopToken) mutable {
        task(RiderFixtureResolutionStopToken(stopToken));
      });
#endif
}

// Requests cooperative cancellation from the active task.
void RiderFixtureResolutionWorker::RequestStop() {
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  stopRequested_.store(true);
#else
  if (thread_.joinable())
    thread_.request_stop();
#endif
}

// Waits for active work to finish without detaching it.
void RiderFixtureResolutionWorker::Join() {
  if (thread_.joinable())
    thread_.join();
}

// Reports whether the worker currently owns a joinable thread.
bool RiderFixtureResolutionWorker::Joinable() const {
  return thread_.joinable();
}
