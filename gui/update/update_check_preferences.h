#pragma once

#include <chrono>
#include <optional>
#include <string>

class IGuiPreferencesService;

namespace gui::update {

enum class StartupCheckMode { Disabled, StartupRecommended, ManualOnly };

// Returns the configured startup-check mode from persisted GUI preferences.
StartupCheckMode ReadStartupCheckMode(const IGuiPreferencesService &preferences);

// Persists the selected startup-check mode in GUI preferences.
void WriteStartupCheckMode(IGuiPreferencesService &preferences,
                           StartupCheckMode mode);

// Returns true when startup checks are enabled and the cooldown has elapsed.
bool ShouldRunStartupCheckNow(const IGuiPreferencesService &preferences,
                              std::chrono::system_clock::time_point now,
                              std::chrono::hours cooldown =
                                  std::chrono::hours(24));

// Stores the UTC timestamp of the most recent automatic startup check.
void MarkStartupCheckRun(IGuiPreferencesService &preferences,
                         std::chrono::system_clock::time_point now);

} // namespace gui::update
