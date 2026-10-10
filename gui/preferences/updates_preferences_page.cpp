#include "preferences/updates_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "update/update_check_preferences.h"

UpdatesPreferencesPage::UpdatesPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *updatesSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *updatesGrid = new wxFlexGridSizer(1, 2, 10, 10);
  updatesGrid->AddGrowableCol(1, 1);
  updatesGrid->Add(
      new wxStaticText(this, wxID_ANY, _("Automatic update checks:")),
                   0, wxALIGN_CENTER_VERTICAL);
  updateCheckModeChoice = new wxChoice(this, wxID_ANY);
  updateCheckModeChoice->Append(_("Check on startup (recommended)"));
  updateCheckModeChoice->Append(_("Manual only"));
  updatesGrid->Add(updateCheckModeChoice, 1, wxEXPAND);
  updatesSizer->Add(updatesGrid, 0, wxALL | wxEXPAND, 10);
  updatesSizer->Add(new wxStaticText(this, wxID_ANY,
                                     _("Manual checks are always available "
                                       "from Help -> Check for Updates.")),
                    0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  SetSizer(updatesSizer);

  PrepareLayout();
}

void UpdatesPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  const auto startupMode = gui::update::ReadStartupCheckMode(
      cfg);
  if (startupMode == gui::update::StartupCheckMode::ManualOnly)
    updateCheckModeChoice->SetSelection(1);
  else
    updateCheckModeChoice->SetSelection(0);
}

void UpdatesPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  if (updateCheckModeChoice && updateCheckModeChoice->GetSelection() == 1)
    gui::update::WriteStartupCheckMode(
        cfg, gui::update::StartupCheckMode::ManualOnly);
  else
    gui::update::WriteStartupCheckMode(
        cfg, gui::update::StartupCheckMode::StartupRecommended);
}
