#include "localization/app_language.h"
#include "localization/localization_manager.h"

#include <cassert>
#include <filesystem>
#include <string>

// Verifies supported language parsing and portable locale-root candidates.
int main() {
  using namespace localization;

  assert(ParseAppLanguageCode("en") == AppLanguage::English);
  assert(ParseAppLanguageCode("es") == AppLanguage::Spanish);
  assert(ParseAppLanguageCode("") == AppLanguage::English);
  assert(ParseAppLanguageCode("fr") == AppLanguage::English);
  assert(AppLanguageCode(AppLanguage::English) == "en");
  assert(AppLanguageCode(AppLanguage::Spanish) == "es");
  assert(DefaultAppLanguage() == AppLanguage::English);
  assert(IsSourceLanguage(AppLanguage::English));
  assert(!IsSourceLanguage(AppLanguage::Spanish));

  const auto roots = ResolveLocaleRootCandidatesForPaths(
      std::filesystem::path("/opt/perastage/Perastage"),
      std::filesystem::path("/workspace/Perastage/build"));
  assert(!roots.empty());
  bool hasInstalledResourceRoot = false;
  bool hasDevelopmentResourceRoot = false;
  for (const auto &root : roots) {
    const auto normalized = root.generic_string();
    if (normalized.find("/opt/perastage/resources/locale") != std::string::npos)
      hasInstalledResourceRoot = true;
    if (normalized.find("/workspace/Perastage/resources/locale") != std::string::npos)
      hasDevelopmentResourceRoot = true;
  }
  assert(hasInstalledResourceRoot);
  assert(hasDevelopmentResourceRoot);

  return 0;
}
