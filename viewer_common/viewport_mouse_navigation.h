#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "../core/user_navigation_preferences.h"

namespace viewport_navigation {

enum class MouseButton { Left, Middle };
enum class Viewer3DAction { Orbit, Pan };

// Resolves a 3D primary navigation gesture without involving wxWidgets.
constexpr Viewer3DAction ResolveViewer3DAction(MouseButton button,
                                               bool shiftDown) {
  return button == MouseButton::Middle || shiftDown ? Viewer3DAction::Pan
                                                     : Viewer3DAction::Orbit;
}

// Reports whether a 2D gesture is reserved exclusively for viewport panning.
constexpr bool IsExclusiveViewer2DPan(MouseButton button) {
  return button == MouseButton::Middle;
}

// Allows MMB pan from idle or while continuous placement owns selection mode.
constexpr bool CanBeginViewer2DPan(bool interactionIdle,
                                   bool continuousPlacementActive) {
  return interactionIdle || continuousPlacementActive;
}

// Selects the safe 2D interaction state after a temporary viewport pan.
constexpr bool ShouldResumeViewer2DSelection(bool continuousPlacementActive) {
  return continuousPlacementActive;
}

// Applies independent inversion preferences to the existing orbit deltas.
constexpr std::pair<float, float>
ResolveOrbitDeltas(float horizontalDelta, float verticalDelta,
                   bool invertHorizontal, bool invertVertical) {
  return {invertHorizontal ? -horizontalDelta : horizontalDelta,
          invertVertical ? -verticalDelta : verticalDelta};
}

// Reads persisted checkbox values while defaulting missing keys off.
inline bool ReadBooleanPreference(const std::optional<std::string> &value) {
  return value && *value == "1";
}

} // namespace viewport_navigation
