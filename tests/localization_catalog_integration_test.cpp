#include "localization/app_language.h"
#include "localization/localization_manager.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/utils.h>
#include <wx/wx.h>

namespace {

// Reports a failed test condition and returns false.
bool Check(bool condition, const std::string &message) {
  if (condition)
    return true;
  std::cerr << "LocalizationCatalogIntegration: " << message << '\n';
  return false;
}

// Returns the expected Spanish label using explicit Unicode code points.
wxString ExpectedSpanishLanguageName() {
  wxString text("Espa");
  text += wxUniChar(0x00F1);
  text += "ol";
  return text;
}

// Returns true when the text contains mojibake markers for UTF-8 ñ.
bool ContainsSpanishMojibake(const wxString &text) {
  return text.Find(wxUniChar(0x00C3)) != wxNOT_FOUND ||
         text.Find(wxUniChar(0x00B1)) != wxNOT_FOUND;
}

} // namespace

// Verifies Spanish catalog loading and safe fallback without creating GUI windows.
int main() {
  bool ok = true;
  wxInitializer initializer;
  ok &= Check(initializer.IsOk(), "wxWidgets initialization failed.");
  if (!ok)
    return 1;

  const char *localeRoot = std::getenv("PERASTAGE_LOCALE_ROOT");
  ok &= Check(localeRoot && *localeRoot,
              "PERASTAGE_LOCALE_ROOT must point to the generated locale root.");
  if (!ok)
    return 1;

  const std::filesystem::path catalogPath =
      std::filesystem::u8path(localeRoot) / "es" / "LC_MESSAGES" /
      "perastage.mo";
  ok &= Check(std::filesystem::is_regular_file(catalogPath),
              "Generated Spanish catalog is missing: " + catalogPath.string());

  auto &manager = localization::LocalizationManager::Get();
  wxSetEnv("PERASTAGE_LOCALE_ROOT",
           wxFileName::GetTempDir() + "/perastage-empty-locale-root");
  const auto fallbackResult = manager.Initialize(localization::AppLanguage::Spanish);
  ok &= Check(!fallbackResult.catalogFound,
              "Missing-catalog scenario unexpectedly found a catalog.");
  ok &= Check(!fallbackResult.catalogLoaded,
              "Missing-catalog scenario unexpectedly loaded a catalog.");
  ok &= Check(fallbackResult.activeLanguage == localization::AppLanguage::English,
              "Missing-catalog scenario did not fall back to English.");
  ok &= Check(manager.ActiveLanguage() == localization::AppLanguage::English,
              "Manager active language is not English after missing-catalog fallback.");
  ok &= Check(_("Preferences") == wxString("Preferences"),
              "Missing-catalog fallback did not return English source text.");

  wxSetEnv("PERASTAGE_LOCALE_ROOT", wxString::FromUTF8(localeRoot));
  const auto spanishResult = manager.Initialize(localization::AppLanguage::Spanish);
  const wxString spanishName = _("Spanish");
  ok &= Check(spanishResult.catalogFound, "Spanish catalog was not found.");
  ok &= Check(spanishResult.catalogLoaded, "Spanish catalog was not loaded.");
  ok &= Check(spanishResult.activeLanguage == localization::AppLanguage::Spanish,
              "Spanish did not become the active language after catalog loading.");
  ok &= Check(manager.ActiveLanguage() == localization::AppLanguage::Spanish,
              "Manager active language is not Spanish after catalog loading.");
  ok &= Check(_("Preferences") == wxString("Preferencias"),
              "Preferences did not translate to Preferencias.");
  ok &= Check(_("Language") == wxString("Idioma"),
              "Language did not translate to Idioma.");
  ok &= Check(spanishName == ExpectedSpanishLanguageName(),
              "Spanish did not translate to the expected native language name.");
  ok &= Check(spanishName.Find(wxUniChar(0x00F1)) != wxNOT_FOUND,
              "Spanish native name does not contain U+00F1.");
  ok &= Check(!ContainsSpanishMojibake(spanishName),
              "Spanish native name contains mojibake markers.");

  return ok ? 0 : 1;
}
