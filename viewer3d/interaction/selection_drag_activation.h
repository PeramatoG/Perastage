#pragma once

#include <chrono>

namespace viewer3d::interaction {

class SelectionDragActivation {
public:
  enum class Decision { Inactive, WaitingForDelay, WaitingForMovement, Active };

  // Arms selection dragging at the supplied monotonic time.
  void Arm(std::chrono::steady_clock::time_point now);
  // Resolves whether elapsed time and pointer travel activate the drag.
  Decision Evaluate(std::chrono::steady_clock::time_point now, int deltaX,
                    int deltaY) const;
  // Records that projected selection movement was applied.
  void MarkMoved();
  // Clears all prepared and active drag state.
  void Reset();

  bool IsArmed() const { return m_armed; }
  bool HasMoved() const { return m_moved; }

private:
  static constexpr auto kActivationDelay = std::chrono::milliseconds(120);
  static constexpr int kActivationThresholdPixels = 3;

  std::chrono::steady_clock::time_point m_armedAt{};
  bool m_armed = false;
  bool m_moved = false;
};

} // namespace viewer3d::interaction
