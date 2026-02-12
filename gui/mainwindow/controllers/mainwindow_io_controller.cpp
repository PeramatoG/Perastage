#include "mainwindow/controllers/mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/filedlg.h>
#include <wx/msgdlg.h>

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
  if (!MvrImporter::ImportAndRegister(pathUtf8)) {
    wxMessageBox("Failed to import MVR file.", "Error", wxICON_ERROR);
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Failed to import " + filePath);
  } else {
    wxMessageBox("MVR file imported successfully.", "Success",
                 wxICON_INFORMATION);
    if (owner_.consolePanel)
      owner_.consolePanel->AppendMessage("Imported " + filePath);
    owner_.RefreshAfterSceneChange();
  }
}
