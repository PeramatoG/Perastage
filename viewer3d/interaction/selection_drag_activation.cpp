#include "selection_drag_activation.h"

#include <cstdlib>

namespace viewer3d::interaction {

// Arms selection dragging at the supplied monotonic time.
void SelectionDragActivation::Arm(std::chrono::steady_clock::time_point now) {
  m_armedAt = now;
  m_armed = true;
  m_moved = false;
}

// Resolves whether elapsed time and pointer travel activate the drag.
SelectionDragActivation::Decision
SelectionDragActivation::Evaluate(std::chrono::steady_clock::time_point now,
                                  int deltaX, int deltaY) const {
  if (!m_armed)
    return Decision::Inactive;
  if (now - m_armedAt < kActivationDelay)
    return Decision::WaitingForDelay;
  if (!m_moved && std::abs(deltaX) < kActivationThresholdPixels &&
      std::abs(deltaY) < kActivationThresholdPixels) {
    return Decision::WaitingForMovement;
  }
  return Decision::Active;
}

// Records that projected selection movement was applied.
void SelectionDragActivation::MarkMoved() { m_moved = true; }

// Clears all prepared and active drag state.
void SelectionDragActivation::Reset() {
  m_armedAt = {};
  m_armed = false;
  m_moved = false;
}

} // namespace viewer3d::interaction
