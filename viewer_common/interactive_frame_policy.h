#pragma once

#include <chrono>
#include <optional>

namespace interactive_frame {

struct PointerPosition {
  int x = 0;
  int y = 0;
};

PointerPosition ResolveLatestPointer(
    PointerPosition queued,
    const std::optional<PointerPosition> &liveInsideClient);

class Cadence {
public:
  using Clock = std::chrono::steady_clock;

  explicit Cadence(std::chrono::milliseconds interval =
                       std::chrono::milliseconds(16));
  bool IsPresentationDue(Clock::time_point now);
  void Reset();

private:
  std::chrono::milliseconds m_interval;
  std::optional<Clock::time_point> m_lastPresentation;
};

} // namespace interactive_frame
