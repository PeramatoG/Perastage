#include "rider_fixture_resolution_worker.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

using namespace std::chrono_literals;

// Waits until a worker task confirms that it has started.
static void WaitForStart(std::mutex &mutex, std::condition_variable &condition,
                         bool &started) {
  std::unique_lock<std::mutex> lock(mutex);
  assert(condition.wait_for(lock, 2s, [&started]() { return started; }));
}

// Verifies cancellation suppresses publication and joins the owned task.
static void TestStopAndJoin() {
  RiderFixtureResolutionWorker worker;
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  std::atomic<bool> published{false};
  worker.Start([&](RiderFixtureResolutionStopToken stopToken) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      started = true;
    }
    condition.notify_one();
    while (!stopToken.stop_requested())
      std::this_thread::yield();
    if (!stopToken.stop_requested())
      published.store(true);
  });
  WaitForStart(mutex, condition, started);
  worker.RequestStop();
  worker.Join();
  assert(!worker.Joinable());
  assert(!published.load());
}

// Verifies starting replacement work safely stops and joins previous work.
static void TestReplacement() {
  RiderFixtureResolutionWorker worker;
  std::atomic<bool> firstStopped{false};
  std::atomic<bool> replacementRan{false};
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  worker.Start([&](RiderFixtureResolutionStopToken stopToken) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      started = true;
    }
    condition.notify_one();
    while (!stopToken.stop_requested())
      std::this_thread::yield();
    firstStopped.store(true);
  });
  WaitForStart(mutex, condition, started);
  worker.Start(
      [&](RiderFixtureResolutionStopToken) { replacementRan.store(true); });
  worker.Join();
  assert(firstStopped.load());
  assert(replacementRan.load());
}

// Verifies destruction requests cancellation and waits for active work.
static void TestActiveDestruction() {
  std::atomic<bool> stopped{false};
  {
    RiderFixtureResolutionWorker worker;
    worker.Start([&](RiderFixtureResolutionStopToken stopToken) {
      while (!stopToken.stop_requested())
        std::this_thread::yield();
      stopped.store(true);
    });
  }
  assert(stopped.load());
}

// Runs the owned-worker contract against the selected threading backend.
int main() {
  TestStopAndJoin();
  TestReplacement();
  TestActiveDestruction();
  return 0;
}
