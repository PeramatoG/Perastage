#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/busyinfo.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

#include <memory>
#include <string>

#include "consolepanel.h"
#include "mvrimporter.h"
#include "projectutils.h"

bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  const wxString filePath = wxString::FromUTF8(pathUtf8);
  auto setImportStatus = [this](const wxString &message) {
    if (!owner_.GetStatusBar())
      return;
    owner_.SetStatusText(message, 0);
    owner_.GetStatusBar()->Update();
  };

  setImportStatus("MVR import: preparing...");

  std::unique_ptr<wxWindowDisabler> importDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> importOverlay =
      std::make_unique<wxBusyInfo>("Importing MVR file...");
  wxYieldIfNeeded();

  owner_.LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(
      pathUtf8, true, true, [&](const std::string &stage) {
        if (stage == "Conflict dialog:show") {
          importOverlay.reset();
          importDisabler.reset();
          return;
        }

        if (stage == "Conflict dialog:hide") {
          importDisabler = std::make_unique<wxWindowDisabler>();
          importOverlay = std::make_unique<wxBusyInfo>("Importing MVR file...");
          wxYieldIfNeeded();
          return;
        }

        setImportStatus("MVR import: " + wxString::FromUTF8(stage));
      });
  owner_.UnlockViewportInteraction();

  if (!imported) {
    importOverlay.reset();
    importDisabler.reset();
    if (owner_.GetStatusBar())
      owner_.SetStatusText("MVR import failed.", 0);
    wxMessageBox("Failed to import MVR file.", "Error", wxICON_ERROR);
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Failed to import " + filePath);
    return false;
  }

  setImportStatus("MVR import: refreshing panels...");
  if (owner_.consolePanel)
    owner_.consolePanel->AppendMessage("Imported " + filePath);
  owner_.RefreshAfterSceneChange();

  importOverlay.reset();
  importDisabler.reset();

  if (owner_.GetStatusBar()) {
    const wxString fileName = wxFileName(filePath).GetFullName();
    owner_.SetStatusText("MVR imported: " + fileName, 0);
  }
  return true;
}

void MainWindowIoController::OnImportMVR(wxCommandEvent &) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog openFileDialog(&owner_, "Import MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (openFileDialog.ShowModal() == wxID_CANCEL)
    return;

  const wxString filePath = openFileDialog.GetPath();
  const std::string pathUtf8 = filePath.ToUTF8().data();
  (void)ImportMvrFromPath(pathUtf8);
}

bool MainWindowIoController::OpenPathFromCommandLine(
    const std::string &pathUtf8) {
  std::string extension = wxFileName(wxString::FromUTF8(pathUtf8)).GetExt()
                              .Lower()
                              .ToStdString();

  if (extension == "psproj") {
    if (!owner_.ConfirmSaveIfDirty("loading a project", "Open Project"))
      return false;

    if (!owner_.LoadProjectFromPath(pathUtf8)) {
      wxMessageBox("Failed to load project.", "Error", wxICON_ERROR);
      if (owner_.GetStatusBar())
        owner_.SetStatusText("Project load failed.", 0);
      if (owner_.consolePanel) {
        owner_.consolePanel->AppendMessage("Failed to load " +
                                           wxString::FromUTF8(pathUtf8));
      }
      return false;
    }

    if (owner_.GetStatusBar()) {
      wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
      owner_.SetStatusText("Project loaded: " + fileInfo.GetFullName(), 0);
    }
    return true;
  }

  if (extension == "mvr") {
    if (!owner_.ConfirmSaveIfDirty("importing an MVR file", "Import MVR"))
      return false;
    return ImportMvrFromPath(pathUtf8);
  }

  if (owner_.GetStatusBar())
    owner_.SetStatusText("Unsupported startup file format.", 0);
  if (owner_.consolePanel) {
    owner_.consolePanel->AppendMessage("Unsupported startup file: " +
                                       wxString::FromUTF8(pathUtf8));
  }
  wxMessageBox("Unsupported startup file. Use .psproj or .mvr files.",
               "Unsupported file", wxICON_WARNING);
  return false;
}
