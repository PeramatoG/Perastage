#include "navigation_interaction_policy.h"

#include <cmath>

namespace viewer3d::interaction {

// Starts a navigation gesture and clears movement from any prior gesture.
void NavigationSession::Begin(NavigationMode mode) {
  m_mode = mode;
  m_active = mode != NavigationMode::None;
  m_moved = false;
}

// Records that the active pointer gesture produced movement.
void NavigationSession::MarkMoved() { m_moved = true; }

// Ends navigation while retaining movement for click suppression.
void NavigationSession::End() {
  m_mode = NavigationMode::None;
  m_active = false;
}

// Clears movement without changing whether navigation is active.
void NavigationSession::ClearMoved() { m_moved = false; }

// Clears all transient navigation state after completion or cancellation.
void NavigationSession::Reset() {
  m_mode = NavigationMode::None;
  m_active = false;
  m_moved = false;
}

// Resolves a pointer movement into a camera action without widget dependencies.
std::optional<CameraDragIntent>
ResolveCameraDrag(const CameraDragInput &input) {
  const int deltaX = input.current.x - input.previous.x;
  const int deltaY = input.current.y - input.previous.y;
  if (deltaX == 0 && deltaY == 0)
    return std::nullopt;

  if (input.mode == NavigationMode::Orbit && input.orbitButtonDown) {
    float horizontal = static_cast<float>(deltaX) * 0.5f;
    float vertical = -static_cast<float>(deltaY) * 0.5f;
    if (input.orbitPreferences.invertHorizontal)
      horizontal = -horizontal;
    if (input.orbitPreferences.invertVertical)
      vertical = -vertical;
    return CameraDragIntent{CameraDragIntent::Action::Orbit, horizontal,
                            vertical};
  }
  if (input.mode == NavigationMode::Pan && input.panGestureActive) {
    return CameraDragIntent{CameraDragIntent::Action::Pan,
                            -static_cast<float>(deltaX) * 0.01f,
                            static_cast<float>(deltaY) * 0.01f};
  }
  return std::nullopt;
}

// Converts a platform wheel report into the established signed zoom step count.
std::optional<float> ResolveWheelZoomSteps(int rotation, int wheelDelta) {
  if (wheelDelta == 0)
    return std::nullopt;
  const float steps =
      -static_cast<float>(rotation) / static_cast<float>(wheelDelta);
  if (steps == 0.0f || !std::isfinite(steps))
    return std::nullopt;
  return steps;
}

// Reports whether pointer travel has crossed the per-axis selection drag
// threshold.
bool HasSelectionDragStarted(int deltaX, int deltaY, int thresholdPixels) {
  return std::abs(deltaX) >= thresholdPixels ||
         std::abs(deltaY) >= thresholdPixels;
}

} // namespace viewer3d::interaction
