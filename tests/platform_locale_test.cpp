#include "platform_locale.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// Returns true when the locale name denotes UTF-8 text encoding.
bool IsUtf8Locale(const std::string &localeName) {
  return localeName.find("UTF-8") != std::string::npos ||
         localeName.find("utf8") != std::string::npos ||
         localeName.find("UTF8") != std::string::npos ||
         localeName.find("utf-8") != std::string::npos;
}

} // namespace

// Verifies the platform text locale setup selects UTF-8 on Linux.
int main() {
  const platform::LocaleSetupResult result = platform::EnsureProcessTextLocale();
#if defined(__linux__)
  if (!IsUtf8Locale(result.activeLocale)) {
    std::cerr << "Expected UTF-8 LC_CTYPE on Linux, got '"
              << result.activeLocale << "' note='" << result.note << "'\n";
    return EXIT_FAILURE;
  }
#endif
  return EXIT_SUCCESS;
}
