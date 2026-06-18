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
#include "addtrussdialog.h"

#include "configmanager.h"
#include "guiconfigservices.h"
#include "ui_unit_utils.h"
#include "units/unit_label_utils.h"

#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace {

// Resolves the active project distance unit for add-truss coordinates.
Units::DistanceUnitSystem ResolveDistanceUnitSystem() {
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  return UiUnitUtils::ParseDistanceUnitSystem(
      cfg.GetValue("ui_distance_unit_system"));
}

// Builds a coordinate label with the active distance unit suffix.
wxString CoordinateLabel(const char *axis, Units::DistanceUnitSystem unitSystem) {
  const std::string label = Units::LabelWithUnit(
      std::string("Insertion point ") + axis,
      UiUnitUtils::DistanceUnitSuffix(unitSystem));
  return wxString::FromUTF8(label + ":");
}

// Adds a signed world-coordinate editor row to the dialog grid.
wxSpinCtrlDouble *AddCoordinateRow(wxWindow *parent, wxFlexGridSizer *grid,
                                   const char *axis,
                                   Units::DistanceUnitSystem unitSystem) {
  grid->Add(new wxStaticText(parent, wxID_ANY, CoordinateLabel(axis, unitSystem)),
            0, wxALIGN_CENTER_VERTICAL);
  auto *ctrl = new wxSpinCtrlDouble(parent, wxID_ANY);
  ctrl->SetRange(
      UiUnitUtils::DistanceMillimetersToDisplay(-1000000.0, unitSystem),
      UiUnitUtils::DistanceMillimetersToDisplay(1000000.0, unitSystem));
  ctrl->SetIncrement(0.1);
  ctrl->SetDigits(2);
  ctrl->SetValue(0.0);
  grid->Add(ctrl, 1, wxEXPAND);
  return ctrl;
}

} // namespace

// Creates the add-truss options dialog.
AddTrussDialog::AddTrussDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Add Truss", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  const auto unitSystem = ResolveDistanceUnitSystem();

  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(6, 2, 8, 8);
  grid->AddGrowableCol(1, 1);

  grid->Add(new wxStaticText(this, wxID_ANY, "Quantity:"), 0,
            wxALIGN_CENTER_VERTICAL);
  quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
  quantityCtrl_->SetRange(1, 1000);
  quantityCtrl_->SetValue(1);
  grid->Add(quantityCtrl_, 1, wxEXPAND);

  xCtrl_ = AddCoordinateRow(this, grid, "X", unitSystem);
  yCtrl_ = AddCoordinateRow(this, grid, "Y", unitSystem);
  zCtrl_ = AddCoordinateRow(this, grid, "Z", unitSystem);

  grid->AddSpacer(1);
  createGroupCtrl_ = new wxCheckBox(this, wxID_ANY, "Create group");
  createGroupCtrl_->SetValue(true);
  grid->Add(createGroupCtrl_, 1, wxEXPAND);

  grid->AddSpacer(1);
  continuousPlacementCtrl_ =
      new wxCheckBox(this, wxID_ANY, "Place continuously in the viewer");
  grid->Add(continuousPlacementCtrl_, 1, wxEXPAND);
  continuousPlacementCtrl_->Bind(
      wxEVT_CHECKBOX, &AddTrussDialog::OnContinuousPlacementChanged, this);

  root->Add(grid, 1, wxALL | wxEXPAND, 12);
  root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
            wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
  SetSizerAndFit(root);
  CentreOnParent();
}

// Returns the requested truss quantity, insertion point, and grouping mode.
AddTrussRequest AddTrussDialog::GetRequest() const {
  const auto unitSystem = ResolveDistanceUnitSystem();
  AddTrussRequest request;
  request.quantity = quantityCtrl_ ? quantityCtrl_->GetValue() : 1;
  request.insertionPointMm = {
      static_cast<float>(UiUnitUtils::DistanceDisplayToMillimeters(
          xCtrl_ ? xCtrl_->GetValue() : 0.0, unitSystem)),
      static_cast<float>(UiUnitUtils::DistanceDisplayToMillimeters(
          yCtrl_ ? yCtrl_->GetValue() : 0.0, unitSystem)),
      static_cast<float>(UiUnitUtils::DistanceDisplayToMillimeters(
          zCtrl_ ? zCtrl_->GetValue() : 0.0, unitSystem))};
  request.createGroup = createGroupCtrl_ ? createGroupCtrl_->GetValue() : true;
  request.continuousPlacement =
      continuousPlacementCtrl_ && continuousPlacementCtrl_->GetValue();
  return request;
}

// Disables fixed-count options while continuous placement is selected.
void AddTrussDialog::OnContinuousPlacementChanged(wxCommandEvent &event) {
  const bool enabled = !event.IsChecked();
  if (quantityCtrl_)
    quantityCtrl_->Enable(enabled);
  if (createGroupCtrl_)
    createGroupCtrl_->Enable(enabled);
}
