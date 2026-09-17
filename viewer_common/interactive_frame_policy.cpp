#include "interactive_frame_policy.h"

namespace interactive_frame {

// Selects the current live pointer when it is valid for the client area.
PointerPosition ResolveLatestPointer(
    PointerPosition queued,
    const std::optional<PointerPosition> &liveInsideClient) {
  return liveInsideClient.value_or(queued);
}

// Creates a bounded synchronous-presentation cadence.
Cadence::Cadence(std::chrono::milliseconds interval) : m_interval(interval) {}

// Consumes one presentation slot when the frame interval has elapsed.
bool Cadence::IsPresentationDue(Clock::time_point now) {
  if (m_lastPresentation && now - *m_lastPresentation < m_interval)
    return false;
  m_lastPresentation = now;
  return true;
}

// Makes the next changed transform immediately presentation-eligible.
void Cadence::Reset() { m_lastPresentation.reset(); }

} // namespace interactive_frame
