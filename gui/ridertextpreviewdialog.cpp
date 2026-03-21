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
#include "ridertextpreviewdialog.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

RiderTextPreviewDialog::RiderTextPreviewDialog(wxWindow *parent,
                                               const wxString &previewText)
    : wxDialog(parent, wxID_ANY, "Filtered rider preview", wxDefaultPosition,
               wxSize(720, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  mainSizer->Add(new wxStaticText(this, wxID_ANY,
                                  "Only the lines below will be used to "
                                  "create fixtures in the scene."),
                 0, wxEXPAND | wxALL, 8);

  previewCtrl =
      new wxTextCtrl(this, wxID_ANY, previewText, wxDefaultPosition,
                     wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  previewCtrl->SetMinSize(wxSize(680, 380));
  mainSizer->Add(previewCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  wxButton *usePreviewButton =
      new wxButton(this, wxID_OK, "Use preview in editor");
  wxButton *closeButton = new wxButton(this, wxID_CANCEL, "Close");
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(usePreviewButton, 0, wxRIGHT, 8);
  buttonSizer->Add(closeButton, 0);
  mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(mainSizer);
  Layout();
  Centre();
}

wxString RiderTextPreviewDialog::GetPreviewText() const {
  return previewCtrl ? previewCtrl->GetValue() : wxString();
}
