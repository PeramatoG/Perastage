#include "localization/app_language.h"
#include "localization/localization_manager.h"

#include <filesystem>
#include <iostream>
#include <string>

#include <wx/intl.h>

namespace {

// Reports a failed test condition and returns false.
bool Check(bool condition, const std::string &message) {
  if (condition)
    return true;
  std::cerr << "LocalizationAppLanguage: " << message << '\n';
  return false;
}

} // namespace

// Verifies supported language parsing and portable locale-root candidates.
int main() {
  using namespace localization;
  bool ok = true;

  ok &= Check(ParseAppLanguageCode("en") == AppLanguage::English,
              "en did not parse as English.");
#if PERASTAGE_ENABLE_LOCALIZATION
  ok &= Check(ParseAppLanguageCode("es") == AppLanguage::Spanish,
              "es did not parse as Spanish.");
  for (const std::string code : {"zh_CN", "zh-cn", "zh_cn", "ZH_CN",
                                "zh-Hans", "zh_Hans"}) {
    ok &= Check(ParseAppLanguageCode(code) == AppLanguage::SimplifiedChinese,
                code + " did not parse as Simplified Chinese.");
  }
#else
  ok &= Check(ParseAppLanguageCode("es") == AppLanguage::English,
              "es should fall back to English when localization is disabled.");
  ok &= Check(ParseAppLanguageCode("zh_CN") == AppLanguage::English,
              "zh_CN should fall back to English when localization is disabled.");
#endif
  ok &= Check(ParseAppLanguageCode("") == AppLanguage::English,
              "Empty code did not fall back to English.");
  ok &= Check(ParseAppLanguageCode("fr") == AppLanguage::English,
              "Unsupported code did not fall back to English.");
  ok &= Check(AppLanguageCode(AppLanguage::English) == "en",
              "English canonical code regressed.");
#if PERASTAGE_ENABLE_LOCALIZATION
  ok &= Check(AppLanguageCode(AppLanguage::Spanish) == "es",
              "Spanish canonical code regressed.");
  ok &= Check(AppLanguageCode(AppLanguage::SimplifiedChinese) == "zh_CN",
              "Simplified Chinese canonical code is not zh_CN.");
  ok &= Check(WxLanguageId(AppLanguage::SimplifiedChinese) ==
                  wxLANGUAGE_CHINESE_SIMPLIFIED,
              "Simplified Chinese wxWidgets language id regressed.");
#endif
  ok &= Check(DefaultAppLanguage() == AppLanguage::English,
              "Default language is not English.");
  ok &= Check(IsSourceLanguage(AppLanguage::English),
              "English is not treated as the source language.");
#if PERASTAGE_ENABLE_LOCALIZATION
  ok &= Check(!IsSourceLanguage(AppLanguage::Spanish),
              "Spanish is unexpectedly treated as the source language.");
  ok &= Check(!IsSourceLanguage(AppLanguage::SimplifiedChinese),
              "Simplified Chinese is unexpectedly treated as the source language.");
#endif

  const auto roots = ResolveLocaleRootCandidatesForPaths(
      std::filesystem::path("/opt/perastage/Perastage"),
      std::filesystem::path("/workspace/Perastage/build"));
  ok &= Check(!roots.empty(), "Locale-root candidates are empty.");
  bool hasInstalledResourceRoot = false;
  bool hasDevelopmentResourceRoot = false;
  for (const auto &root : roots) {
    const auto normalized = root.generic_string();
    if (normalized.find("/opt/perastage/resources/locale") != std::string::npos)
      hasInstalledResourceRoot = true;
    if (normalized.find("/workspace/Perastage/resources/locale") != std::string::npos)
      hasDevelopmentResourceRoot = true;
  }
  ok &= Check(hasInstalledResourceRoot, "Installed locale root was not resolved.");
  ok &= Check(hasDevelopmentResourceRoot, "Development locale root was not resolved.");

  return ok ? 0 : 1;
}
