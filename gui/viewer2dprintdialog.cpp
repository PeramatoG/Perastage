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
    : wxDialog(parent, wxID_ANY, "Print Setup", wxDefaultPosition,
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

  pageSizeA3Radio->SetValue(settings.pageSize == print::PageSize::A3);
  pageSizeA4Radio->SetValue(settings.pageSize == print::PageSize::A4);
  if (showOrientation_) {
    landscapeRadio->SetValue(settings.landscape);
    portraitRadio->SetValue(!settings.landscape);
  }

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
  settings.includeGrid = true;
  settings.detailedFootprints = false;
  return settings;
}
