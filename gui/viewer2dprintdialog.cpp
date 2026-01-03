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
#include "viewer2dprintdialog.h"

Viewer2DPrintDialog::Viewer2DPrintDialog(
    wxWindow *parent, const print::Viewer2DPrintSettings &settings,
    bool showOrientation)
    : wxDialog(parent, wxID_ANY, "Print Viewer 2D", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      showOrientation_(showOrientation),
      initialLandscape_(settings.landscape) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticBoxSizer *pageSizeSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Page size");
  pageSizeA3Radio =
      new wxRadioButton(this, wxID_ANY, "A3", wxDefaultPosition,
                        wxDefaultSize, wxRB_GROUP);
  pageSizeA4Radio = new wxRadioButton(this, wxID_ANY, "A4");
  pageSizeSizer->Add(pageSizeA3Radio, 0, wxALL, 5);
  pageSizeSizer->Add(pageSizeA4Radio, 0, wxALL, 5);
  topSizer->Add(pageSizeSizer, 0, wxEXPAND | wxALL, 10);

  if (showOrientation_) {
    wxStaticBoxSizer *orientationSizer =
        new wxStaticBoxSizer(wxVERTICAL, this, "Orientation");
    portraitRadio = new wxRadioButton(this, wxID_ANY, "Portrait",
                                      wxDefaultPosition, wxDefaultSize,
                                      wxRB_GROUP);
    landscapeRadio = new wxRadioButton(this, wxID_ANY, "Landscape");
    orientationSizer->Add(portraitRadio, 0, wxALL, 5);
    orientationSizer->Add(landscapeRadio, 0, wxALL, 5);
    topSizer->Add(orientationSizer, 0,
                  wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  }

  includeGridCheck = new wxCheckBox(this, wxID_ANY, "Include grid");
  topSizer->Add(includeGridCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  wxStaticBoxSizer *elementsSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Elements detail");
  detailedRadio = new wxRadioButton(this, wxID_ANY, "Detailed",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxRB_GROUP);
  schematicRadio = new wxRadioButton(this, wxID_ANY, "Schematic");
  elementsSizer->Add(detailedRadio, 0, wxALL, 5);
  elementsSizer->Add(schematicRadio, 0, wxALL, 5);
  topSizer->Add(elementsSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  pageSizeA3Radio->SetValue(settings.pageSize == print::PageSize::A3);
  pageSizeA4Radio->SetValue(settings.pageSize == print::PageSize::A4);
  if (showOrientation_) {
    landscapeRadio->SetValue(settings.landscape);
    portraitRadio->SetValue(!settings.landscape);
  }
  includeGridCheck->SetValue(settings.includeGrid);
  detailedRadio->SetValue(settings.detailedFootprints);
  schematicRadio->SetValue(!settings.detailedFootprints);

  detailedRadio->Bind(wxEVT_RADIOBUTTON,
                      [this](wxCommandEvent &event) {
                        if (event.IsChecked())
                          ShowDetailedWarning();
                      });

  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxALL | wxEXPAND, 10);
  SetSizerAndFit(topSizer);
}

print::Viewer2DPrintSettings Viewer2DPrintDialog::GetSettings() const {
  print::Viewer2DPrintSettings settings;
  settings.pageSize = pageSizeA4Radio->GetValue() ? print::PageSize::A4
                                                  : print::PageSize::A3;
  settings.landscape =
      showOrientation_ && landscapeRadio ? landscapeRadio->GetValue()
                                         : initialLandscape_;
  settings.includeGrid = includeGridCheck->GetValue();
  settings.detailedFootprints = detailedRadio->GetValue();
  return settings;
}

void Viewer2DPrintDialog::ShowDetailedWarning() {
  wxMessageBox(
      "El modo Detailed tarda mucho más y genera archivos más pesados.\n"
      "De momento solo lo mantengo para pruebas.",
      "Print Viewer 2D", wxOK | wxICON_WARNING, this);
}
