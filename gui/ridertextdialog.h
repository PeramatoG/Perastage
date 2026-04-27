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

#include <string>
#include <vector>

#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/event.h>

class wxTextCtrl;
class wxStaticText;
class wxListBox;
class wxPopupTransientWindow;

#include "ridertext_autocomplete_provider.h"

class RiderTextDialog : public wxDialog {
public:
  explicit RiderTextDialog(wxWindow *parent,
                           const wxString &initialText = wxEmptyString,
                           const wxString &initialSource = wxEmptyString);
  const std::string &GetRiderTextUtf8() const;
  wxString GetLoadedFileTitle() const;

private:
  bool TryGetCurrentText(std::string &outText) const;
  void OnLoadFromFile(wxCommandEvent &event);
  void OnLoadExample(wxCommandEvent &event);
  void OnApplyFilter(wxCommandEvent &event);
  void OnApply(wxCommandEvent &event);
  void OnTextChanged(wxCommandEvent &event);
  void OnTextKeyDown(wxKeyEvent &event);
  void OnTextMouseDown(wxMouseEvent &event);
  void OnAutocompleteTimer(wxTimerEvent &event);
  void OnSuggestionClick(wxCommandEvent &event);
  void OnDialogCharHook(wxKeyEvent &event);
  void RefreshAutocompleteSuggestions();
  void HideSuggestionPopup();
  bool AcceptCurrentSuggestion();
  bool IsSuggestionPopupVisible() const;
  void ReplaceCurrentToken(const std::string &replacement);
  static bool IsTokenDelimiter(wxUniChar c);

  wxTextCtrl *textCtrl = nullptr;
  wxStaticText *sourceText = nullptr;
  wxPopupTransientWindow *suggestionPopup = nullptr;
  wxListBox *suggestionList = nullptr;
  wxString sourceLabel;
  bool sourceLoadedFromFile = false;
  std::string selectedRiderTextUtf8;
  RiderTextAutocompleteProvider autocompleteProvider;
  std::vector<RiderTextAutocompleteProvider::Suggestion> currentSuggestions;
  wxTimer autocompleteTimer;
  bool suppressAutocompleteTextEvent = false;

  wxDECLARE_EVENT_TABLE();
};
