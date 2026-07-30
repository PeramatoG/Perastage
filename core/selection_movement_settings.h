#pragma once

#include "configmanager.h"
#include "interactive_transform_policy.h"

namespace selection_movement_settings {

constexpr const char *kAxisConstrainedMovementConfigKey =
    "selection_axis_constrained_movement";
constexpr const char *kLeftDragSelectionMovementConfigKey =
    "selection_left_drag_movement";
constexpr const char *kLocalTransformSpaceConfigKey =
    "selection_local_transform_space";
constexpr const char *kGroupMoveFixtureConfigKey =
    "selection_group_move_fixture";
constexpr const char *kGroupMoveTrussConfigKey = "selection_group_move_truss";
constexpr const char *kGroupMoveSupportConfigKey =
    "selection_group_move_support";
constexpr const char *kGroupMoveSceneObjectConfigKey =
    "selection_group_move_scene_object";

// Loads the user policy while retaining legacy grouped-truss defaults.
inline scene_grouping::InteractiveTransformPolicy
LoadInteractiveTransformPolicy(const ConfigManager &cfg) {
  auto enabled = [&](const char *key, bool fallback) {
    const auto value = cfg.GetValue(key);
    return value ? *value == "1" : fallback;
  };
  return {.promoteFixturesToGroup = enabled(kGroupMoveFixtureConfigKey, false),
          .promoteTrussesToGroup = enabled(kGroupMoveTrussConfigKey, true),
          .promoteSupportsToGroup = enabled(kGroupMoveSupportConfigKey, false),
          .promoteSceneObjectsToGroup =
              enabled(kGroupMoveSceneObjectConfigKey, false)};
}

// Persists all grouped-object movement choices in user configuration.
inline void SaveInteractiveTransformPolicy(
    ConfigManager &cfg,
    const scene_grouping::InteractiveTransformPolicy &policy) {
  cfg.SetValue(kGroupMoveFixtureConfigKey,
               policy.promoteFixturesToGroup ? "1" : "0");
  cfg.SetValue(kGroupMoveTrussConfigKey,
               policy.promoteTrussesToGroup ? "1" : "0");
  cfg.SetValue(kGroupMoveSupportConfigKey,
               policy.promoteSupportsToGroup ? "1" : "0");
  cfg.SetValue(kGroupMoveSceneObjectConfigKey,
               policy.promoteSceneObjectsToGroup ? "1" : "0");
}

// Returns whether selection dragging should stay constrained to axes.
inline bool IsAxisConstrainedMovementEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kAxisConstrainedMovementConfigKey);
  return !value || *value != "0";
}

// Persists whether selection dragging should stay constrained to axes.
inline void SetAxisConstrainedMovementEnabled(ConfigManager &cfg,
                                              bool enabled) {
  cfg.SetValue(kAxisConstrainedMovementConfigKey, enabled ? "1" : "0");
}

// Returns whether left-click dragging may move selected scene elements.
inline bool IsLeftDragSelectionMovementEnabled(const ConfigManager &cfg) {
  const auto value = cfg.GetValue(kLeftDragSelectionMovementConfigKey);
  return value && *value == "1";
}

// Persists whether left-click dragging may move selected scene elements.
inline void SetLeftDragSelectionMovementEnabled(ConfigManager &cfg,
                                                bool enabled) {
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
