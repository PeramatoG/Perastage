#include "localization/app_language.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace localization {
namespace {
const std::vector<AppLanguageOption> kSupportedLanguages = {
    {AppLanguage::English, "en", "English"},
#if PERASTAGE_ENABLE_LOCALIZATION
    {AppLanguage::Spanish, "es", "Spanish"},
#endif
};

// Returns an ASCII-lowercase copy of a persisted language code.
std::string NormalizeLanguageCode(std::string_view code) {
  std::string normalized(code);
  normalized.erase(
      std::remove_if(normalized.begin(), normalized.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
      }),
      normalized.end());
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return normalized;
}
} // namespace

// Returns the source and default application language.
AppLanguage DefaultAppLanguage() { return AppLanguage::English; }

// Returns all supported application languages in display order.
const std::vector<AppLanguageOption> &SupportedAppLanguages() {
  return kSupportedLanguages;
}

// Parses a persisted language code and falls back to English when unsupported.
AppLanguage ParseAppLanguageCode(std::string_view code) {
  const std::string normalized = NormalizeLanguageCode(code);
  for (const auto &option : kSupportedLanguages) {
    if (normalized == option.code)
      return option.language;
  }
  return DefaultAppLanguage();
}

// Returns the canonical persisted language code for a supported language.
std::string_view AppLanguageCode(AppLanguage language) {
  for (const auto &option : kSupportedLanguages) {
    if (option.language == language)
      return option.code;
  }
  return "en";
}

// Returns the source-language display name for a supported language.
std::string_view AppLanguageDisplayName(AppLanguage language) {
  for (const auto &option : kSupportedLanguages) {
    if (option.language == language)
      return option.displayName;
  }
  return "English";
}

// Returns true when the language uses the built-in source strings.
bool IsSourceLanguage(AppLanguage language) {
  return language == AppLanguage::English;
}

} // namespace localization
