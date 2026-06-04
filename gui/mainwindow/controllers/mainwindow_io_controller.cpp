#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/arrstr.h>
#include <wx/busyinfo.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "LayoutManager.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "fixture_label_overrides.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"
#include "logger.h"
#include "mvrimporter.h"
#include "mvrscenemerger.h"
#include "projectutils.h"
#include "sceneobjecttablepanel.h"
#include "splashscreen.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

namespace {

enum class MvrImportChoice {
  OpenAsNewProject,
  MergeIntoCurrentProject,
  Cancel,
};

// Shows merge UUID collision policy choices.
std::optional<mvr::MvrMergeUuidCollisionBehavior>
ShowMvrMergeUuidCollisionDialog(wxWindow *parent, std::size_t collisionCount) {
  wxArrayString choices;
  choices.Add("Create new UUIDs for imported objects");
  choices.Add("Replace existing project objects");
  choices.Add("Skip incoming colliding objects");
  wxSingleChoiceDialog dialog(
      parent,
      wxString::Format("The selected MVR contains %zu UUIDs that already exist "
                       "in the current project. Choose how Perastage should "
                       "handle those incoming objects.",
                       collisionCount),
      "MVR Merge UUID Collisions", choices);
  dialog.SetSelection(0);

  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;
  if (dialog.GetSelection() == 1)
    return mvr::MvrMergeUuidCollisionBehavior::ReplaceExisting;
  if (dialog.GetSelection() == 2)
    return mvr::MvrMergeUuidCollisionBehavior::SkipIncoming;
  return mvr::MvrMergeUuidCollisionBehavior::GenerateStableUuid;
}

// Shows the user-facing MVR import mode selection dialog.
MvrImportChoice ShowMvrImportChoiceDialog(wxWindow *parent) {
  wxArrayString choices;
  choices.Add("Open as new project");
  choices.Add("Merge into current project");
  wxSingleChoiceDialog dialog(
      parent, "Choose how Perastage should import the selected MVR file.",
      "Import MVR", choices);
  dialog.SetSelection(0);

  if (dialog.ShowModal() != wxID_OK)
    return MvrImportChoice::Cancel;
  if (dialog.GetSelection() == 1)
    return MvrImportChoice::MergeIntoCurrentProject;
  return MvrImportChoice::OpenAsNewProject;
}

} // namespace

// Stores the owning MainWindow pointer for IO operations during the controller
// lifetime.
MainWindowIoController::MainWindowIoController(MainWindow &owner)
    : ownerRef_(&owner) {}

// Imports an MVR file while keeping import progress and UI panel refreshes
// synchronized on the main thread.
bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  MainWindow *owner = ownerRef_;
  if (owner == nullptr || owner->guiConfigServices == nullptr)
    return false;
  // Marks the full MVR import pipeline as active to defer layout rendering
  // until data is stable.
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
  const bool shouldUseBusyOverlay = false;
  const bool shouldUseProgressDialog = false;
  std::unique_ptr<wxWindowDisabler> importDisabler;
  std::unique_ptr<wxBusyInfo> importOverlay;
  std::unique_ptr<wxProgressDialog> importProgress;
  if (shouldShowBlockingImportUi) {
    importDisabler = std::make_unique<wxWindowDisabler>();
    if (shouldUseBusyOverlay)
      importOverlay = std::make_unique<wxBusyInfo>("Importing MVR file...");
  }

  owner->LockViewportInteraction();
  const bool imported = MvrImporter::ImportAndRegister(
      pathUtf8, true, true, [&](const MvrImporter::ProgressState &progress) {
        // Ignores worker-thread progress callbacks to keep UI mutation strictly
        // main-thread only.
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
            if (shouldUseBusyOverlay)
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
          const int clampedCompleted =
              std::clamp(progress.completed, 0, safeTotal);

          if (shouldShowBlockingImportUi && shouldUseProgressDialog &&
              !importProgress) {
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

  RefreshPanelsAfterMvrSceneChange();

  importProgress.reset();
  importOverlay.reset();
  importDisabler.reset();

  if (owner->GetStatusBar()) {
    const wxString fileName = wxFileName(filePath).GetFullName();
    owner->SetStatusText("MVR imported: " + fileName, 0);
  }
  // Re-enables layout rendering only after all import-driven panel updates
  // finish.
  owner->mvrImportPipelineActive = false;
  return true;
}

// Refreshes all scene-dependent UI panels after MVR scene content changes.
void MainWindowIoController::RefreshPanelsAfterMvrSceneChange() {
  MainWindow *owner = ownerRef_;
  if (owner == nullptr)
    return;

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

  // Keeps MVR scene updates aligned with the existing Tools > Auto color flow.
  wxCommandEvent autoColorEvent;
  owner->OnAutoColor(autoColorEvent);

  if (owner->layoutViewerPanel)
    owner->layoutViewerPanel->RefreshAfterSceneContentUpdate();
}

// Opens a file picker and imports the selected MVR file using the selected
// import mode.
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
  switch (ShowMvrImportChoiceDialog(ownerRef_)) {
  case MvrImportChoice::OpenAsNewProject:
    (void)ImportMvrWithOfficialPolicy(pathUtf8);
    return;
  case MvrImportChoice::MergeIntoCurrentProject:
    (void)MergeMvrFromPath(pathUtf8);
    return;
  case MvrImportChoice::Cancel:
    return;
  }
}

// Merges an MVR file into the current scene.
bool MainWindowIoController::MergeMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  MainWindow *owner = ownerRef_;
  if (owner == nullptr || owner->guiConfigServices == nullptr)
    return false;

  ConfigManager &cfg = owner->guiConfigServices->LegacyConfigManager();
  MvrScene currentScene = cfg.GetScene();
  const std::optional<std::string> preservedLayoutsConfig =
      cfg.GetValue(kLayoutsConfigKey);
  const std::optional<std::string> preservedViewer3DRenderStyle =
      cfg.GetValue(kViewer3DRenderStyleConfigKey);

  auto restorePreservedConfig = [&cfg, &preservedLayoutsConfig,
                                 &preservedViewer3DRenderStyle]() {
    if (preservedLayoutsConfig.has_value())
      cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
    else
      cfg.RemoveKey(kLayoutsConfigKey);
    if (preservedViewer3DRenderStyle.has_value())
      cfg.SetValue(kViewer3DRenderStyleConfigKey,
                   *preservedViewer3DRenderStyle);
    else
      cfg.RemoveKey(kViewer3DRenderStyleConfigKey);
    layouts::LayoutManager::Get().LoadFromConfig(cfg);
  };

  owner->mvrImportPipelineActive = true;
  SplashScreen::Hide();
  if (owner->GetStatusBar())
    owner->SetStatusText("MVR merge: importing selected file...", 0);

  MvrImportResult importResult;
  MvrImporter mergeImporter;
  owner->LockViewportInteraction();
  const bool imported = mergeImporter.ImportFromFile(
      pathUtf8, importResult, MvrImportMode::ParseOnly, true, true);
  owner->UnlockViewportInteraction();

  if (!imported) {
    cfg.GetScene() = currentScene;
    restorePreservedConfig();
    owner->mvrImportPipelineActive = false;
    if (owner->GetStatusBar())
      owner->SetStatusText("MVR merge failed.", 0);
    wxMessageBox("Failed to import MVR file for merge.", "Error",
                 wxOK | wxICON_ERROR, owner);
    if (owner->consolePanel)
      owner->consolePanel->AppendMessage("Failed to merge " +
                                         wxString::FromUTF8(pathUtf8));
    return false;
  }

  cfg.GetScene() = currentScene;
  mvr::MvrMergeOptions mergeOptions;
  const mvr::MvrMergeAnalysis preflightAnalysis =
      mvr::AnalyzeImportedSceneMerge(currentScene, importResult.scene);
  if (preflightAnalysis.uuidCollisionsDetected > 0) {
    const auto collisionBehavior = ShowMvrMergeUuidCollisionDialog(
        owner, preflightAnalysis.uuidCollisionsDetected);
    if (!collisionBehavior.has_value()) {
      restorePreservedConfig();
      owner->mvrImportPipelineActive = false;
      if (owner->GetStatusBar())
        owner->SetStatusText("MVR merge cancelled.", 0);
      return false;
    }
    mergeOptions.uuidCollisionBehavior = *collisionBehavior;
  }
  cfg.PushUndoState("merge MVR");
  const mvr::MvrSceneMergeResult mergeResult =
      mvr::MergeImportedSceneIntoCurrent(cfg.GetScene(), importResult.scene,
                                         mergeOptions);
  cfg.MarkDirty();
  restorePreservedConfig();
  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);
  RefreshPanelsAfterMvrSceneChange();
  owner->mvrImportPipelineActive = false;

  const wxString fileName =
      wxFileName(wxString::FromUTF8(pathUtf8)).GetFullName();
  if (owner->consolePanel) {
    std::ostringstream summary;
    summary << "Merged " << pathUtf8 << " (" << mergeResult.fixturesAdded
            << " fixtures, " << mergeResult.trussesAdded << " trusses, "
            << mergeResult.supportsAdded << " hoists, "
            << mergeResult.sceneObjectsAdded << " objects)";
    owner->consolePanel->AppendMessage(wxString::FromUTF8(summary.str()));
  }
  if (owner->GetStatusBar())
    owner->SetStatusText("MVR merged: " + fileName, 0);
  return true;
}

// Applies the official MVR-open policy after dirty checks.
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
  std::string extension =
      wxFileName(wxString::FromUTF8(pathUtf8)).GetExt().Lower().ToStdString();
  std::string projectExtension =
      wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION)
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

  if (extension == "mvr") {
    Logger::Instance().Log("Opening MVR from external request: " + pathUtf8);
    const bool imported = ImportMvrWithOfficialPolicy(pathUtf8);
    wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
    if (imported) {
      Logger::Instance().Log("MVR imported: " +
                             fileInfo.GetFullName().ToStdString());
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
