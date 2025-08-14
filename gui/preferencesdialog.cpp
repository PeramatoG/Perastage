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

#include <wx/notebook.h>
#include <wx/checkbox.h>
#include <wx/radiobut.h>

PreferencesDialog::PreferencesDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition,
               wxDefaultSize) {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxNotebook *book = new wxNotebook(this, wxID_ANY);

  // Rider Import page
  wxPanel *riderPanel = new wxPanel(book);
  wxBoxSizer *riderSizer = new wxBoxSizer(wxVERTICAL);
  ConfigManager &cfg = ConfigManager::Get();
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

  topSizer->Add(book, 1, wxEXPAND | wxALL, 5);
  topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxALL | wxEXPAND, 5);

  SetSizerAndFit(topSizer);

  Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
    if (evt.GetId() == wxID_OK) {
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
    }
    evt.Skip();
  });
}
