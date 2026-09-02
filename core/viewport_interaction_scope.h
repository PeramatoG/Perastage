#pragma once

namespace viewport_interaction_scope {

inline constexpr const char *kCrossTableActionsConfigKey =
    "viewport_cross_table_actions_enabled";

// Returns the default state for cross-table viewport interactions.
inline constexpr bool CrossTableActionsDefaultEnabled() { return false; }

} // namespace viewport_interaction_scope
