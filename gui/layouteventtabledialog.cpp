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
#include "layouteventtabledialog.h"

namespace {
constexpr std::array<const char *, 7> kEventTableLabels = {
    "Venue:", "Location:", "Date:", "Stage:",
    "Version:", "Design:", "Mail:"};
}

LayoutEventTableDialog::LayoutEventTableDialog(
    wxWindow *parent, const layouts::LayoutEventTableDefinition &table)
    : wxDialog(parent, wxID_ANY, "Edit Event Table", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer *grid = new wxFlexGridSizer(2, 8, 10);
  grid->AddGrowableCol(1, 1);

  for (size_t idx = 0; idx < kEventTableLabels.size(); ++idx) {
    wxStaticText *label =
        new wxStaticText(this, wxID_ANY, kEventTableLabels[idx]);
    grid->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

    wxTextCtrl *field = new wxTextCtrl(this, wxID_ANY);
    if (idx < table.fields.size()) {
      field->SetValue(wxString::FromUTF8(table.fields[idx]));
    }
    fieldControls_[idx] = field;
    grid->Add(field, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);
  }

  mainSizer->Add(grid, 1, wxEXPAND | wxALL, 12);
  mainSizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0,
                 wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  SetSizerAndFit(mainSizer);
  CentreOnParent();
}

std::array<std::string, 7> LayoutEventTableDialog::GetFields() const {
  std::array<std::string, 7> fields{};
  for (size_t idx = 0; idx < fieldControls_.size(); ++idx) {
    if (fieldControls_[idx]) {
      wxString value = fieldControls_[idx]->GetValue();
      value.Trim(true).Trim(false);
      fields[idx] = value.ToStdString();
    }
  }
  return fields;
}
