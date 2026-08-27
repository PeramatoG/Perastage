#include "localization/app_language.h"
#include "localization/localization_manager.h"
#include "localized_unit_labels.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <system_error>
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

// Returns the expected Simplified Chinese native language label.
wxString ExpectedChineseLanguageName() {
  wxString text;
  text += wxUniChar(0x7B80);
  text += wxUniChar(0x4F53);
  text += wxUniChar(0x4E2D);
  text += wxUniChar(0x6587);
  return text;
}

// Returns true when the text contains mojibake markers for UTF-8 ñ.
bool ContainsSpanishMojibake(const wxString &text) {
  return text.Find(wxUniChar(0x00C3)) != wxNOT_FOUND ||
         text.Find(wxUniChar(0x00B1)) != wxNOT_FOUND;
}

// Returns true when text contains Unicode replacement characters.
bool ContainsReplacementCharacter(const wxString &text) {
  return text.Find(wxUniChar(0xFFFD)) != wxNOT_FOUND;
}

// Builds a wxString from explicit Unicode code points.
wxString ChineseFixture(std::initializer_list<unsigned int> codePoints) {
  wxString text;
  for (const unsigned int codePoint : codePoints)
    text += wxUniChar(codePoint);
  return text;
}

// Verifies Simplified Chinese fixture text round-trips through UTF-8.
bool CheckChineseRoundTrip(const wxString &text, const std::string &label) {
  const wxScopedCharBuffer utf8 = text.ToUTF8();
  if (!utf8)
    return Check(false, label + " did not convert to UTF-8.");
  const wxString roundTrip = wxString::FromUTF8(utf8.data(), utf8.length());
  bool ok = true;
  ok &= Check(roundTrip == text, label + " did not round-trip through UTF-8.");
  ok &= Check(!ContainsReplacementCharacter(roundTrip),
              label + " contains a replacement character.");
  return ok;
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
  const std::filesystem::path chineseCatalogPath =
      std::filesystem::u8path(localeRoot) / "zh_CN" / "LC_MESSAGES" /
      "perastage.mo";
  ok &= Check(std::filesystem::is_regular_file(chineseCatalogPath),
              "Generated Simplified Chinese catalog is missing: " +
                  chineseCatalogPath.string());

  auto &manager = localization::LocalizationManager::Get();
  const std::filesystem::path emptyLocaleRoot =
      std::filesystem::temp_directory_path() / "perastage-empty-locale-root" /
      std::to_string(wxGetProcessId());
  std::error_code localeRootEc;
  std::filesystem::remove_all(emptyLocaleRoot, localeRootEc);
  std::filesystem::create_directories(emptyLocaleRoot, localeRootEc);
  wxSetEnv("PERASTAGE_LOCALE_ROOT",
           wxString::FromUTF8(emptyLocaleRoot.string().c_str()));
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
  const auto overrideRoots = localization::ResolveLocaleRootCandidates();
  ok &= Check(overrideRoots.size() == 1 &&
                  overrideRoots.front() == std::filesystem::weakly_canonical(std::filesystem::u8path(localeRoot)),
              "PERASTAGE_LOCALE_ROOT did not act as an exclusive override.");
  wxSetEnv("PERASTAGE_LOCALE_ROOT", "");
  ok &= Check(localization::ResolveLocaleRootCandidates().size() > 1,
              "Empty PERASTAGE_LOCALE_ROOT did not restore normal locale candidates.");
  wxSetEnv("PERASTAGE_LOCALE_ROOT", wxString::FromUTF8(localeRoot));
  const auto chineseResult =
      manager.Initialize(localization::AppLanguage::SimplifiedChinese);
  ok &= Check(chineseResult.catalogFound,
              "Simplified Chinese complete catalog was not found.");
  ok &= Check(chineseResult.catalogLoaded,
              "Simplified Chinese complete catalog was not loaded.");
  ok &= Check(chineseResult.activeLanguage ==
                  localization::AppLanguage::SimplifiedChinese,
              "Simplified Chinese did not become active after catalog loading.");
  ok &= Check(_("Preferences") == ChineseFixture({0x9996, 0x9009, 0x9879}),
              "Simplified Chinese catalog did not translate Preferences to 首选项.");
  const wxString missingSentinel = "Perastage missing localization sentinel";
  ok &= Check(wxGetTranslation(missingSentinel) == missingSentinel,
              "Missing Simplified Chinese message did not fall back to source text.");
  ok &= Check(ExpectedChineseLanguageName() ==
                  ChineseFixture({0x7B80, 0x4F53, 0x4E2D, 0x6587}),
              "Simplified Chinese native language name code points regressed.");
  ok &= CheckChineseRoundTrip(ExpectedChineseLanguageName(),
                              "Simplified Chinese native language name");
  ok &= CheckChineseRoundTrip(ChineseFixture({0x9996, 0x9009, 0x9879}),
                              "Simplified Chinese Preferences fixture");
  ok &= CheckChineseRoundTrip(ChineseFixture({0x8BED, 0x8A00}),
                              "Simplified Chinese Language fixture");
  ok &= CheckChineseRoundTrip(ChineseFixture({0x8BBE, 0x5907}),
                              "Simplified Chinese Equipment fixture");
  ok &= CheckChineseRoundTrip(ChineseFixture({0x6841, 0x67B6}),
                              "Simplified Chinese Truss fixture");
  ok &= CheckChineseRoundTrip(
      ChineseFixture({0x573A, 0x666F, 0x5BF9, 0x8C61}),
      "Simplified Chinese scene object fixture");

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

  ok &= Check(_("Weight (kg)") == wxString("Peso (kg)"),
              "Weight (kg) did not translate to Peso (kg).");
  ok &= Check(_("Hoist ID") == wxString("ID del motor"),
              "Hoist ID did not translate to ID del motor.");
  ok &= Check(_("Chain Length") == wxString("Longitud de cadena"),
              "Chain Length did not translate to Longitud de cadena.");
  ok &= Check(_("Visible") == wxString("Visible"),
              "Visible did not stay localized as Visible.");
  ok &= Check(_("Count") == wxString("Cantidad"),
              "Count did not translate to Cantidad.");
  ok &= Check(_("Position") == wxString::FromUTF8("Posición"),
              "Position did not translate to Posición.");
  ok &= Check(_("Fixture Weight") == wxString("Peso de aparatos"),
              "Fixture Weight did not translate to Peso de aparatos.");
  ok &= Check(_("Console commands") == wxString("Comandos de consola"),
              "Console commands did not translate to Comandos de consola.");
  ok &= Check(_("Create scene from text") == wxString("Crear escena desde texto"),
              "Create scene from text did not translate correctly.");
  ok &= Check(_("Search GDTF") == wxString("Buscar GDTF"),
              "Search GDTF did not translate to Buscar GDTF.");
  ok &= Check(_("Manufacturer:") == wxString("Fabricante:"),
              "Manufacturer: did not translate to Fabricante:.");
  ok &= Check(_("Download") == wxString("Descargar"),
              "Download did not translate to Descargar.");
  ok &= Check(_("Enter new layer name:") ==
                  wxString::FromUTF8("Introduzca el nombre de la nueva capa:"),
              "Layer prompt did not translate correctly.");

  ok &= Check(ui::LocalizedLabelWithUnit(_("Weight"), "kg") ==
                  wxString("Peso (kg)"),
              "Spanish metric fixture weight label regressed.");
  ok &= Check(ui::LocalizedLabelWithUnit(_("Weight"), "lb") ==
                  wxString("Peso (lb)"),
              "Spanish imperial fixture weight label regressed.");
  ok &= Check(ui::LocalizedLabelWithUnit(_("Length"), "m") ==
                  wxString("Longitud (m)"),
              "Spanish metric truss length label regressed.");
  ok &= Check(ui::LocalizedLabelWithUnit(_("Length"), "ft") ==
                  wxString("Longitud (ft)"),
              "Spanish imperial truss length label regressed.");
  ok &= Check(ui::LocalizedLabelWithUnit(_("Capacity"), "kg") ==
                  wxString("Capacidad (kg)"),
              "Spanish metric hoist capacity label regressed.");
  ok &= Check(ui::LocalizedLabelWithUnit(_("Capacity"), "lb") ==
                  wxString("Capacidad (lb)"),
              "Spanish imperial hoist capacity label regressed.");
  const wxString riggingWeight =
      ui::LocalizedLabelWithUnit(_("Fixture Weight"), "kg");
  ok &= Check(riggingWeight == wxString("Peso de aparatos (kg)"),
              "Spanish rigging dynamic weight header regressed.");

  const auto repeatedEnglish = manager.Initialize(localization::AppLanguage::English);
  ok &= Check(repeatedEnglish.activeLanguage == localization::AppLanguage::English,
              "Repeated language change did not reset the active catalog to English.");
  wxUnsetEnv("PERASTAGE_LOCALE_ROOT");
  std::filesystem::remove_all(emptyLocaleRoot, localeRootEc);
  return ok ? 0 : 1;
}
