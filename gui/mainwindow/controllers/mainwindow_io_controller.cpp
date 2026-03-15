#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/filedlg.h>
#include <wx/busyinfo.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

#include "consolepanel.h"
#include "mvrimporter.h"
#include "projectutils.h"

void MainWindowIoController::OnImportMVR(wxCommandEvent &) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog openFileDialog(&owner_, "Import MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (openFileDialog.ShowModal() == wxID_CANCEL)
    return;

  wxString filePath = openFileDialog.GetPath();
  std::string pathUtf8 = filePath.ToUTF8().data();

  if (owner_.GetStatusBar()) {
    owner_.SetStatusText("MVR import: preparing...", 0);
    owner_.GetStatusBar()->Update();
  }

  wxBusyInfo importOverlay(
      "Importing MVR file...\n"
      "\n"
      "Please wait while Perastage:\n"
      "- Extracts package resources\n"
      "- Parses scene data\n"
      "- Builds fixtures, trusses, and objects");
  wxYieldIfNeeded();

  owner_.LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(pathUtf8);
  owner_.UnlockViewportInteraction();

  if (!imported) {
    if (owner_.GetStatusBar())
      owner_.SetStatusText("MVR import failed.", 0);
    wxMessageBox("Failed to import MVR file.", "Error", wxICON_ERROR);
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Failed to import " + filePath);
  } else {
    if (owner_.GetStatusBar()) {
      owner_.SetStatusText("MVR import: refreshing panels...", 0);
      owner_.GetStatusBar()->Update();
    }
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Imported " + filePath);
    owner_.RefreshAfterSceneChange();

    if (owner_.GetStatusBar()) {
      const wxString fileName = wxFileName(filePath).GetFullName();
      owner_.SetStatusText("MVR imported: " + fileName, 0);
    }
  }
}
