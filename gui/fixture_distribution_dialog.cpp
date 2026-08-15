#include "fixture_distribution_dialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

// Builds the fixture distribution workflow and its exact-spacing controls.
FixtureDistributionDialog::FixtureDistributionDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _("Distribute fixtures"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY, _("Distribution system:")), 0,
            wxLEFT | wxRIGHT | wxTOP, 12);
  modeChoice_ = new wxChoice(this, wxID_ANY);
  modeChoice_->Append(_("Exact spacing"));
  modeChoice_->Append(_("Uniformly over the full truss"));
  modeChoice_->Append(_("Uniformly between two points"));
  modeChoice_->SetSelection(0);
  root->Add(modeChoice_, 0, wxEXPAND | wxALL, 12);

  auto *spacingRow = new wxBoxSizer(wxHORIZONTAL);
  spacingRow->Add(new wxStaticText(this, wxID_ANY, _("Spacing:")), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  spacing_ = new wxSpinCtrlDouble(this, wxID_ANY);
  spacing_->SetRange(0.0, 1000.0);
  spacing_->SetDigits(3);
  spacing_->SetIncrement(0.05);
  spacing_->SetValue(0.5);
  spacingRow->Add(spacing_, 1);
  spacingRow->Add(new wxStaticText(this, wxID_ANY, _(" m")), 0,
                  wxALIGN_CENTER_VERTICAL);
  root->Add(spacingRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

  const wxString references[] = {_("Fixture centers"), _("Fixture edges")};
  reference_ =
      new wxRadioBox(this, wxID_ANY, _("Spacing reference"), wxDefaultPosition,
                     wxDefaultSize, 2, references, 1, wxRA_SPECIFY_ROWS);
  root->Add(reference_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  const wxString origins[] = {_("Between two points, outside inward"),
                              _("From one point in the chosen direction")};
  origin_ = new wxRadioBox(this, wxID_ANY, _("Placement"), wxDefaultPosition,
                           wxDefaultSize, 2, origins, 1, wxRA_SPECIFY_ROWS);
  root->Add(origin_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL,
            12);
  SetSizerAndFit(root);
  SetMinSize(GetSize());
  modeChoice_->Bind(wxEVT_CHOICE,
                    [this](wxCommandEvent &) { UpdateOptionAvailability(); });
  UpdateOptionAvailability();
}

// Returns the distribution choices currently selected by the user.
FixtureDistributionDialogOptions FixtureDistributionDialog::GetOptions() const {
  FixtureDistributionDialogOptions options;
  options.mode =
      static_cast<FixtureDistributionMode>(modeChoice_->GetSelection());
  options.spacingMeters = spacing_->GetValue();
  options.edgeToEdge = reference_->GetSelection() == 1;
  options.fromPoint = origin_->GetSelection() == 1;
  return options;
}

// Enables exact-spacing controls only for the exact-spacing system.
void FixtureDistributionDialog::UpdateOptionAvailability() {
  const bool enabled = modeChoice_->GetSelection() == 0;
  spacing_->Enable(enabled);
  reference_->Enable(enabled);
  origin_->Enable(enabled);
}
