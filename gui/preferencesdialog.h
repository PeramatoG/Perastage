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
#pragma once

#include "localization/app_language.h"
#include <functional>
#include <vector>
#include <wx/dialog.h>

wxDECLARE_EVENT(EVT_UI_UNITS_CHANGED, wxCommandEvent);
wxDECLARE_EVENT(EVT_UI_PREFERENCES_APPLIED, wxCommandEvent);

class IGuiPreferencesService;
class PreferencesPage;
class LanguagePreferencesPage;
class UnitsPreferencesPage;

class PreferencesDialog : public wxDialog {
public:
  explicit PreferencesDialog(wxWindow *parent);
  // The commit adapter preserves the application's existing persistence/dirty
  // state boundary and permits isolated UI regression tests.
  PreferencesDialog(wxWindow *parent, IGuiPreferencesService &preferences,
                    std::function<bool(const std::function<void()> &)> commit);

private:
  void OnApplyButton(wxCommandEvent &event);
  void OnOkButton(wxCommandEvent &event);
  void NotifyPreferencesApplied();
  bool ApplyPreferences();
  void NotifyUnitsChanged();
  void ShowLanguageRestartNoticeIfNeeded(localization::AppLanguage language);

  IGuiPreferencesService &preferences;
  std::function<bool(const std::function<void()> &)> commitPreferences;
  std::vector<PreferencesPage *> pages;
  UnitsPreferencesPage *unitsPage = nullptr;
  LanguagePreferencesPage *languagePage = nullptr;
  localization::AppLanguage lastRestartNoticeLanguage =
      localization::DefaultAppLanguage();
};
