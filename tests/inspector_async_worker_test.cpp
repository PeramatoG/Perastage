#include "inspection/inspector_async_worker.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct Value final : gui::inspection::InspectorAsyncPayload {
  explicit Value(int initial) : value(initial) {}
  int value;
};

// Synchronizes by predicate, with a timeout to diagnose stalled worker progress.
void Wait(std::condition_variable &condition, std::unique_lock<std::mutex> &lock,
          const auto &predicate) {
  const bool signaled = condition.wait_for(lock, std::chrono::seconds(5), predicate);
  assert(signaled);
}

// Exercises shutdown while idle and immediately after a task completes.
void CheckIdleShutdown() {
  for (const bool afterCompletion : {false, true}) {
    for (int iteration = 0; iteration < 256; ++iteration) {
      std::mutex mutex;
      std::condition_variable condition;
      bool completed = false;
      gui::inspection::InspectorAsyncWorker worker(
          [&](gui::inspection::InspectorTaskDomain, std::uint64_t,
              gui::inspection::InspectorAsyncWorker::Result result) {
            std::lock_guard lock(mutex);
            assert(result.Success());
            completed = true;
            condition.notify_one();
          });
      if (afterCompletion) {
        worker.Submit([](gui::inspection::InspectorStopToken)
                          -> gui::inspection::InspectorAsyncWorker::Payload {
          return std::make_shared<Value>(1);
        });
        std::unique_lock lock(mutex);
        Wait(condition, lock, [&] { return completed; });
      }
    }
  }
}

// Verifies replacement rejects a stale completion and publishes the latest.
void CheckLatestWins() {
  std::mutex mutex;
  std::condition_variable condition;
  bool firstStarted = false;
  bool releaseFirst = false;
  std::vector<int> completed;
  gui::inspection::InspectorAsyncWorker worker(
      [&](gui::inspection::InspectorTaskDomain, std::uint64_t,
          gui::inspection::InspectorAsyncWorker::Result result) {
        std::lock_guard lock(mutex);
        assert(result.Success());
        completed.push_back(static_cast<const Value &>(*result.payload).value);
        condition.notify_all();
      });
  worker.Submit([&](gui::inspection::InspectorStopToken token) {
    std::unique_lock lock(mutex);
    firstStarted = true;
    condition.notify_all();
    Wait(condition, lock, [&] { return releaseFirst; });
    assert(token.stop_requested());
    return std::make_shared<Value>(1);
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return firstStarted; });
  }
  worker.Submit([](gui::inspection::InspectorStopToken)
                    -> gui::inspection::InspectorAsyncWorker::Payload {
    return std::make_shared<Value>(2);
  });
  {
    std::lock_guard lock(mutex);
    releaseFirst = true;
  }
  condition.notify_all();
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return completed.size() == 1; });
  }
  assert((completed == std::vector<int>{2}));
}

// Verifies shutdown requests cancellation, joins, and never runs pending work.
void CheckDestruction() {
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  int completions = 0;
  auto worker = std::make_unique<gui::inspection::InspectorAsyncWorker>(
      [&](gui::inspection::InspectorTaskDomain, std::uint64_t,
          gui::inspection::InspectorAsyncWorker::Result) {
        ++completions;
      });
  worker->Submit([&](gui::inspection::InspectorStopToken token) {
    {
      std::lock_guard lock(mutex);
      started = true;
    }
    condition.notify_all();
    while (!token.stop_requested())
      std::this_thread::yield();
    return std::make_shared<Value>(1);
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return started; });
  }
  worker.reset();
  assert(completions == 0);
}

// Verifies cancellation removes pending work before blocked active work exits.
void CheckPendingCancellation() {
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  bool release = false;
  bool pendingRan = false;
  int completions = 0;
  gui::inspection::InspectorAsyncWorker worker(
      [&](gui::inspection::InspectorTaskDomain, std::uint64_t,
          gui::inspection::InspectorAsyncWorker::Result) {
        ++completions;
      });
  worker.Submit([&](gui::inspection::InspectorStopToken) {
    std::unique_lock lock(mutex);
    started = true;
    condition.notify_all();
    Wait(condition, lock, [&] { return release; });
    return std::make_shared<Value>(1);
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return started; });
  }
  worker.Submit([&](gui::inspection::InspectorStopToken)
                    -> gui::inspection::InspectorAsyncWorker::Payload {
    pendingRan = true;
    return std::make_shared<Value>(2);
  });
  worker.Cancel();
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  condition.notify_all();
  worker.Cancel();
  assert(!pendingRan);
  assert(completions == 0);
}

// Verifies latest failures publish, stale failures do not, and recovery works.
void CheckFailures() {
  std::mutex mutex;
  std::condition_variable condition;
  bool staleStarted = false;
  bool releaseStale = false;
  std::vector<std::string> errors;
  std::vector<int> values;
  gui::inspection::InspectorAsyncWorker worker(
      [&](gui::inspection::InspectorTaskDomain, std::uint64_t,
          gui::inspection::InspectorAsyncWorker::Result result) {
        std::lock_guard lock(mutex);
        if (!result.error.empty())
          errors.push_back(result.error);
        else
          values.push_back(static_cast<const Value &>(*result.payload).value);
        condition.notify_all();
      });
  worker.Submit([&](gui::inspection::InspectorStopToken)
                    -> gui::inspection::InspectorAsyncWorker::Payload {
    std::unique_lock lock(mutex);
    staleStarted = true;
    condition.notify_all();
    Wait(condition, lock, [&] { return releaseStale; });
    throw std::runtime_error("stale failure");
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return staleStarted; });
  }
  worker.Submit([](gui::inspection::InspectorStopToken)
                    -> gui::inspection::InspectorAsyncWorker::Payload {
    throw std::runtime_error("latest failure");
  });
  {
    std::lock_guard lock(mutex);
    releaseStale = true;
  }
  condition.notify_all();
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return errors.size() == 1; });
  }
  assert(errors.front() == "latest failure");
  worker.Submit([](gui::inspection::InspectorStopToken)
                    -> gui::inspection::InspectorAsyncWorker::Payload {
    return std::make_shared<Value>(7);
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return values.size() == 1; });
  }
  assert(values.front() == 7);
}

// Verifies preview replacement cannot invalidate an authoritative source task.
void CheckPreviewDoesNotSupersedeSource() {
  std::mutex mutex;
  std::condition_variable condition;
  bool sourceStarted = false;
  bool releaseSource = false;
  std::vector<gui::inspection::InspectorTaskDomain> domains;
  gui::inspection::InspectorAsyncWorker worker(
      [&](gui::inspection::InspectorTaskDomain domain, std::uint64_t,
          gui::inspection::InspectorAsyncWorker::Result result) {
        std::lock_guard lock(mutex);
        assert(result.Success());
        domains.push_back(domain);
        condition.notify_all();
      });
  worker.Submit([&](gui::inspection::InspectorStopToken token) {
    std::unique_lock lock(mutex);
    sourceStarted = true;
    condition.notify_all();
    Wait(condition, lock, [&] { return releaseSource; });
    assert(!token.stop_requested());
    return std::make_shared<Value>(1);
  });
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return sourceStarted; });
  }
  worker.SubmitPreview([](gui::inspection::InspectorStopToken)
                           -> gui::inspection::InspectorAsyncWorker::Payload {
    return std::make_shared<Value>(2);
  });
  {
    std::lock_guard lock(mutex);
    releaseSource = true;
  }
  condition.notify_all();
  {
    std::unique_lock lock(mutex);
    Wait(condition, lock, [&] { return domains.size() == 2; });
  }
  assert(domains.front() == gui::inspection::InspectorTaskDomain::Source);
  assert(domains.back() == gui::inspection::InspectorTaskDomain::Preview);
}

} // namespace

// Exercises worker replacement, shutdown, failure publication, and recovery.
int main() {
  CheckIdleShutdown();
  CheckLatestWins();
  CheckDestruction();
  CheckPendingCancellation();
  CheckFailures();
  CheckPreviewDoesNotSupersedeSource();
  return 0;
}
