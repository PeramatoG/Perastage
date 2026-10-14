#include "resources/progressive_load_policy.h"
#include <cassert>
#include <chrono>

int main() {
  using namespace viewer3d::resources;
  using namespace std::chrono_literals;
  const auto start = ProgressiveLoadCadence::Clock::time_point{1ms};
  ProgressiveLoadCadence cadence;
  assert(cadence.PresentationDue(start, false));
  cadence.Presented(start);
  assert(!cadence.PresentationDue(start + 149ms, false));
  assert(cadence.PresentationDue(start + 150ms, false));
  // A slow rendered frame gets a full loading interval after presentation.
  cadence.Presented(start + 500ms);
  assert(!cadence.PresentationDue(start + 501ms, false));
  assert(cadence.PresentationDue(start + 501ms, true));
  assert(cadence.PresentationDue(start + 650ms, false));
  assert((ResourceLoadProgress{4, 10}.Pending()));
  assert((ResourceLoadProgress{10, 10, 1, 2}.Pending()));
  assert((!ResourceLoadProgress{10, 10, 1, 0}.Pending()));
}
