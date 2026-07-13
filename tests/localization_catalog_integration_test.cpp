#include "localization/app_language.h"
#include "localization/localization_manager.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/utils.h>
#include <wx/wx.h>

namespace {

// Returns the UTF-8 bytes for a wxString.
std::string ToUtf8(const wxString &text) {
  const wxScopedCharBuffer utf8 = text.ToUTF8();
  return utf8 ? std::string(utf8.data(), utf8.length()) : std::string();
}

// Returns the expected Spanish label bytes without using a translated source literal.
std::string ExpectedSpanishLanguageName() {
  return std::string("Espa") + std::string("\xC3\xB1") + "ol";
}

} // namespace

// Verifies Spanish catalog loading and safe fallback without creating GUI windows.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const char *localeRoot = std::getenv("PERASTAGE_LOCALE_ROOT");
  assert(localeRoot && *localeRoot);
  const std::filesystem::path catalogPath =
      std::filesystem::u8path(localeRoot) / "es" / "LC_MESSAGES" /
      "perastage.mo";
  assert(std::filesystem::is_regular_file(catalogPath));

  auto &manager = localization::LocalizationManager::Get();
  wxSetEnv("PERASTAGE_LOCALE_ROOT",
           wxFileName::GetTempDir() + "/perastage-empty-locale-root");
  const auto fallbackResult = manager.Initialize(localization::AppLanguage::Spanish);
  assert(!fallbackResult.catalogFound);
  assert(!fallbackResult.catalogLoaded);
  assert(fallbackResult.activeLanguage == localization::AppLanguage::English);
  assert(manager.ActiveLanguage() == localization::AppLanguage::English);
  assert(ToUtf8(_("Preferences")) == "Preferences");

  wxSetEnv("PERASTAGE_LOCALE_ROOT", wxString::FromUTF8(localeRoot));
  const auto spanishResult = manager.Initialize(localization::AppLanguage::Spanish);
  assert(spanishResult.catalogFound);
  assert(spanishResult.catalogLoaded);
  assert(spanishResult.activeLanguage == localization::AppLanguage::Spanish);
  assert(manager.ActiveLanguage() == localization::AppLanguage::Spanish);
  assert(ToUtf8(_("Preferences")) == "Preferencias");
  assert(ToUtf8(_("Spanish")) == ExpectedSpanishLanguageName());

  return 0;
}
