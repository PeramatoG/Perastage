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

#include <wx/dialog.h>

class wxTextCtrl;
class wxStaticText;

class RiderTextDialog : public wxDialog {
public:
  explicit RiderTextDialog(wxWindow *parent,
                           const wxString &initialText = wxEmptyString,
                           const wxString &initialSource = wxEmptyString);

private:
  void OnLoadFromFile(wxCommandEvent &event);
  void OnLoadExample(wxCommandEvent &event);
  void OnApply(wxCommandEvent &event);

  wxTextCtrl *textCtrl = nullptr;
  wxStaticText *sourceText = nullptr;
  wxString sourceLabel;

  wxDECLARE_EVENT_TABLE();
};
