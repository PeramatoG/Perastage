#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace viewport_navigation {

enum class MouseButton { Left, Middle };
enum class Viewer3DAction { Orbit, Pan };

inline constexpr std::string_view kVerticalOrbitInversionConfigKey =
    "viewer3d_invert_orbit";
inline constexpr std::string_view kHorizontalOrbitInversionConfigKey =
    "viewer3d_invert_orbit_horizontal";

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
