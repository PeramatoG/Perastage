#include <cassert>
#include <chrono>
#include <thread>
#include <utility>

#include "services/fixture_symbol_processing_worker.h"
#include "tools/scene_model_symbol_capture_service.h"

namespace tools {

// Supplies deterministic processing output for worker lifecycle tests.
SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage> renders,
    const FixtureGeometryBounds &fixtureBoundsMm,
    symbols::FixtureSymbolTimings *) {
  SceneModelSymbolCaptureResult result;
  result.fixtureBoundsMm = fixtureBoundsMm;
  result.ok = !renders.empty();
  for (const auto &render : renders) {
    symbols::Symbol2D symbol;
    symbol.view = render.view;
    result.symbols.push_back(std::move(symbol));
  }
  return result;
}

} // namespace tools

namespace {

// Creates one deterministic rendered view without image-decoder dependencies.
symbols::RenderedSymbolImage MakeRender(symbols::SymbolView view, int variant) {
  symbols::RenderedSymbolImage render{
      view, 18, 16, std::vector<unsigned char>(18 * 16 * 4, 255)};
  for (int y = 2; y < 12; ++y) {
    for (int x = 2; x < 10 + variant; ++x) {
      if (x == 5 && y >= 5 && y <= 7)
        continue;
      const auto offset = static_cast<std::size_t>((y * render.width + x) * 4);
      render.rgba[offset] = 63;
      render.rgba[offset + 1] = 169;
      render.rgba[offset + 2] = 245;
    }
  }
  return render;
}

// Creates the complete production-ordered four-view processing input.
std::vector<symbols::RenderedSymbolImage> MakeRenders() {
  std::vector<symbols::RenderedSymbolImage> renders;
  const auto &plan = symbols::FixtureSymbolCapturePlan();
  for (std::size_t index = 0; index < plan.size(); ++index)
    renders.push_back(MakeRender(plan[index].symbolView,
                                 static_cast<int>(index)));
  return renders;
}

// Waits for one worker result with a bounded regression-test timeout.
std::optional<gui::FixtureSymbolProcessingResult>
WaitForResult(gui::FixtureSymbolProcessingWorker &worker) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto result = worker.TakeResult())
      return result;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return std::nullopt;
}

// Verifies request preservation, single-result delivery, and worker reuse.
void TestWorkerContract() {
  tools::FixtureGeometryBounds bounds;
  bounds.min = {-40.0f, -25.0f, -60.0f};
  bounds.max = {80.0f, 45.0f, 90.0f};
  bounds.valid = true;
  const auto renders = MakeRenders();
  gui::FixtureSymbolProcessingWorker worker;
  assert(worker.Submit({4182, renders, bounds}));
  assert(!worker.Submit({4183, renders, bounds}));
  auto result = WaitForResult(worker);
  assert(result && result->ok && result->epoch == 4182);
  assert(result->symbols.size() == 4);
  assert(result->bounds.min == bounds.min && result->bounds.max == bounds.max &&
         result->bounds.valid == bounds.valid);
  assert(!worker.TakeResult());

  assert(worker.Submit({4184, renders, bounds}));
  auto second = WaitForResult(worker);
  assert(second && second->epoch == 4184 && second->symbols.size() == 4);
}

// Verifies destruction wakes and joins an idle worker without deadlocking.
void TestIdleShutdown() { gui::FixtureSymbolProcessingWorker worker; }

// Verifies destruction safely resolves a worker that still owns pending work.
void TestPendingShutdown() {
  tools::FixtureGeometryBounds bounds;
  bounds.valid = true;
  gui::FixtureSymbolProcessingWorker worker;
  assert(worker.Submit({99, MakeRenders(), bounds}));
}

} // namespace

// Runs the fixture-symbol worker behavioral regression contract.
int main() {
  TestWorkerContract();
  TestIdleShutdown();
  TestPendingShutdown();
  return 0;
}
