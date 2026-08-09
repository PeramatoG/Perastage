#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "symbols/Symbol2DImageBuilder.h"
#include "tools/fixture_geometry_bounds.h"

namespace gui {

struct FixtureSymbolProcessingRequest {
  std::uint64_t epoch = 0;
  std::vector<symbols::RenderedSymbolImage> renders;
  tools::FixtureGeometryBounds bounds;
};

struct FixtureSymbolProcessingResult {
  std::uint64_t epoch = 0;
  bool ok = false;
  std::string error;
  std::vector<symbols::Symbol2D> symbols;
  tools::FixtureGeometryBounds bounds;
};

class FixtureSymbolProcessingWorker {
public:
  FixtureSymbolProcessingWorker();
  ~FixtureSymbolProcessingWorker();

  bool Submit(FixtureSymbolProcessingRequest request);
  std::optional<FixtureSymbolProcessingResult> TakeResult();

private:
  void Run(std::stop_token stopToken);

  std::mutex mutex_;
  std::condition_variable_any condition_;
  std::deque<FixtureSymbolProcessingRequest> requests_;
  std::deque<FixtureSymbolProcessingResult> results_;
  bool busy_ = false;
  std::jthread thread_;
};

} // namespace gui
