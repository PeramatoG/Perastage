#include "addsceneobjectdialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

// Creates the add-scene-object options dialog.
AddSceneObjectDialog::AddSceneObjectDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Add Scene Object", wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 2, 8, 8);
  grid->AddGrowableCol(1, 1);

  grid->Add(new wxStaticText(this, wxID_ANY, "Quantity:"), 0,
            wxALIGN_CENTER_VERTICAL);
  quantityCtrl_ = new wxSpinCtrl(this, wxID_ANY);
  quantityCtrl_->SetRange(1, 1000);
  quantityCtrl_->SetValue(1);
  grid->Add(quantityCtrl_, 1, wxEXPAND);

  grid->AddSpacer(1);
  continuousPlacementCtrl_ =
      new wxCheckBox(this, wxID_ANY, "Place continuously in the viewer");
  grid->Add(continuousPlacementCtrl_, 1, wxEXPAND);
  continuousPlacementCtrl_->Bind(
      wxEVT_CHECKBOX, &AddSceneObjectDialog::OnContinuousPlacementChanged,
      this);

  root->Add(grid, 1, wxALL | wxEXPAND, 12);
  root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
            wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
  SetSizerAndFit(root);
  CentreOnParent();
}

// Returns the requested object quantity and placement mode.
AddSceneObjectRequest AddSceneObjectDialog::GetRequest() const {
  AddSceneObjectRequest request;
  request.quantity = quantityCtrl_ ? quantityCtrl_->GetValue() : 1;
  request.continuousPlacement =
      continuousPlacementCtrl_ && continuousPlacementCtrl_->GetValue();
  return request;
}

// Disables fixed quantity while continuous placement is selected.
void AddSceneObjectDialog::OnContinuousPlacementChanged(wxCommandEvent &event) {
  if (quantityCtrl_)
    quantityCtrl_->Enable(!event.IsChecked());
}
