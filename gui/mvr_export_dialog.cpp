#include "mvr_export_dialog.h"
#include <wx/intl.h>
#include <wx/checkbox.h>

void MvrExportDialogOptions::AddCustomControls(wxFileDialogCustomize &customizer) {
  customizer.AddStaticText(_("Canonical MVR matches the scene saved in PSTG."));
  compatibility_ = customizer.AddCheckBox(_("Use compatibility export for this file"));
  compatibility_->SetValue(false);
  compatibility_->Bind(wxEVT_CHECKBOX,
                       [this](wxCommandEvent &) { UpdateCustomControls(); });
  const wxString choices[] = {_("Standard MVR representation"),
                             _("Direct Geometry3D for truss symbols")};
  representation_ = customizer.AddChoice(2, choices);
  representation_->SetSelection(
      preferred_.trussGeometryExportMode ==
              MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols ? 1 : 0);
  customizer.AddStaticText(_("Direct Geometry3D expands truss Symbol/Symdef references. "
                             "The project scene stays unchanged."));
  UpdateCustomControls();
}

void MvrExportDialogOptions::UpdateCustomControls() {
  if (compatibility_ && representation_)
    representation_->Enable(compatibility_->GetValue());
}

void MvrExportDialogOptions::TransferDataFromCustomControls() {
  auto requested = preferred_;
  if (representation_)
    requested.trussGeometryExportMode = representation_->GetSelection() == 1
        ? MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols
        : MvrTrussGeometryExportMode::Standard;
  selected_ = ResolveStandaloneMvrExportOptions(
      requested, compatibility_ && compatibility_->GetValue());
}
