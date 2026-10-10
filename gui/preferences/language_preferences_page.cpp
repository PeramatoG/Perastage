#include "preferences/language_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "localization/app_language.h"

namespace {
// Returns the native display label for a supported language option.
wxString NativeLanguageDisplayName(localization::AppLanguage language) {
  switch (language) {
  case localization::AppLanguage::Spanish: {
    wxString name("Espa");
    name += wxUniChar(0x00F1);
    name += "ol";
    return name;
  }
  case localization::AppLanguage::SimplifiedChinese: {
    wxString name;
    name += wxUniChar(0x7B80);
    name += wxUniChar(0x4F53);
    name += wxUniChar(0x4E2D);
    name += wxUniChar(0x6587);
    return name;
  }
  case localization::AppLanguage::English:
  default:
    return "English";
  }
}

} // namespace

LanguagePreferencesPage::LanguagePreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *languageSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *languageGrid = new wxFlexGridSizer(1, 2, 10, 10);
  languageGrid->AddGrowableCol(1, 1);
  languageGrid->Add(
      new wxStaticText(this, wxID_ANY, _("Interface language:")), 0,
      wxALIGN_CENTER_VERTICAL);
  interfaceLanguageChoice = new wxChoice(this, wxID_ANY);
  for (const auto &option : localization::SupportedAppLanguages()) {
    interfaceLanguageChoice->Append(NativeLanguageDisplayName(option.language));
  }
  languageGrid->Add(interfaceLanguageChoice, 1, wxEXPAND);
  languageSizer->Add(languageGrid, 0, wxALL | wxEXPAND, 10);
  wxStaticText *languageHint = new wxStaticText(
      this, wxID_ANY,
      _("Language changes will be applied after restarting Perastage."));
  languageHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  languageSizer->Add(languageHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  SetSizer(languageSizer);

  PrepareLayout();
}

void LanguagePreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  const auto configuredLanguage = localization::ParseAppLanguageCode(
      cfg.GetValue(localization::kUiLanguageConfigKey).value_or(""));
  int languageSelection = 0;
  const auto &languages = localization::SupportedAppLanguages();
  for (std::size_t i = 0; i < languages.size(); ++i) {
    if (languages[i].language == configuredLanguage) {
      languageSelection = static_cast<int>(i);
      break;
    }
  }
  interfaceLanguageChoice->SetSelection(languageSelection);
}

void LanguagePreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  cfg.SetValue(localization::kUiLanguageConfigKey,
               std::string(localization::AppLanguageCode(SelectedLanguage())));
}

localization::AppLanguage LanguagePreferencesPage::SelectedLanguage() const {
  const auto &options = localization::SupportedAppLanguages();
  const int selection = interfaceLanguageChoice->GetSelection();
  if (selection >= 0 && static_cast<std::size_t>(selection) < options.size())
    return options[static_cast<std::size_t>(selection)].language;
  return localization::DefaultAppLanguage();
}
