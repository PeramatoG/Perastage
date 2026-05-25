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
#include "logger.h"
#include "mvrimporter.h"
#include "projectutils.h"
#include "sceneobjecttablepanel.h"
#include "splashscreen.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

// Stores the owning MainWindow pointer for IO operations during the controller lifetime.
MainWindowIoController::MainWindowIoController(MainWindow &owner)
    : ownerRef_(&owner) {}

// Imports an MVR file while keeping import progress and UI panel refreshes synchronized on the main thread.
bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  MainWindow *owner = ownerRef_;
  if (owner == nullptr || owner->guiConfigServices == nullptr)
    return false;
  // Marks the full MVR import pipeline as active to defer layout rendering until data is stable.
  owner->mvrImportPipelineActive = true;
  const wxString filePath = wxString::FromUTF8(pathUtf8);
  ConfigManager &cfg = owner->guiConfigServices->LegacyConfigManager();
  const std::optional<std::string> preservedLayoutsConfig =
      cfg.GetValue(kLayoutsConfigKey);
  const std::optional<std::string> preservedViewer3DRenderStyle =
      cfg.GetValue(kViewer3DRenderStyleConfigKey);
  auto setImportStatus = [owner](const wxString &message) {
    if (owner == nullptr || !owner->GetStatusBar())
      return;
    owner->SetStatusText(message, 0);
    owner->GetStatusBar()->Update();
  };

  setImportStatus("MVR import: preparing...");
  SplashScreen::Hide();

  const bool shouldShowBlockingImportUi = !owner->IsStartupProjectLoadPending();
  std::unique_ptr<wxWindowDisabler> importDisabler;
  std::unique_ptr<wxBusyInfo> importOverlay;
  std::unique_ptr<wxProgressDialog> importProgress;
  if (shouldShowBlockingImportUi) {
    importDisabler = std::make_unique<wxWindowDisabler>();
    importOverlay = std::make_unique<wxBusyInfo>("Importing MVR file...");
  }

  owner->LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(
      pathUtf8, true, true, [&](const MvrImporter::ProgressState &progress) {
        // Ignores worker-thread progress callbacks to keep UI mutation strictly main-thread only.
        if (!wxThread::IsMain())
          return;
        if (owner == nullptr)
          return;
        const std::string &stage = progress.stage;
        if (stage == "Conflict dialog:show") {
          importProgress.reset();
          importOverlay.reset();
          importDisabler.reset();
          return;
        }

        if (stage == "Conflict dialog:hide") {
          if (shouldShowBlockingImportUi) {
            importDisabler = std::make_unique<wxWindowDisabler>();
            importOverlay =
                std::make_unique<wxBusyInfo>("Importing MVR file...");
          }
          importProgress.reset();
          return;
        }

        if (progress.HasCount()) {
          const wxString title = "MVR import progress";
          const wxString stageText = wxString::FromUTF8(stage);
          const int safeTotal = std::max(progress.total, 1);
          const int clampedCompleted = std::clamp(progress.completed, 0, safeTotal);

          if (shouldShowBlockingImportUi && !importProgress) {
            importOverlay.reset();
            importProgress = std::make_unique<wxProgressDialog>(
                title, stageText, safeTotal + 1, owner,
                wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_APP_MODAL);
          } else if (importProgress) {
            importProgress->SetRange(safeTotal + 1);
          }

          const int dialogProgressValue =
              std::clamp(clampedCompleted, 0, std::max(1, safeTotal));
          if (importProgress)
            importProgress->Update(dialogProgressValue, stageText);
          setImportStatus(wxString::Format("MVR import: %s (%d/%d)", stageText,
                                           clampedCompleted, safeTotal));
          return;
        }

        if (importProgress)
          importProgress->Pulse(wxString::FromUTF8(stage));
        setImportStatus("MVR import: " + wxString::FromUTF8(stage));
      });
  owner->UnlockViewportInteraction();

  if (!imported) {
    if (preservedLayoutsConfig.has_value())
      cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
    else
      cfg.RemoveKey(kLayoutsConfigKey);
    layouts::LayoutManager::Get().LoadFromConfig(cfg);

    importProgress.reset();
    importOverlay.reset();
    importDisabler.reset();
    if (owner->GetStatusBar())
      owner->SetStatusText("MVR import failed.", 0);
    wxMessageBox("Failed to import MVR file.", "Error", wxOK | wxICON_ERROR,
                 owner);
    if (owner->consolePanel)
      owner->consolePanel->AppendMessage("Failed to import " + filePath);
    owner->mvrImportPipelineActive = false;
    return false;
  }

  setImportStatus("MVR import: refreshing panels...");
  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);
  if (owner->consolePanel)
    owner->consolePanel->AppendMessage("Imported " + filePath);
  owner->currentProjectPath.clear();
  owner->currentProjectDisplayName = wxFileName(filePath).GetName();
  ProjectUtils::SaveLastProjectPath("");
  owner->UpdateTitle();

  if (preservedLayoutsConfig.has_value())
    cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
  else
    cfg.RemoveKey(kLayoutsConfigKey);
  if (preservedViewer3DRenderStyle.has_value())
    cfg.SetValue(kViewer3DRenderStyleConfigKey, *preservedViewer3DRenderStyle);
  layouts::LayoutManager::Get().LoadFromConfig(cfg);
  // Re-enables layout rendering before UI panels reload imported layout data.
  owner->mvrImportPipelineActive = false;

  if (owner->layoutPanel)
    owner->layoutPanel->ReloadLayouts();
  if (owner->fixturePanel)
    owner->fixturePanel->ReloadData();
  if (owner->trussPanel)
    owner->trussPanel->ReloadData();
  if (owner->hoistPanel)
    owner->hoistPanel->ReloadData();
  if (owner->sceneObjPanel)
    owner->sceneObjPanel->ReloadData();
  if (owner->viewportPanel) {
    owner->viewportPanel->UpdateScene();
    owner->viewportPanel->Refresh();
  }
  if (owner->viewport2DPanel) {
    if (!owner->HasActiveLayout2DView())
      owner->viewport2DPanel->LoadViewFromConfig();
    owner->viewport2DPanel->UpdateScene();
    owner->viewport2DPanel->Refresh();
  }
  if (owner->viewport2DRenderPanel)
    owner->viewport2DRenderPanel->ApplyConfig();
  if (owner->layerPanel)
    owner->layerPanel->ReloadLayers();
  owner->RefreshSummary();
  owner->RefreshRigging();

  // Keep import behavior aligned with the existing Tools > Auto color flow:
  // once the MVR scene is loaded, trigger auto-color so fixture types without
  // dictionary colors still receive a color by type/group.
  wxCommandEvent autoColorEvent;
  owner->OnAutoColor(autoColorEvent);

  importProgress.reset();
  importOverlay.reset();
  importDisabler.reset();

  if (owner->GetStatusBar()) {
    const wxString fileName = wxFileName(filePath).GetFullName();
    owner->SetStatusText("MVR imported: " + fileName, 0);
  }
  return true;
}

// Opens a file picker and imports the selected MVR file using the official open policy.
void MainWindowIoController::OnImportMVR(wxCommandEvent &) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("misc"));
  if (!ownerRef_)
    return;

  wxFileDialog openFileDialog(ownerRef_, "Import MVR file", miscDir, "",
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
  if (!ownerRef_)
    return false;

  const bool shouldSkipDirtyConfirmation =
      ownerRef_->IsStartupProjectLoadPending() ||
      ownerRef_->IsStartupInitializationPending() ||
      ownerRef_->currentProjectPath.empty();
  if (!shouldSkipDirtyConfirmation &&
      !ownerRef_->ConfirmSaveIfDirty(kMvrOpenAction, kMvrOpenTitle))
    return false;

  // Official policy for .mvr open/import actions:
  // reset application scene state and then import the selected package.
  // MvrImporter::ImportFromFile() performs the reset via ConfigManager.
  return ImportMvrFromPath(pathUtf8);
}

// Routes startup file paths to the corresponding project or MVR open workflow.
bool MainWindowIoController::OpenPathFromCommandLine(
    const std::string &pathUtf8) {
  if (!ownerRef_)
    return false;

  MainWindow *owner = ownerRef_;
  if (owner == nullptr)
    return false;
  std::string extension = wxFileName(wxString::FromUTF8(pathUtf8)).GetExt()
                              .Lower()
                              .ToStdString();
  std::string projectExtension = wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION)
                                     .AfterFirst('.')
                                     .Lower()
                                     .ToStdString();

  if (extension == projectExtension) {
    const bool shouldSkipDirtyConfirmation =
        owner->IsStartupProjectLoadPending() ||
        owner->IsStartupInitializationPending() ||
        owner->currentProjectPath.empty() ||
        (owner->deferredStartupOpenPath.has_value() &&
         owner->currentProjectPath.empty());
    if (!shouldSkipDirtyConfirmation &&
        !owner->ConfirmSaveIfDirty("loading a project", "Open Project"))
      return false;

    if (!owner->LoadProjectFromPath(pathUtf8)) {
      wxMessageBox("Failed to load project.", "Error", wxOK | wxICON_ERROR,
                   owner);
      if (owner->GetStatusBar())
        owner->SetStatusText("Project load failed.", 0);
      if (owner->consolePanel) {
        owner->consolePanel->AppendMessage("Failed to load " +
                                           wxString::FromUTF8(pathUtf8));
      }
      return false;
    }

    if (owner->GetStatusBar()) {
      wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
      owner->SetStatusText("Project loaded: " + fileInfo.GetFullName(), 0);
    }
    return true;
  }

  if (extension == "mvr")
  {
    Logger::Instance().Log("Opening MVR from external request: " + pathUtf8);
    const bool imported = ImportMvrFromPath(pathUtf8);
    wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
    if (imported) {
      Logger::Instance().Log("MVR imported: " + fileInfo.GetFullName().ToStdString());
    } else {
      Logger::Instance().Log("MVR import failed: " + pathUtf8);
    }
    return imported;
  }

  if (owner->GetStatusBar())
    owner->SetStatusText("Unsupported startup file format.", 0);
  if (owner->consolePanel) {
    owner->consolePanel->AppendMessage("Unsupported startup file: " +
                                       wxString::FromUTF8(pathUtf8));
  }
  wxMessageBox("Unsupported startup file. Use " +
                   wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION) +
                   " or .mvr files.",
               "Unsupported file", wxOK | wxICON_WARNING, owner);
  return false;
}
