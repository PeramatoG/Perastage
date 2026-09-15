#pragma once

#include <optional>

namespace viewer3d::interaction {

enum class NavigationMode { None, Orbit, Pan };

class NavigationSession {
public:
  // Starts a navigation gesture and clears movement from any prior gesture.
  void Begin(NavigationMode mode);
  // Records that the active pointer gesture produced movement.
  void MarkMoved();
  // Ends navigation while retaining movement for click suppression.
  void End();
  // Clears movement without changing whether navigation is active.
  void ClearMoved();
  // Clears all transient navigation state after completion or cancellation.
  void Reset();

  bool IsActive() const { return m_active; }
  bool HasMoved() const { return m_moved; }
  NavigationMode GetMode() const { return m_mode; }

private:
  NavigationMode m_mode = NavigationMode::None;
  bool m_active = false;
  bool m_moved = false;
};

struct PointerPosition {
  int x = 0;
  int y = 0;
};

struct OrbitPreferences {
  bool invertHorizontal = false;
  bool invertVertical = false;
};

struct CameraDragInput {
  NavigationMode mode = NavigationMode::None;
  PointerPosition previous;
  PointerPosition current;
  bool orbitButtonDown = false;
  bool panGestureActive = false;
  OrbitPreferences orbitPreferences;
};

struct CameraDragIntent {
  enum class Action { Orbit, Pan };

  Action action = Action::Orbit;
  float horizontal = 0.0f;
  float vertical = 0.0f;
};

// Resolves a pointer movement into a camera action without widget dependencies.
std::optional<CameraDragIntent> ResolveCameraDrag(const CameraDragInput &input);

// Converts a platform wheel report into the established signed zoom step count.
std::optional<float> ResolveWheelZoomSteps(int rotation, int wheelDelta);

// Reports whether pointer travel has crossed the per-axis selection drag
// threshold.
bool HasSelectionDragStarted(int deltaX, int deltaY, int thresholdPixels);

} // namespace viewer3d::interaction
