#include "preferences/gdtf_mutation_policy_panel.h"

#include "gdtf_mutation_policy.h"
#include "guiconfigservices.h"

#include <wx/choice.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

GdtfMutationPolicyPanel::GdtfMutationPolicyPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *sizer = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(1, 2, 8, 10);
  grid->AddGrowableCol(1, 1);
  grid->Add(new wxStaticText(this, wxID_ANY, _("GDTF definition completion:")),
            0, wxALIGN_CENTER_VERTICAL);
  policyChoice = new wxChoice(this, wxID_ANY);
  policyChoice->Append(_("Complete and improve GDTF definitions (recommended)"));
  policyChoice->Append(_("Preserve imported GDTF definitions"));
  grid->Add(policyChoice, 1, wxEXPAND);
  sizer->Add(grid, 0, wxALL | wxEXPAND, 10);

  auto *hint = new wxStaticText(
      this, wxID_ANY,
      _("Adds missing standard SVG views to a derived @Perastage GDTF when "
        "enabled. Existing resources require explicit replacement or repair.\n"
        "Preserving imported definitions disables automatic completion. "
        "Project symbols, scene editing, and explicit GDTF edits remain "
        "available."));
  hint->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  hint->Wrap(740);
  sizer->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  SetSizer(sizer);
}

void GdtfMutationPolicyPanel::LoadPreferences(
    const IGuiPreferencesService &preferences) {
  policyChoice->SetSelection(
      gdtf::ReadMutationPolicy(preferences) == gdtf::MutationPolicy::PreserveImported
          ? 1
          : 0);
}

void GdtfMutationPolicyPanel::ApplyPreferences(
    IGuiPreferencesService &preferences) const {
  gdtf::SaveMutationPolicy(
      preferences, policyChoice->GetSelection() == 1
                       ? gdtf::MutationPolicy::PreserveImported
                       : gdtf::MutationPolicy::CompleteAndImprove);
}
