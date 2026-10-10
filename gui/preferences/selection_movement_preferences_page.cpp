#include "preferences/selection_movement_preferences_page.h"
#include "guiconfigservices.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include "magnet_snap.h"
#include "selection_movement_settings.h"

SelectionMovementPreferencesPage::SelectionMovementPreferencesPage(wxWindow *parent)
    : PreferencesPage(parent) {
  wxBoxSizer *selectionSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText *selectionTitle =
      new wxStaticText(this, wxID_ANY, _("Selection & Movement"));
  wxFont selectionTitleFont = selectionTitle->GetFont();
  selectionTitleFont.MakeBold();
  selectionTitleFont.SetPointSize(selectionTitleFont.GetPointSize() + 2);
  selectionTitle->SetFont(selectionTitleFont);
  selectionSizer->Add(selectionTitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                      12);
  wxStaticText *selectionSubtitle = new wxStaticText(
      this, wxID_ANY,
      _("Choose how interactive transforms handle objects inside groups."));
  selectionSubtitle->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  selectionSizer->Add(selectionSubtitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
                      6);

  wxStaticBoxSizer *groupMoveSizer = new wxStaticBoxSizer(
      wxVERTICAL, this, _("Grouped object movement"));
  groupMoveFixtureCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Fixtures move the containing group"));
  groupMoveTrussCheck = new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                                       _("Trusses move the containing group"));
  groupMoveSupportCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Supports / Hoists move the containing group"));
  groupMoveSceneObjectCheck =
      new wxCheckBox(groupMoveSizer->GetStaticBox(), wxID_ANY,
                     _("Scene Objects move the containing group"));
  for (wxCheckBox *check : {groupMoveFixtureCheck, groupMoveTrussCheck,
                            groupMoveSupportCheck, groupMoveSceneObjectCheck})
    groupMoveSizer->Add(check, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  wxStaticText *groupMoveHint = new wxStaticText(
      groupMoveSizer->GetStaticBox(), wxID_ANY,
      _("When enabled, selecting this type inside a group moves the highest "
        "parent GroupObject during mouse, CLI, and Magnet transformations.\n"
        "Table edits always modify only the edited object.\n"
        "Selecting a GroupObject directly always moves that group."));
  groupMoveHint->SetForegroundColour(
      wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  groupMoveSizer->Add(groupMoveHint, 0, wxALL, 8);
  selectionSizer->Add(groupMoveSizer, 0, wxALL | wxEXPAND, 10);
  wxStaticBoxSizer *magnetSizer = new wxStaticBoxSizer(
      wxVERTICAL, this, _("Magnet visual feedback"));
  magnetAnchorReferencesCheck = new wxCheckBox(
      magnetSizer->GetStaticBox(), wxID_ANY,
      _("Show anchor references while moving or inserting elements"));
  magnetSizer->Add(magnetAnchorReferencesCheck, 0, wxALL, 8);
  selectionSizer->Add(magnetSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                      10);
  SetSizer(selectionSizer);

  PrepareLayout();
}

void SelectionMovementPreferencesPage::LoadPreferences(const IGuiPreferencesService &cfg) {
  const auto groupMovePolicy =
      selection_movement_settings::LoadInteractiveTransformPolicy(cfg);
  groupMoveFixtureCheck->SetValue(groupMovePolicy.promoteFixturesToGroup);
  groupMoveTrussCheck->SetValue(groupMovePolicy.promoteTrussesToGroup);
  groupMoveSupportCheck->SetValue(groupMovePolicy.promoteSupportsToGroup);
  groupMoveSceneObjectCheck->SetValue(
      groupMovePolicy.promoteSceneObjectsToGroup);
  const auto magnetReferenceValue =
      cfg.GetValue(magnet_snap::kShowAnchorReferencesConfigKey);
  magnetAnchorReferencesCheck->SetValue(!magnetReferenceValue ||
                                        *magnetReferenceValue != "0");
}

void SelectionMovementPreferencesPage::ApplyPreferences(IGuiPreferencesService &cfg) {
  selection_movement_settings::SaveInteractiveTransformPolicy(
      cfg,
      {.promoteFixturesToGroup = groupMoveFixtureCheck->GetValue(),
       .promoteTrussesToGroup = groupMoveTrussCheck->GetValue(),
       .promoteSupportsToGroup = groupMoveSupportCheck->GetValue(),
       .promoteSceneObjectsToGroup = groupMoveSceneObjectCheck->GetValue()});
  cfg.SetValue(magnet_snap::kShowAnchorReferencesConfigKey,
               magnetAnchorReferencesCheck &&
                       magnetAnchorReferencesCheck->GetValue()
                   ? "1"
                   : "0");

}
