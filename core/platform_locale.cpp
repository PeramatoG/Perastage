#include "platform_locale.h"

#include <clocale>
#include <cstdlib>
#include <string>
#include <string_view>

namespace platform {
namespace {

// Returns true when the locale name denotes UTF-8 text encoding.
bool IsUtf8Locale(std::string_view localeName) {
  return localeName.find("UTF-8") != std::string_view::npos ||
         localeName.find("utf8") != std::string_view::npos ||
         localeName.find("UTF8") != std::string_view::npos ||
         localeName.find("utf-8") != std::string_view::npos;
}

// Applies a CTYPE locale candidate and reports whether it became active.
bool TrySetCtypeLocale(const char *localeName, std::string &activeLocale) {
  const char *result = std::setlocale(LC_CTYPE, localeName);
  if (!result)
    return false;
  activeLocale = result;
  return IsUtf8Locale(activeLocale);
}

} // namespace

// Ensures Linux narrow-to-wide text conversions use a UTF-8 locale.
LocaleSetupResult EnsureProcessTextLocale() {
  LocaleSetupResult result;
#if defined(__linux__)
  std::string activeLocale;
  if (TrySetCtypeLocale("", activeLocale)) {
    result.activeLocale = activeLocale;
    return result;
  }

  const char *candidates[] = {"C.UTF-8", "C.utf8", "en_US.UTF-8"};
  for (const char *candidate : candidates) {
    if (TrySetCtypeLocale(candidate, activeLocale)) {
      result.changed = true;
      result.activeLocale = activeLocale;
      result.note = std::string("LC_CTYPE forced to ") + activeLocale;
      if (!std::getenv("LC_CTYPE"))
        setenv("LC_CTYPE", candidate, 0);
      return result;
    }
  }

  const char *fallback = std::setlocale(LC_CTYPE, nullptr);
  result.activeLocale = fallback ? fallback : "unknown";
  result.note = "Unable to activate a UTF-8 LC_CTYPE locale.";
#else
  const char *active = std::setlocale(LC_CTYPE, nullptr);
  result.activeLocale = active ? active : "";
#endif
  return result;
}

} // namespace platform
