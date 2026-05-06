#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/busyinfo.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/weakref.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "consolepanel.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "fixture_label_overrides.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "LayoutManager.h"
#include "layoutpanel.h"
#include "mvrimporter.h"
#include "projectutils.h"
#include "sceneobjecttablepanel.h"
#include "splashscreen.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

// Imports an MVR file from disk, preserving selected UI config keys and refreshing dependent panels.
bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  wxWeakRef<MainWindow> ownerRef(&owner_);
  if (!ownerRef || ownerRef->guiConfigServices == nullptr)
    return false;
  const wxString filePath = wxString::FromUTF8(pathUtf8);
  ConfigManager &cfg =
      owner_.guiConfigServices->LegacyConfigManager();
  const std::optional<std::string> preservedLayoutsConfig =
      cfg.GetValue(kLayoutsConfigKey);
  const std::optional<std::string> preservedViewer3DRenderStyle =
      cfg.GetValue(kViewer3DRenderStyleConfigKey);
  auto setImportStatus = [ownerRef](const wxString &message) {
    if (!ownerRef || !ownerRef->GetStatusBar())
      return;
    ownerRef->SetStatusText(message, 0);
    ownerRef->GetStatusBar()->Update();
  };

  setImportStatus("MVR import: preparing...");
  SplashScreen::Hide();

  std::unique_ptr<wxWindowDisabler> importDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> importOverlay =
      std::make_unique<wxBusyInfo>("Importing MVR file...");
  std::unique_ptr<wxProgressDialog> importProgress;

  if (!ownerRef)
    return false;
  ownerRef->LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(
      pathUtf8, true, true, [&](const MvrImporter::ProgressState &progress) {
        if (!ownerRef)
          return;
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
                title, stageText, safeTotal + 1, ownerRef.get(),
                wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_APP_MODAL);
          } else {
            importProgress->SetRange(safeTotal + 1);
          }

          const int dialogProgressValue =
              std::clamp(clampedCompleted, 0, std::max(1, safeTotal));
          importProgress->Update(dialogProgressValue, stageText);
          wxYieldIfNeeded();
          setImportStatus(wxString::Format("MVR import: %s (%d/%d)", stageText,
                                           clampedCompleted, safeTotal));
          return;
        }

        if (importProgress) {
          importProgress->Pulse(wxString::FromUTF8(stage));
          wxYieldIfNeeded();
        }
        setImportStatus("MVR import: " + wxString::FromUTF8(stage));
      });
  if (ownerRef)
    ownerRef->UnlockViewportInteraction();
  if (!ownerRef)
    return false;

  if (!imported) {
    if (preservedLayoutsConfig.has_value())
      cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
    else
      cfg.RemoveKey(kLayoutsConfigKey);
    layouts::LayoutManager::Get().LoadFromConfig(cfg);

    importProgress.reset();
    importOverlay.reset();
    importDisabler.reset();
    if (ownerRef->GetStatusBar())
      ownerRef->SetStatusText("MVR import failed.", 0);
    wxMessageBox("Failed to import MVR file.", "Error", wxOK | wxICON_ERROR,
                 ownerRef.get());
    if (ownerRef->consolePanel)
      ownerRef->consolePanel->AppendMessage("Failed to import " + filePath);
    return false;
  }

  setImportStatus("MVR import: refreshing panels...");
  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);
  if (ownerRef->consolePanel)
    ownerRef->consolePanel->AppendMessage("Imported " + filePath);
  ownerRef->currentProjectPath.clear();
  ownerRef->currentProjectDisplayName = wxFileName(filePath).GetName();
  ProjectUtils::SaveLastProjectPath("");
  ownerRef->UpdateTitle();

  if (preservedLayoutsConfig.has_value())
    cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
  else
    cfg.RemoveKey(kLayoutsConfigKey);
  if (preservedViewer3DRenderStyle.has_value())
    cfg.SetValue(kViewer3DRenderStyleConfigKey, *preservedViewer3DRenderStyle);
  layouts::LayoutManager::Get().LoadFromConfig(cfg);

  if (ownerRef->layoutPanel)
    ownerRef->layoutPanel->ReloadLayouts();
  if (ownerRef->fixturePanel)
    ownerRef->fixturePanel->ReloadData();
  if (ownerRef->trussPanel)
    ownerRef->trussPanel->ReloadData();
  if (ownerRef->hoistPanel)
    ownerRef->hoistPanel->ReloadData();
  if (ownerRef->sceneObjPanel)
    ownerRef->sceneObjPanel->ReloadData();
  if (ownerRef->viewportPanel) {
    ownerRef->viewportPanel->UpdateScene();
    ownerRef->viewportPanel->Refresh();
  }
  if (ownerRef->viewport2DPanel) {
    if (!ownerRef->HasActiveLayout2DView())
      ownerRef->viewport2DPanel->LoadViewFromConfig();
    ownerRef->viewport2DPanel->UpdateScene();
    ownerRef->viewport2DPanel->Refresh();
  }
  if (ownerRef->viewport2DRenderPanel)
    ownerRef->viewport2DRenderPanel->ApplyConfig();
  if (ownerRef->layerPanel)
    ownerRef->layerPanel->ReloadLayers();
  ownerRef->RefreshSummary();
  ownerRef->RefreshRigging();

  // Keep import behavior aligned with the existing Tools > Auto color flow:
  // once the MVR scene is loaded, trigger auto-color so fixture types without
  // dictionary colors still receive a color by type/group.
  wxCommandEvent autoColorEvent;
  ownerRef->OnAutoColor(autoColorEvent);

  importProgress.reset();
  importOverlay.reset();
  importDisabler.reset();

  if (ownerRef->GetStatusBar()) {
    const wxString fileName = wxFileName(filePath).GetFullName();
    ownerRef->SetStatusText("MVR imported: " + fileName, 0);
  }
  return true;
}

// Opens a file picker and imports the selected MVR file using the official open policy.
void MainWindowIoController::OnImportMVR(wxCommandEvent &) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("misc"));
  wxFileDialog openFileDialog(&owner_, "Import MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (openFileDialog.ShowModal() == wxID_CANCEL)
    return;

  const wxString filePath = openFileDialog.GetPath();
  const std::string pathUtf8 = filePath.ToUTF8().data();
  (void)ImportMvrWithOfficialPolicy(pathUtf8);
}


// Applies the official MVR-open policy by confirming unsaved changes before importing the file.
bool MainWindowIoController::ImportMvrWithOfficialPolicy(
    const std::string &pathUtf8) {
  if (!owner_.ConfirmSaveIfDirty(kMvrOpenAction, kMvrOpenTitle))
    return false;

  // Official policy for .mvr open/import actions:
  // reset application scene state and then import the selected package.
  // MvrImporter::ImportFromFile() performs the reset via ConfigManager.
  return ImportMvrFromPath(pathUtf8);
}

// Routes startup file paths to the corresponding project or MVR open workflow.
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
