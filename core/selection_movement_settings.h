#pragma once

#include "configmanager.h"

namespace selection_movement_settings {

constexpr const char *kAxisConstrainedMovementConfigKey =
    "selection_axis_constrained_movement";

// Returns whether selection dragging should stay constrained to axes.
inline bool IsAxisConstrainedMovementEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kAxisConstrainedMovementConfigKey);
  return !value || *value != "0";
}

// Persists whether selection dragging should stay constrained to axes.
inline void SetAxisConstrainedMovementEnabled(ConfigManager &cfg, bool enabled) {
  cfg.SetValue(kAxisConstrainedMovementConfigKey, enabled ? "1" : "0");
}

} // namespace selection_movement_settings
