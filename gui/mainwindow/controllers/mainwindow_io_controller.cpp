#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/busyinfo.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>

#include <algorithm>
#include <memory>
#include <string>

#include "consolepanel.h"
#include "mvrimporter.h"
#include "projectutils.h"
#include "splashscreen.h"

bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  const wxString filePath = wxString::FromUTF8(pathUtf8);
  auto setImportStatus = [this](const wxString &message) {
    if (!owner_.GetStatusBar())
      return;
    owner_.SetStatusText(message, 0);
    owner_.GetStatusBar()->Update();
  };

  setImportStatus("MVR import: preparing...");
  SplashScreen::Hide();

  std::unique_ptr<wxWindowDisabler> importDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> importOverlay =
      std::make_unique<wxBusyInfo>("Importing MVR file...");
  std::unique_ptr<wxProgressDialog> importProgress;
  wxYieldIfNeeded();

  owner_.LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(
      pathUtf8, true, true, [&](const MvrImporter::ProgressState &progress) {
        const std::string &stage = progress.stage;
        if (stage == "Conflict dialog:show") {
          importProgress.reset();
          importOverlay.reset();
          importDisabler.reset();
          return;
        }

        if (stage == "Conflict dialog:hide") {
          importDisabler = std::make_unique<wxWindowDisabler>();
          importOverlay = std::make_unique<wxBusyInfo>("Importing MVR file...");
          importProgress.reset();
          wxYieldIfNeeded();
          return;
        }

        if (progress.HasCount()) {
          const wxString title = "MVR import progress";
          const wxString stageText = wxString::FromUTF8(stage);
          const int safeTotal = std::max(progress.total, 1);
          const int clampedCompleted = std::clamp(progress.completed, 0, safeTotal);

          if (!importProgress) {
            importOverlay.reset();
            importProgress = std::make_unique<wxProgressDialog>(
                title, stageText, safeTotal, &owner_,
                wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_APP_MODAL);
          } else {
            importProgress->SetRange(safeTotal);
          }

          importProgress->Update(clampedCompleted, stageText);
          setImportStatus(wxString::Format("MVR import: %s (%d/%d)", stageText,
                                           clampedCompleted, safeTotal));
          return;
        }

        if (importProgress) {
          importProgress->Pulse(wxString::FromUTF8(stage));
        }
        setImportStatus("MVR import: " + wxString::FromUTF8(stage));
      });
  owner_.UnlockViewportInteraction();

  if (!imported) {
    importProgress.reset();
    importOverlay.reset();
    importDisabler.reset();
    if (owner_.GetStatusBar())
      owner_.SetStatusText("MVR import failed.", 0);
    wxMessageBox("Failed to import MVR file.", "Error", wxOK | wxICON_ERROR,
                 &owner_);
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Failed to import " + filePath);
    return false;
  }

  setImportStatus("MVR import: refreshing panels...");
  if (owner_.consolePanel)
    owner_.consolePanel->AppendMessage("Imported " + filePath);
  owner_.currentProjectPath.clear();
  owner_.currentProjectDisplayName = wxFileName(filePath).GetName();
  ProjectUtils::SaveLastProjectPath("");
  owner_.UpdateTitle();
  owner_.RefreshAfterSceneChange();

  importProgress.reset();
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
  (void)ImportMvrWithOfficialPolicy(pathUtf8);
}


bool MainWindowIoController::ImportMvrWithOfficialPolicy(
    const std::string &pathUtf8) {
  if (!owner_.ConfirmSaveIfDirty(kMvrOpenAction, kMvrOpenTitle))
    return false;

  // Official policy for .mvr open/import actions:
  // reset application scene state and then import the selected package.
  // MvrImporter::ImportFromFile() performs the reset via ConfigManager.
  return ImportMvrFromPath(pathUtf8);
}

bool MainWindowIoController::OpenPathFromCommandLine(
    const std::string &pathUtf8) {
  std::string extension = wxFileName(wxString::FromUTF8(pathUtf8)).GetExt()
                              .Lower()
                              .ToStdString();
  std::string projectExtension = wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION)
                                     .AfterFirst('.')
                                     .Lower()
                                     .ToStdString();

  if (extension == projectExtension) {
    if (!owner_.ConfirmSaveIfDirty("loading a project", "Open Project"))
      return false;

    if (!owner_.LoadProjectFromPath(pathUtf8)) {
      wxMessageBox("Failed to load project.", "Error", wxOK | wxICON_ERROR,
                   &owner_);
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

  if (extension == "mvr")
    return ImportMvrWithOfficialPolicy(pathUtf8);

  if (owner_.GetStatusBar())
    owner_.SetStatusText("Unsupported startup file format.", 0);
  if (owner_.consolePanel) {
    owner_.consolePanel->AppendMessage("Unsupported startup file: " +
                                       wxString::FromUTF8(pathUtf8));
  }
  wxMessageBox("Unsupported startup file. Use " +
                   wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION) +
                   " or .mvr files.",
               "Unsupported file", wxOK | wxICON_WARNING, &owner_);
  return false;
}
