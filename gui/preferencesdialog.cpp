/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "preferencesdialog.h"
#include "localization/localization_manager.h"
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/treectrl.h>
#include <wx/treebook.h>
#include "preferences/rider_import_preferences_page.h"
#include "preferences/units_preferences_page.h"
#include "preferences/language_preferences_page.h"
#include "preferences/updates_preferences_page.h"
#include "preferences/gdtf_preferences_page.h"
#include "preferences/mvr_preferences_page.h"
#include "preferences/selection_movement_preferences_page.h"
#include "preferences/viewer3d_preferences_page.h"

wxDEFINE_EVENT(EVT_UI_UNITS_CHANGED, wxCommandEvent);
wxDEFINE_EVENT(EVT_UI_PREFERENCES_APPLIED, wxCommandEvent);

PreferencesDialog::PreferencesDialog(
    wxWindow *parent, IGuiPreferencesService &preferences,
    std::function<bool(const std::function<void()> &)> commit)
    : wxDialog(parent, wxID_ANY, _("Preferences"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      preferences(preferences), commitPreferences(std::move(commit)) {
  auto *topSizer = new wxBoxSizer(wxVERTICAL);
  auto *book = new wxTreebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxBK_LEFT);
  auto addPage = [&](PreferencesPage *page, const wxString &label) {
    page->LoadPreferences(preferences);
    book->AddSubPage(page, label);
    pages.push_back(page);
  };

  book->AddPage(nullptr, _("General"));
  languagePage = new LanguagePreferencesPage(book);
  addPage(languagePage, _("Language"));
  unitsPage = new UnitsPreferencesPage(book);
  addPage(unitsPage, _("Units"));
  addPage(new UpdatesPreferencesPage(book), _("Updates"));

  book->AddPage(nullptr, _("Import"));
  auto *riderPage = new RiderImportPreferencesPage(book);
  addPage(riderPage, _("Rider Import"));

  book->AddPage(nullptr, _("Viewer"));
  addPage(new Viewer3DPreferencesPage(book), _("3D Viewer"));
  addPage(new SelectionMovementPreferencesPage(book), _("Selection & Movement"));

  book->AddPage(nullptr, _("Formats"));
  addPage(new GdtfPreferencesPage(book), _("GDTF"));
  addPage(new MvrPreferencesPage(book), _("MVR Import / Export"));

  unitsPage->BindDistanceChanged([riderPage](Units::DistanceUnitSystem unit) {
    riderPage->SetDistanceUnit(unit);
  });
  lastRestartNoticeLanguage =
      localization::LocalizationManager::Get().ActiveLanguage();
  book->GetTreeCtrl()->ExpandAll();
  book->GetTreeCtrl()->SetMinSize(wxSize(book->GetTreeCtrl()->GetBestSize().x,
                                       FromDIP(160)));
  book->SetSelection(1);
  topSizer->Add(book, 1, wxEXPAND | wxALL, FromDIP(8));
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL | wxAPPLY), 0,
                wxALL | wxEXPAND, FromDIP(8));
  SetSizer(topSizer);
  SetMinSize(FromDIP(wxSize(760, 540)));
  SetSize(FromDIP(wxSize(940, 680)));
  Layout();
  CentreOnParent();
  Bind(wxEVT_BUTTON, &PreferencesDialog::OnApplyButton, this, wxID_APPLY);
  Bind(wxEVT_BUTTON, &PreferencesDialog::OnOkButton, this, wxID_OK);
}

// Applies the dialog changes without closing when requested.
void PreferencesDialog::OnApplyButton(wxCommandEvent &WXUNUSED(event)) {
  if (!ApplyPreferences())
    return;
  NotifyUnitsChanged();
  NotifyPreferencesApplied();
}

// Applies the dialog changes and closes the dialog on success.
void PreferencesDialog::OnOkButton(wxCommandEvent &WXUNUSED(event)) {
  if (!ApplyPreferences())
    return;
  NotifyUnitsChanged();
  NotifyPreferencesApplied();
  EndModal(wxID_OK);
}

bool PreferencesDialog::ApplyPreferences() {
  const bool saved = commitPreferences([this] {
    for (auto *page : pages)
      page->ApplyPreferences(preferences);
  });
  if (saved)
    ShowLanguageRestartNoticeIfNeeded(languagePage->SelectedLanguage());
  return saved;
}

void PreferencesDialog::NotifyUnitsChanged() {
  if (!unitsPage->UnitsChanged())
    return;
  unitsPage->MarkApplied();
  wxCommandEvent event(EVT_UI_UNITS_CHANGED);
  wxPostEvent(GetParent(), event);
}

// Notifies the main window that preferences were applied successfully.
void PreferencesDialog::NotifyPreferencesApplied() {
  wxCommandEvent event(EVT_UI_PREFERENCES_APPLIED);
  wxPostEvent(GetParent(), event);
}

// Shows the restart-required language notification once for each selected
// change.
void PreferencesDialog::ShowLanguageRestartNoticeIfNeeded(
    localization::AppLanguage selectedLanguage) {
  const localization::AppLanguage activeLanguage =
      localization::LocalizationManager::Get().ActiveLanguage();
  if (selectedLanguage == activeLanguage ||
      selectedLanguage == lastRestartNoticeLanguage)
    return;
  lastRestartNoticeLanguage = selectedLanguage;
  wxMessageBox(
      _("Language changes will be applied after restarting Perastage."),
               _("Restart required"), wxOK | wxICON_INFORMATION, this);
}
