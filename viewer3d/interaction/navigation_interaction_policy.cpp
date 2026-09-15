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
    const auto deltas = viewport_navigation::ResolveOrbitDeltas(
        static_cast<float>(deltaX) * 0.5f, -static_cast<float>(deltaY) * 0.5f,
        input.orbitPreferences.invertHorizontal,
        input.orbitPreferences.invertVertical);
    return CameraDragIntent{CameraDragIntent::Action::Orbit, deltas.first,
                            deltas.second};
  }
  if (input.mode == NavigationMode::Pan && input.panGestureActive) {
    return CameraDragIntent{CameraDragIntent::Action::Pan,
                            -static_cast<float>(deltaX) * 0.01f,
                            static_cast<float>(deltaY) * 0.01f};
  }
  return std::nullopt;
}

// Resolves an arrow-key gesture into the established camera action and delta.
KeyboardCameraIntent
ResolveKeyboardCameraInput(const KeyboardCameraInput &input) {
  const float horizontalDirection =
      input.direction == CameraDirection::Left    ? -1.0f
      : input.direction == CameraDirection::Right ? 1.0f
                                                  : 0.0f;
  const float verticalDirection = input.direction == CameraDirection::Up ? 1.0f
                                  : input.direction == CameraDirection::Down
                                      ? -1.0f
                                      : 0.0f;

  if (input.shiftDown) {
    return {KeyboardCameraIntent::Action::Pan, horizontalDirection * 0.1f,
            verticalDirection * 0.1f};
  }
  if (input.altDown) {
    const float zoom = input.direction == CameraDirection::Left ||
                               input.direction == CameraDirection::Up
                           ? -1.0f
                           : 1.0f;
    return {KeyboardCameraIntent::Action::Zoom, zoom, 0.0f};
  }
  return {KeyboardCameraIntent::Action::Orbit, horizontalDirection * 5.0f,
          verticalDirection * 5.0f};
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

} // namespace viewer3d::interaction
