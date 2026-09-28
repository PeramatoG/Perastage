#include "inspection/inspector_async_worker.h"

#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace {

struct Value final : gui::inspection::InspectorAsyncPayload {
  explicit Value(int initial) : value(initial) {}
  int value;
};

} // namespace

// Verifies non-blocking replacement, cancellation, and latest-request publication.
int main() {
  std::mutex mutex;
  std::condition_variable condition;
  bool firstStarted = false;
  bool releaseFirst = false;
  std::vector<int> completed;

  {
    gui::inspection::InspectorAsyncWorker worker(
        [&](std::uint64_t, gui::inspection::InspectorAsyncWorker::Payload payload) {
          std::lock_guard lock(mutex);
          completed.push_back(static_cast<const Value &>(*payload).value);
          condition.notify_all();
        });
    worker.Submit([&](gui::inspection::InspectorStopToken token) {
      std::unique_lock lock(mutex);
      firstStarted = true;
      condition.notify_all();
      condition.wait(lock, [&] { return releaseFirst; });
      assert(token.stop_requested());
      return std::make_shared<Value>(1);
    });
    {
      std::unique_lock lock(mutex);
      condition.wait(lock, [&] { return firstStarted; });
    }
    const auto latest = worker.Submit([](gui::inspection::InspectorStopToken token) {
      assert(!token.stop_requested());
      return std::make_shared<Value>(2);
    });
    assert(latest == worker.LatestGeneration());
    {
      std::lock_guard lock(mutex);
      releaseFirst = true;
    }
    condition.notify_all();
    {
      std::unique_lock lock(mutex);
      condition.wait(lock, [&] { return completed.size() == 1; });
    }
    assert((completed == std::vector<int>{2}));

    worker.Submit([&](gui::inspection::InspectorStopToken) {
      std::unique_lock lock(mutex);
      condition.wait(lock, [&] { return releaseFirst; });
      return std::make_shared<Value>(3);
    });
    worker.Cancel();
  }

  assert((completed == std::vector<int>{2}));
  return 0;
}
