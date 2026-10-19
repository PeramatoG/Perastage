/* This file is part of Perastage. */
#include "mvr_export_dialog.h"
#include <cassert>
#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>

class ExportDialogTestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(ExportDialogTestApp);

static wxChoice *FindRepresentation(wxWindow &parent) {
  for (auto *child : parent.GetChildren()) {
    if (auto *choice = wxDynamicCast(child, wxChoice))
      if (choice->GetCount() == 2 &&
          choice->GetString(1) == "Direct Geometry3D for truss symbols")
        return choice;
    if (auto *choice = FindRepresentation(*child)) return choice;
  }
  return nullptr;
}

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  {
    MvrExportOptions preferred;
    preferred.trussGeometryExportMode =
        MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
    MvrExportDialogOptions options(preferred);
    assert(options.GetOptions().trussGeometryExportMode == MvrTrussGeometryExportMode::Standard);
    wxFileDialog dialog(nullptr, "Export", {}, {}, "MVR files (*.mvr)|*.mvr",
                               wxFD_SAVE);
    assert(dialog.SetCustomizeHook(options));
    bool checked = false;
    wxTheApp->CallAfter([&] {
      auto *checkbox = wxDynamicCast(
          wxWindow::FindWindowByLabel("Use compatibility export for this file", &dialog), wxCheckBox);
      auto *choice = FindRepresentation(dialog);
      assert(checkbox && choice);
      assert(!checkbox->GetValue() && !choice->IsEnabled());
      assert(choice->GetSelection() == 1);
      options.TransferDataFromCustomControls();
      assert(options.GetOptions().trussGeometryExportMode == MvrTrussGeometryExportMode::Standard);
      checkbox->SetValue(true);
      wxCommandEvent enabled(wxEVT_CHECKBOX, checkbox->GetId());
      enabled.SetInt(1);
      checkbox->GetEventHandler()->ProcessEvent(enabled);
      assert(choice->IsEnabled());
      options.TransferDataFromCustomControls();
      assert(options.GetOptions().trussGeometryExportMode == preferred.trussGeometryExportMode);
      checkbox->SetValue(false);
      options.TransferDataFromCustomControls();
      assert(options.GetOptions().trussGeometryExportMode == MvrTrussGeometryExportMode::Standard);
      checked = true;
      dialog.EndModal(wxID_CANCEL);
    });
    assert(dialog.ShowModal() == wxID_CANCEL);
    assert(checked);
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
