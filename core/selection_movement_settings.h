#pragma once

#include "configmanager.h"

namespace selection_movement_settings {

constexpr const char *kAxisConstrainedMovementConfigKey =
    "selection_axis_constrained_movement";
constexpr const char *kLeftDragSelectionMovementConfigKey =
    "selection_left_drag_movement";
constexpr const char *kLocalTransformSpaceConfigKey =
    "selection_local_transform_space";

// Returns whether selection dragging should stay constrained to axes.
inline bool IsAxisConstrainedMovementEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kAxisConstrainedMovementConfigKey);
  return !value || *value != "0";
}

// Persists whether selection dragging should stay constrained to axes.
inline void SetAxisConstrainedMovementEnabled(ConfigManager &cfg, bool enabled) {
  cfg.SetValue(kAxisConstrainedMovementConfigKey, enabled ? "1" : "0");
}

// Returns whether left-click dragging may move selected scene elements.
inline bool IsLeftDragSelectionMovementEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kLeftDragSelectionMovementConfigKey);
  return value && *value == "1";
}

// Persists whether left-click dragging may move selected scene elements.
inline void SetLeftDragSelectionMovementEnabled(ConfigManager &cfg, bool enabled) {
  cfg.SetValue(kLeftDragSelectionMovementConfigKey, enabled ? "1" : "0");
}

// Returns whether viewport transforms should use local axes.
inline bool IsLocalTransformSpaceEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kLocalTransformSpaceConfigKey);
  return value && *value == "1";
}

// Persists whether viewport transforms should use local axes.
inline void SetLocalTransformSpaceEnabled(ConfigManager &cfg, bool enabled) {
  cfg.SetValue(kLocalTransformSpaceConfigKey, enabled ? "1" : "0");
}

} // namespace selection_movement_settings
