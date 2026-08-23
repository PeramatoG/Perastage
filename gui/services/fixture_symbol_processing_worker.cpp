#include "services/fixture_symbol_processing_worker.h"

#include <utility>

#include "tools/scene_model_symbol_capture_service.h"

namespace gui {

// Starts the single managed worker used for pure symbol processing.
FixtureSymbolProcessingWorker::FixtureSymbolProcessingWorker()
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
    : thread_([this]() { Run(); }) {}
#else
    : thread_([this](std::stop_token stopToken) { Run(stopToken); }) {}
#endif

// Stops and joins the managed processing worker during application shutdown.
FixtureSymbolProcessingWorker::~FixtureSymbolProcessingWorker() {
#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  condition_.notify_all();
  thread_.join();
#else
  thread_.request_stop();
  condition_.notify_all();
#endif
}

// Submits one immutable processing request when the worker is available.
bool FixtureSymbolProcessingWorker::Submit(
    FixtureSymbolProcessingRequest request) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (busy_ || !requests_.empty() || !results_.empty())
    return false;
  requests_.push_back(std::move(request));
  condition_.notify_one();
  return true;
}

// Returns one completed processing result without blocking the GUI thread.
std::optional<FixtureSymbolProcessingResult>
FixtureSymbolProcessingWorker::TakeResult() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (results_.empty())
    return std::nullopt;
  FixtureSymbolProcessingResult result = std::move(results_.front());
  results_.pop_front();
  return result;
}

#if defined(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)
// Processes copied render data with the macOS 15 hosted-toolchain fallback.
void FixtureSymbolProcessingWorker::Run() {
  while (true) {
    FixtureSymbolProcessingRequest request;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock,
                      [this]() { return stopping_ || !requests_.empty(); });
      if (stopping_)
        return;
      request = std::move(requests_.front());
      requests_.pop_front();
      busy_ = true;
    }
    auto processed = tools::ProcessSceneModelOrthographicRenders(
        std::move(request.renders), request.bounds);
    FixtureSymbolProcessingResult result;
    result.epoch = request.epoch;
    result.ok = processed.ok;
    result.error = std::move(processed.error);
    result.symbols = std::move(processed.symbols);
    result.bounds = processed.fixtureBoundsMm;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      results_.push_back(std::move(result));
      busy_ = false;
    }
  }
}
#else
// Processes copied render data without accessing wx, OpenGL, or live scene state.
void FixtureSymbolProcessingWorker::Run(std::stop_token stopToken) {
  while (!stopToken.stop_requested()) {
    FixtureSymbolProcessingRequest request;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, stopToken,
                      [this]() { return !requests_.empty(); });
      if (stopToken.stop_requested())
        return;
      request = std::move(requests_.front());
      requests_.pop_front();
      busy_ = true;
    }
    auto processed = tools::ProcessSceneModelOrthographicRenders(
        std::move(request.renders), request.bounds);
    FixtureSymbolProcessingResult result;
    result.epoch = request.epoch;
    result.ok = processed.ok;
    result.error = std::move(processed.error);
    result.symbols = std::move(processed.symbols);
    result.bounds = processed.fixtureBoundsMm;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      results_.push_back(std::move(result));
      busy_ = false;
    }
  }
}
#endif

} // namespace gui
