#pragma once

#include <string_view>
#include <vector>

namespace localization {

enum class AppLanguage { English, Spanish };

struct AppLanguageOption {
  AppLanguage language;
  std::string_view code;
  std::string_view displayName;
};

inline constexpr const char *kUiLanguageConfigKey = "ui_language";

// Returns the source and default application language.
AppLanguage DefaultAppLanguage();

// Returns all supported application languages in display order.
const std::vector<AppLanguageOption> &SupportedAppLanguages();

// Parses a persisted language code and falls back to English when unsupported.
AppLanguage ParseAppLanguageCode(std::string_view code);

// Returns the canonical persisted language code for a supported language.
std::string_view AppLanguageCode(AppLanguage language);

// Returns the source-language display name for a supported language.
std::string_view AppLanguageDisplayName(AppLanguage language);

// Returns true when the language uses the built-in source strings.
bool IsSourceLanguage(AppLanguage language);

} // namespace localization
