#pragma once

#include <chrono>
#include <cstddef>

namespace viewer3d::resources {
struct ResourceLoadProgress {
  size_t completed = 0, total = 0, failed = 0, representationsPending = 0;
  bool Pending() const { return completed < total || representationsPending > 0; }
};

// Loading gets short event-loop slices independently of expensive presentation.
// Measure the interval from the END of a paint, so slow frames cannot consume
// every loading slice. Input-triggered paints remain unrestricted.
class ProgressiveLoadCadence {
public:
  using Clock = std::chrono::steady_clock;
  static constexpr size_t kMaximumResources = 128;
  static constexpr std::chrono::milliseconds kWorkSlice{12};
  static constexpr std::chrono::milliseconds kPresentationInterval{150};
  void Presented(Clock::time_point now) { lastPresentation = now; hasPresented = true; }
  bool PresentationDue(Clock::time_point now, bool complete) const {
    return complete || !hasPresented || now - lastPresentation >= kPresentationInterval;
  }
private:
  Clock::time_point lastPresentation{};
  bool hasPresented = false;
};
} // namespace viewer3d::resources
