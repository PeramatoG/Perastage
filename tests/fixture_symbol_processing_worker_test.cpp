#include <cassert>
#include <chrono>
#include <thread>
#include <utility>

#include "services/fixture_symbol_processing_worker.h"
#include "tools/scene_model_symbol_processing.h"

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

// Compares two points exactly because both results use identical input data.
bool SamePoint(const symbols::Point2D &left, const symbols::Point2D &right) {
  return left.x == right.x && left.y == right.y;
}

// Compares two polylines exactly in their generated order.
bool SameLine(const symbols::Polyline2D &left,
              const symbols::Polyline2D &right) {
  if (left.size() != right.size())
    return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (!SamePoint(left[index], right[index]))
      return false;
  }
  return true;
}

// Compares generated symbol geometry and metadata exactly.
bool SameSymbol(const symbols::Symbol2D &left,
                const symbols::Symbol2D &right) {
  if (left.view != right.view || left.strokeWidthPx != right.strokeWidthPx ||
      !SamePoint(left.bounds.min, right.bounds.min) ||
      !SamePoint(left.bounds.max, right.bounds.max) ||
      left.bounds.valid != right.bounds.valid ||
      left.fill.size() != right.fill.size() ||
      left.strokes.size() != right.strokes.size())
    return false;
  for (std::size_t index = 0; index < left.fill.size(); ++index) {
    const auto &leftPolygon = left.fill[index];
    const auto &rightPolygon = right.fill[index];
    if (!SameLine(leftPolygon.outer, rightPolygon.outer) ||
        leftPolygon.holes.size() != rightPolygon.holes.size())
      return false;
    for (std::size_t hole = 0; hole < leftPolygon.holes.size(); ++hole) {
      if (!SameLine(leftPolygon.holes[hole], rightPolygon.holes[hole]))
        return false;
    }
  }
  for (std::size_t index = 0; index < left.strokes.size(); ++index) {
    if (!SameLine(left.strokes[index], right.strokes[index]))
      return false;
  }
  return true;
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

// Verifies processing parity, single-result delivery, and worker reuse.
void TestWorkerContract() {
  tools::FixtureGeometryBounds bounds;
  bounds.min = {-40.0f, -25.0f, -60.0f};
  bounds.max = {80.0f, 45.0f, 90.0f};
  bounds.valid = true;
  const auto renders = MakeRenders();
  const auto direct =
      tools::ProcessSceneModelOrthographicRenders(renders, bounds);
  assert(direct.ok);

  gui::FixtureSymbolProcessingWorker worker;
  assert(worker.Submit({4182, renders, bounds}));
  assert(!worker.Submit({4183, renders, bounds}));
  auto result = WaitForResult(worker);
  assert(result && result->ok && result->epoch == 4182);
  assert(result->symbols.size() == 4);
  assert(result->bounds.min == bounds.min && result->bounds.max == bounds.max &&
         result->bounds.valid == bounds.valid);
  assert(result->symbols.size() == direct.symbols.size());
  for (std::size_t index = 0; index < result->symbols.size(); ++index)
    assert(SameSymbol(result->symbols[index], direct.symbols[index]));
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

// Verifies the manual-processing barrier drains active work without losing its result.
void TestManualProcessingBarrier() {
  tools::FixtureGeometryBounds bounds;
  bounds.valid = true;
  const auto renders = MakeRenders();
  gui::FixtureSymbolProcessingWorker worker;
  assert(!worker.WaitUntilIdle());
  assert(worker.Submit({7001, renders, bounds}));
  assert(worker.WaitUntilIdle());
  const auto result = worker.TakeResult();
  assert(result && result->ok && result->epoch == 7001);
  const auto direct =
      tools::ProcessSceneModelOrthographicRenders(renders, bounds);
  assert(direct.ok && direct.symbols.size() == result->symbols.size());
  for (std::size_t index = 0; index < result->symbols.size(); ++index)
    assert(SameSymbol(result->symbols[index], direct.symbols[index]));
}

} // namespace

// Runs the fixture-symbol worker behavioral regression contract.
int main() {
  TestWorkerContract();
  TestIdleShutdown();
  TestPendingShutdown();
  TestManualProcessingBarrier();
  return 0;
}
