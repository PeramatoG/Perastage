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
#include "configmanager.h"
#include "print/PlanPrintSettings.h"

#include <wx/notebook.h>
#include <wx/checkbox.h>
#include <wx/radiobut.h>

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition,
               wxDefaultSize) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxNotebook *book = new wxNotebook(this, wxID_ANY);

  ConfigManager &cfg = ConfigManager::Get();

  // Rider Import page
  wxPanel *riderPanel = new wxPanel(book);
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  autopatchCheck = new wxCheckBox(riderPanel, wxID_ANY,
                                  "Auto patch after import");
  auto autoVal = cfg.GetValue("rider_autopatch");
  autopatchCheck->SetValue(!autoVal || *autoVal != "0");
  riderSizer->Add(autopatchCheck, 0, wxALL, 10);
  layerPosRadio =
      new wxRadioButton(riderPanel, wxID_ANY,
                        "Auto-create layers by position", wxDefaultPosition,
                        wxDefaultSize, wxRB_GROUP);
  layerTypeRadio = new wxRadioButton(riderPanel, wxID_ANY,
                                     "Auto-create layers by fixture type");
  auto modeVal = cfg.GetValue("rider_layer_mode");
  bool byType = modeVal && *modeVal == "type";
  layerTypeRadio->SetValue(byType);
  layerPosRadio->SetValue(!byType);
  riderSizer->Add(layerPosRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  riderSizer->Add(layerTypeRadio, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  wxFlexGridSizer *grid = new wxFlexGridSizer(6, 5, 5);
  grid->AddGrowableCol(1, 1);
  grid->AddGrowableCol(3, 1);
  grid->AddGrowableCol(5, 1);

  for (int i = 0; i < 6; ++i) {
    wxString labelH = wxString::Format("LX%d height (m):", i + 1);
    grid->Add(new wxStaticText(riderPanel, wxID_ANY, labelH), 0,
              wxALIGN_CENTER_VERTICAL);
    double valH = cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_height");
    lxHeightCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::Format("%.2f", valH));
    grid->Add(lxHeightCtrls[i], 1, wxEXPAND);

    wxString labelP = wxString::Format("LX%d position (m):", i + 1);
    grid->Add(new wxStaticText(riderPanel, wxID_ANY, labelP), 0,
              wxALIGN_CENTER_VERTICAL);
    double valP = cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_pos");
    lxPosCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                   wxString::Format("%.2f", valP));
    grid->Add(lxPosCtrls[i], 1, wxEXPAND);

    wxString labelM = wxString::Format("LX%d margin (m):", i + 1);
    grid->Add(new wxStaticText(riderPanel, wxID_ANY, labelM), 0,
              wxALIGN_CENTER_VERTICAL);
    double valM = cfg.GetFloat("rider_lx" + std::to_string(i + 1) + "_margin");
    lxMarginCtrls[i] = new wxTextCtrl(riderPanel, wxID_ANY,
                                      wxString::Format("%.2f", valM));
    grid->Add(lxMarginCtrls[i], 1, wxEXPAND);
  }
  riderSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
  riderPanel->SetSizer(riderSizer);

  book->AddPage(riderPanel, "Rider Import");

  // Plan printing page
  wxPanel *planPanel = new wxPanel(book);
  wxBoxSizer *planSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticBoxSizer *pageSizeSizer =
      new wxStaticBoxSizer(wxVERTICAL, planPanel, "Page size");
  pageSizeA3Radio = new wxRadioButton(planPanel, wxID_ANY, "A3", wxDefaultPosition,
                                      wxDefaultSize, wxRB_GROUP);
  pageSizeA4Radio = new wxRadioButton(planPanel, wxID_ANY, "A4");
  pageSizeSizer->Add(pageSizeA3Radio, 0, wxALL, 5);
  pageSizeSizer->Add(pageSizeA4Radio, 0, wxALL, 5);
  planSizer->Add(pageSizeSizer, 0, wxEXPAND | wxALL, 10);

  wxStaticBoxSizer *orientationSizer = new wxStaticBoxSizer(
      wxVERTICAL, planPanel, "Orientation");
  portraitRadio = new wxRadioButton(planPanel, wxID_ANY, "Portrait",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxRB_GROUP);
  landscapeRadio = new wxRadioButton(planPanel, wxID_ANY, "Landscape");
  orientationSizer->Add(portraitRadio, 0, wxALL, 5);
  orientationSizer->Add(landscapeRadio, 0, wxALL, 5);
  planSizer->Add(orientationSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                 10);

  includeGridCheck = new wxCheckBox(planPanel, wxID_ANY, "Include grid");
  planSizer->Add(includeGridCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  wxStaticBoxSizer *elementsSizer =
      new wxStaticBoxSizer(wxVERTICAL, planPanel, "Elements detail");
  detailedRadio = new wxRadioButton(planPanel, wxID_ANY, "Detailed",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxRB_GROUP);
  schematicRadio = new wxRadioButton(planPanel, wxID_ANY, "Schematic");
  elementsSizer->Add(detailedRadio, 0, wxALL, 5);
  elementsSizer->Add(schematicRadio, 0, wxALL, 5);
  planSizer->Add(elementsSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  LoadPlanPrintSettings(print::PlanPrintSettings::LoadFromConfig(cfg));

  planPanel->SetSizer(planSizer);
  book->AddPage(planPanel, "Plan Printing");

  topSizer->Add(book, 1, wxEXPAND | wxALL, 5);
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL | wxAPPLY), 0,
                wxALL | wxEXPAND, 5);

  SetSizerAndFit(topSizer);

  Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
    if (evt.GetId() == wxID_OK || evt.GetId() == wxID_APPLY) {
      ConfigManager &cfg = ConfigManager::Get();
      for (int i = 0; i < 6; ++i) {
        double v = 0.0;
        lxHeightCtrls[i]->GetValue().ToDouble(&v);
        cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_height",
                     static_cast<float>(v));
        double p = 0.0;
        lxPosCtrls[i]->GetValue().ToDouble(&p);
        cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_pos",
                     static_cast<float>(p));
        double m = 0.0;
        lxMarginCtrls[i]->GetValue().ToDouble(&m);
        cfg.SetFloat("rider_lx" + std::to_string(i + 1) + "_margin",
                     static_cast<float>(m));
      }
      cfg.SetValue("rider_autopatch", autopatchCheck->GetValue() ? "1" : "0");
      cfg.SetValue("rider_layer_mode",
                   layerTypeRadio->GetValue() ? "type" : "position");

      print::PlanPrintSettings planSettings;
      SavePlanPrintSettings(planSettings);
      planSettings.SaveToConfig(cfg);
    }
    evt.Skip();
  });
}

void PreferencesDialog::LoadPlanPrintSettings(
    const print::PlanPrintSettings &settings) {
  pageSizeA3Radio->SetValue(settings.pageSize == print::PageSize::A3);
  pageSizeA4Radio->SetValue(settings.pageSize == print::PageSize::A4);
  landscapeRadio->SetValue(settings.landscape);
  portraitRadio->SetValue(!settings.landscape);
  includeGridCheck->SetValue(settings.includeGrid);
  detailedRadio->SetValue(settings.detailedFootprints);
  schematicRadio->SetValue(!settings.detailedFootprints);
}

void PreferencesDialog::SavePlanPrintSettings(
    print::PlanPrintSettings &settings) {
  settings.pageSize = pageSizeA4Radio->GetValue() ? print::PageSize::A4
                                                  : print::PageSize::A3;
  settings.landscape = landscapeRadio->GetValue();
  settings.includeGrid = includeGridCheck->GetValue();
  settings.detailedFootprints = detailedRadio->GetValue();
}
