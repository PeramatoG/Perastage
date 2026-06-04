#include "update/update_check_preferences.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include "guiconfigservices.h"

namespace gui::update {
namespace {

constexpr const char *kUpdateModeKey = "app_update_startup_mode";
constexpr const char *kUpdateLastCheckEpochSecondsKey =
    "app_update_last_startup_check_epoch_seconds";
constexpr const char *kDismissedStartupReminderVersionKey =
    "app_update_dismissed_startup_reminder_version";

// Normalizes persisted mode tokens to lowercase for robust parsing.
std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

// Converts the enum mode to a stable config token for persistence.
std::string ModeToConfigValue(const StartupCheckMode mode) {
  switch (mode) {
  case StartupCheckMode::ManualOnly:
    return "manual_only";
  case StartupCheckMode::StartupRecommended:
  default:
    return "startup";
  }
}

// Parses the stored epoch-seconds string into a time point when valid.
std::optional<std::chrono::system_clock::time_point>
ParseLastCheckTime(const IGuiPreferencesService &preferences) {
  const auto persisted = preferences.GetValue(kUpdateLastCheckEpochSecondsKey);
  if (!persisted.has_value() || persisted->empty())
    return std::nullopt;

  try {
    const long long epochSeconds = std::stoll(*persisted);
    return std::chrono::system_clock::time_point(std::chrono::seconds(epochSeconds));
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace

// Returns the configured startup-check mode from persisted GUI preferences.
StartupCheckMode ReadStartupCheckMode(const IGuiPreferencesService &preferences) {
  const auto raw = preferences.GetValue(kUpdateModeKey);
  if (!raw.has_value())
    return StartupCheckMode::StartupRecommended;

  const std::string normalized = ToLowerCopy(*raw);
  if (normalized == "manual_only" || normalized == "never_auto")
    return StartupCheckMode::ManualOnly;
  return StartupCheckMode::StartupRecommended;
}

// Persists the selected startup-check mode in GUI preferences.
void WriteStartupCheckMode(IGuiPreferencesService &preferences,
                           const StartupCheckMode mode) {
  preferences.SetValue(kUpdateModeKey, ModeToConfigValue(mode));
}

// Returns true when startup checks are enabled and the cooldown has elapsed.
bool ShouldRunStartupCheckNow(const IGuiPreferencesService &preferences,
                              const std::chrono::system_clock::time_point now,
                              const std::chrono::hours cooldown) {
  if (ReadStartupCheckMode(preferences) != StartupCheckMode::StartupRecommended)
    return false;

  const auto lastCheck = ParseLastCheckTime(preferences);
  if (!lastCheck.has_value())
    return true;

  return now - *lastCheck >= cooldown;
}

// Stores the UTC timestamp of the most recent automatic startup check.
void MarkStartupCheckRun(IGuiPreferencesService &preferences,
                         const std::chrono::system_clock::time_point now) {
  const auto secondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();
  preferences.SetValue(kUpdateLastCheckEpochSecondsKey,
                       std::to_string(secondsSinceEpoch));
}

// Returns true when startup should show a reminder for the discovered version.
bool ShouldShowStartupUpdateReminder(const IGuiPreferencesService &preferences,
                                     const std::string &latestVersion) {
  if (latestVersion.empty())
    return true;

  const auto dismissedVersion =
      preferences.GetValue(kDismissedStartupReminderVersionKey);
  return !dismissedVersion.has_value() || *dismissedVersion != latestVersion;
}

// Persists the latest version dismissed from an automatic startup reminder.
void WriteDismissedStartupReminderVersion(IGuiPreferencesService &preferences,
                                          const std::string &latestVersion) {
  preferences.SetValue(kDismissedStartupReminderVersionKey, latestVersion);
}

} // namespace gui::update
