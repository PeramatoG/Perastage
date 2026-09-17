#include "interactive_frame_policy.h"

#include <cassert>
#include <chrono>

// Verifies latest-pointer selection and bounded frame presentation.
int main() {
  using namespace std::chrono_literals;
  const auto latest = interactive_frame::ResolveLatestPointer(
      {105, 100}, interactive_frame::PointerPosition{140, 100});
  assert(latest.x == 140 && latest.y == 100);
  const auto queued =
      interactive_frame::ResolveLatestPointer({105, 100}, std::nullopt);
  assert(queued.x == 105 && queued.y == 100);

  interactive_frame::Cadence cadence(16ms);
  const auto start = interactive_frame::Cadence::Clock::time_point{};
  assert(cadence.IsPresentationDue(start));
  assert(!cadence.IsPresentationDue(start + 1ms));
  assert(!cadence.IsPresentationDue(start + 15ms));
  assert(cadence.IsPresentationDue(start + 16ms));
  cadence.Reset();
  assert(cadence.IsPresentationDue(start + 17ms));
  return 0;
}
