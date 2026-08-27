#include "mainwindow_io_controller.h"

#include "mainwindow.h"

#include <wx/arrstr.h>
#include <wx/busyinfo.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "LayoutManager.h"
#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "consolepanel.h"
#include "fixture_label_overrides.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"
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
      wxString::Format(_("The selected MVR contains %zu UUIDs that already exist "
                       "in the current project. Choose how Perastage should "
                       "handle those incoming objects."),
                       collisionCount),
      _("MVR Merge UUID Collisions"), choices);
  dialog.SetSelection(0);

  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;
  if (dialog.GetSelection() == 1)
    return mvr::MvrMergeUuidCollisionBehavior::ReplaceExisting;
  if (dialog.GetSelection() == 2)
    return mvr::MvrMergeUuidCollisionBehavior::SkipIncoming;
  return mvr::MvrMergeUuidCollisionBehavior::GenerateStableUuid;
}

// Formats a fixture type identity for the MVR merge conflict dialog.
wxString FormatFixtureTypeIdentityForDialog(
    const mvr::MvrFixtureTypeIdentity &identity) {
  wxString text = wxString::Format(
      "Type: %s\nGDTF: %s\nMode: %s",
      wxString::FromUTF8(identity.typeName).c_str(),
      wxString::FromUTF8(identity.gdtfSpec.empty() ? "(none)"
                                                   : identity.gdtfSpec)
          .c_str(),
      wxString::FromUTF8(identity.gdtfMode.empty() ? "(default)"
                                                   : identity.gdtfMode)
          .c_str());
  if (!identity.gdtfSha256.empty()) {
    text += wxString::Format(
        "\nSHA-256: %s",
        wxString::FromUTF8(identity.gdtfSha256.substr(0, 16) + "...").c_str());
  }
  return text;
}

// Shows fixture type conflict choices before an MVR merge is applied.
std::optional<mvr::MvrMergeFixtureTypeDecision>
ShowMvrFixtureTypeConflictDialog(wxWindow *parent,
                                 const mvr::MvrFixtureTypeConflict &conflict) {
  wxArrayString choices;
  choices.Add("Use current project definition for imported fixtures");
  choices.Add(wxString::Format(
      "Keep imported definition by renaming it to \"%s\"",
      wxString::FromUTF8(conflict.suggestedIncomingTypeName).c_str()));
  choices.Add("Cancel merge");

  wxString message = wxString::Format(
      "The imported MVR uses fixture type \"%s\" with a different GDTF "
      "definition than the current project. Choose how Perastage should "
      "resolve all incoming fixtures of this type.\n\nCurrent project "
      "definition:\n%s\n\n"
      "Imported definition:\n%s",
      wxString::FromUTF8(conflict.currentIdentity.typeName).c_str(),
      FormatFixtureTypeIdentityForDialog(conflict.currentIdentity).c_str(),
      FormatFixtureTypeIdentityForDialog(conflict.incomingIdentity).c_str());

  wxSingleChoiceDialog dialog(parent, message,
                              _("MVR Merge Fixture Type Conflict"), choices);
  dialog.SetSelection(0);
  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;
  if (dialog.GetSelection() == 1)
    return mvr::MvrMergeFixtureTypeDecision::RenameIncomingType;
  if (dialog.GetSelection() == 2)
    return mvr::MvrMergeFixtureTypeDecision::CancelMerge;
  return mvr::MvrMergeFixtureTypeDecision::UseCurrentDefinition;
}

// Formats non-blocking duplicate patch warnings for the merge summary.
std::string FormatMvrMergePatchAddressWarningSummary(
    const mvr::MvrMergeAnalysis &analysis) {
  const std::size_t warningCount = analysis.patchAddressWarnings.size();
  if (warningCount == 0)
    return {};

  return std::to_string(warningCount) +
         (warningCount == 1
              ? " duplicate DMX address warning"
              : " duplicate DMX address warnings") +
         "; duplicate patches will be highlighted after reload";
}

// Shows the user-facing MVR import mode selection dialog.
MvrImportChoice ShowMvrImportChoiceDialog(wxWindow *parent) {
  wxArrayString choices;
  choices.Add(_("Open as new project"));
  choices.Add(_("Merge into current project"));
  wxSingleChoiceDialog dialog(
      parent, _("Choose how Perastage should import the selected MVR file."),
      _("Import MVR"), choices);
  dialog.SetSelection(0);

  if (dialog.ShowModal() != wxID_OK)
    return MvrImportChoice::Cancel;
  if (dialog.GetSelection() == 1)
    return MvrImportChoice::MergeIntoCurrentProject;
  return MvrImportChoice::OpenAsNewProject;
}

} // namespace

// Stores the owning MainWindow pointer for IO operations.
MainWindowIoController::MainWindowIoController(MainWindow &owner)
    : ownerRef_(&owner) {}

// Imports an MVR file and synchronizes UI refreshes.
bool MainWindowIoController::ImportMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  diagnostics::DiagnosticLogger::Info(
      "MVR import started: " +
      diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
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
                  std::make_unique<wxBusyInfo>(_("Importing MVR file..."));
          }
          importProgress.reset();
          return;
        }

        if (progress.HasCount()) {
          const wxString title = _("MVR import progress");
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
      owner->SetStatusText(_("MVR import failed."), 0);
    wxMessageBox(_("Failed to import MVR file."), _("Error"), wxOK | wxICON_ERROR,
                 owner);
    if (owner->consolePanel)
      owner->consolePanel->AppendMessage("Failed to import " + filePath);
    diagnostics::DiagnosticLogger::Error(
        "MVR import failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
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

  // Re-enables layout rendering before queuing the scene-dependent preview.
  owner->mvrImportPipelineActive = false;
  owner->NotifySceneVisualContentChanged();

  importProgress.reset();
  importOverlay.reset();
  importDisabler.reset();

  if (owner->GetStatusBar()) {
    const wxString fileName = wxFileName(filePath).GetFullName();
    owner->SetStatusText(wxString::Format(_("MVR imported: %s"), fileName), 0);
  }
  diagnostics::DiagnosticLogger::Info(
      "MVR import completed: " +
      diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
  owner->NotifyFixtureSymbolProjectReplaced(true);
  return true;
}

// Refreshes all scene-dependent UI panels after MVR scene content changes.
void MainWindowIoController::RefreshPanelsAfterMvrSceneChange(
    bool autoColorScene) {
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

  if (autoColorScene) {
    // Keeps MVR scene updates aligned with the existing Tools > Auto color flow.
    wxCommandEvent autoColorEvent;
    owner->OnAutoColor(autoColorEvent);
  }
}

// Opens a file picker and imports the selected MVR file.
void MainWindowIoController::OnImportMVR(wxCommandEvent &) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("misc"));
  if (!ownerRef_)
    return;

  wxFileDialog openFileDialog(ownerRef_, _("Import MVR file"), miscDir, "",
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


// Imports an MVR file after asking whether to open or merge it.
bool MainWindowIoController::ImportMvrWithUserChoice(const std::string &pathUtf8) {
  if (!ownerRef_)
    return false;

  switch (ShowMvrImportChoiceDialog(ownerRef_)) {
  case MvrImportChoice::OpenAsNewProject:
    return ImportMvrWithOfficialPolicy(pathUtf8);
  case MvrImportChoice::MergeIntoCurrentProject:
    return MergeMvrFromPath(pathUtf8);
  case MvrImportChoice::Cancel:
    return false;
  }
  return false;
}

// Merges an MVR file into the current scene.
bool MainWindowIoController::MergeMvrFromPath(const std::string &pathUtf8) {
  constexpr const char *kLayoutsConfigKey = "layouts_collection";
  constexpr const char *kViewer3DRenderStyleConfigKey = "viewer3d_render_style";
  diagnostics::DiagnosticLogger::Info(
      "MVR import started: " +
      diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
  MainWindow *owner = ownerRef_;
  if (owner == nullptr || owner->guiConfigServices == nullptr)
    return false;

  ConfigManager &cfg = owner->guiConfigServices->LegacyConfigManager();
  const MvrScene preservedScene = cfg.GetScene();
  const ConfigManager::DirtyState preservedDirtyState =
      cfg.CaptureDirtyState();
  const std::vector<std::string> preservedSelectedFixtures =
      cfg.GetSelectedFixtures();
  const std::vector<std::string> preservedSelectedTrusses =
      cfg.GetSelectedTrusses();
  const std::vector<std::string> preservedSelectedSupports =
      cfg.GetSelectedSupports();
  const std::vector<std::string> preservedSelectedSceneObjects =
      cfg.GetSelectedSceneObjects();
  const std::optional<std::string> preservedLayoutsConfig =
      cfg.GetValue(kLayoutsConfigKey);
  const std::optional<std::string> preservedViewer3DRenderStyle =
      cfg.GetValue(kViewer3DRenderStyleConfigKey);
  const std::unordered_set<std::string> preservedHiddenLayers =
      cfg.GetHiddenLayers();
  const std::string preservedCurrentLayer = cfg.GetCurrentLayer();

  // Restores merge-preserved project configuration and layer UI state.
  auto restorePreservedConfig = [&cfg, &preservedLayoutsConfig,
                                 &preservedViewer3DRenderStyle,
                                 &preservedHiddenLayers,
                                 &preservedCurrentLayer]() {
    if (preservedLayoutsConfig.has_value())
      cfg.SetValue(kLayoutsConfigKey, *preservedLayoutsConfig);
    else
      cfg.RemoveKey(kLayoutsConfigKey);
    if (preservedViewer3DRenderStyle.has_value())
      cfg.SetValue(kViewer3DRenderStyleConfigKey,
                   *preservedViewer3DRenderStyle);
    else
      cfg.RemoveKey(kViewer3DRenderStyleConfigKey);
    cfg.SetHiddenLayers(preservedHiddenLayers);
    cfg.SetCurrentLayer(preservedCurrentLayer);
    layouts::LayoutManager::Get().LoadFromConfig(cfg);
  };

  // Restores the live project exactly as it was before merge analysis began.
  auto restoreMergeRollbackState = [&cfg, &preservedScene,
                                    &preservedDirtyState,
                                    &preservedSelectedFixtures,
                                    &preservedSelectedTrusses,
                                    &preservedSelectedSupports,
                                    &preservedSelectedSceneObjects,
                                    &restorePreservedConfig]() {
    cfg.GetScene() = preservedScene;
    cfg.SetSelectedFixtures(preservedSelectedFixtures);
    cfg.SetSelectedTrusses(preservedSelectedTrusses);
    cfg.SetSelectedSupports(preservedSelectedSupports);
    cfg.SetSelectedSceneObjects(preservedSelectedSceneObjects);
    restorePreservedConfig();
    cfg.RestoreDirtyState(preservedDirtyState);
  };

  diagnostics::DiagnosticLogger::Info(
      "MVR merge started: " +
      diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
  owner->mvrImportPipelineActive = true;
  SplashScreen::Hide();
  if (owner->GetStatusBar())
    owner->SetStatusText(_("MVR merge: importing selected file..."), 0);

  MvrImportResult importResult;
  MvrImporter mergeImporter;
  owner->LockViewportInteraction();
  MvrImportOptions importOptions;
  importOptions.sourceKind = MvrImportSourceKind::MergeImport;
  const bool imported = mergeImporter.ImportFromFile(
      pathUtf8, importResult, MvrImportMode::ParseOnly, importOptions);
  owner->UnlockViewportInteraction();

  if (!imported) {
    restoreMergeRollbackState();
    owner->mvrImportPipelineActive = false;
    if (owner->GetStatusBar())
      owner->SetStatusText(_("MVR merge failed."), 0);
    wxMessageBox(_("Failed to import MVR file for merge."), _("Error"),
                 wxOK | wxICON_ERROR, owner);
    if (owner->consolePanel)
      owner->consolePanel->AppendMessage("Failed to merge " +
                                         wxString::FromUTF8(pathUtf8));
    diagnostics::DiagnosticLogger::Error(
        "MVR merge failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
    return false;
  }

  mvr::MvrMergeOptions mergeOptions;
  const mvr::MvrMergeAnalysis preflightAnalysis =
      mvr::AnalyzeImportedSceneMerge(preservedScene, importResult.scene);
  if (preflightAnalysis.uuidCollisionsDetected > 0) {
    const auto collisionBehavior = ShowMvrMergeUuidCollisionDialog(
        owner, preflightAnalysis.uuidCollisionsDetected);
    if (!collisionBehavior.has_value()) {
      restoreMergeRollbackState();
      owner->mvrImportPipelineActive = false;
      if (owner->GetStatusBar())
        owner->SetStatusText(_("MVR merge cancelled."), 0);
      return false;
    }
    mergeOptions.uuidCollisionBehavior = *collisionBehavior;
  }
  for (const auto &conflict : preflightAnalysis.fixtureTypeConflicts) {
    const auto decision = ShowMvrFixtureTypeConflictDialog(owner, conflict);
    if (!decision.has_value() ||
        *decision == mvr::MvrMergeFixtureTypeDecision::CancelMerge) {
      restoreMergeRollbackState();
      owner->mvrImportPipelineActive = false;
      if (owner->GetStatusBar())
        owner->SetStatusText(_("MVR merge cancelled."), 0);
      return false;
    }
    mergeOptions.fixtureTypeDecisions[conflict.normalizedTypeName] =
        *decision;
  }
  const mvr::MvrMergeAnalysis applyAnalysis =
      mvr::AnalyzeImportedSceneMerge(preservedScene, importResult.scene,
                                     mergeOptions);
  cfg.PushUndoState("merge MVR");
  cfg.GetScene() = preservedScene;
  const mvr::MvrSceneMergeResult mergeResult =
      mvr::ApplyImportedSceneMergeAtomically(
          cfg.GetScene(), importResult.scene, applyAnalysis);
  if (mergeResult.fixtureTypeConflictsBlocked > 0) {
    restoreMergeRollbackState();
    owner->mvrImportPipelineActive = false;
    if (owner->GetStatusBar())
      owner->SetStatusText(_("MVR merge cancelled."), 0);
    wxMessageBox(_("The MVR merge was cancelled because fixture type definition "
                 "conflicts were not resolved."),
                 _("MVR Merge Cancelled"), wxOK | wxICON_WARNING, owner);
    return false;
  }
  cfg.SetSelectedFixtures(preservedSelectedFixtures);
  cfg.SetSelectedTrusses(preservedSelectedTrusses);
  cfg.SetSelectedSupports(preservedSelectedSupports);
  cfg.SetSelectedSceneObjects(preservedSelectedSceneObjects);
  restorePreservedConfig();
  cfg.MarkDirty();
  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);
  RefreshPanelsAfterMvrSceneChange(false);
  owner->mvrImportPipelineActive = false;
  owner->NotifySceneVisualContentChanged();

  const wxString fileName =
      wxFileName(wxString::FromUTF8(pathUtf8)).GetFullName();
  if (owner->consolePanel) {
    std::ostringstream summary;
    summary << "Merged " << pathUtf8 << " (" << mergeResult.fixturesAdded
            << " fixtures, " << mergeResult.trussesAdded << " trusses, "
            << mergeResult.supportsAdded << " hoists, "
            << mergeResult.sceneObjectsAdded << " objects";
    if (mergeResult.nonObjectLookupConflictsResolved > 0)
      summary << ", " << mergeResult.nonObjectLookupConflictsResolved
              << " lookup conflicts resolved";
    const std::string patchWarningSummary =
        FormatMvrMergePatchAddressWarningSummary(applyAnalysis);
    if (!patchWarningSummary.empty())
      summary << "; Warning: " << patchWarningSummary;
    summary << ")";
    owner->consolePanel->AppendMessage(wxString::FromUTF8(summary.str()));
  }
  if (owner->GetStatusBar())
    owner->SetStatusText(wxString::Format(_("MVR merged: %s"), fileName), 0);
  diagnostics::DiagnosticLogger::Info(
      "MVR merge completed: " +
      diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
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
        !owner->ConfirmSaveIfDirty(_("loading a project"), _("Open Project")))
      return false;

    if (!owner->LoadProjectFromPath(pathUtf8)) {
      wxMessageBox(_("Failed to load project."), _("Error"), wxOK | wxICON_ERROR,
                   owner);
      if (owner->GetStatusBar())
        owner->SetStatusText(_("Project load failed."), 0);
      if (owner->consolePanel) {
        owner->consolePanel->AppendMessage("Failed to load " +
                                           wxString::FromUTF8(pathUtf8));
      }
      return false;
    }

    if (owner->GetStatusBar()) {
      wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
      owner->SetStatusText(
          wxString::Format(_("Project loaded: %s"), fileInfo.GetFullName()), 0);
    }
    return true;
  }

  if (extension == "mvr") {
    diagnostics::DiagnosticLogger::Info(
        "Opening MVR from external request: " +
        diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
    const bool imported = ImportMvrWithOfficialPolicy(pathUtf8);
    wxFileName fileInfo(wxString::FromUTF8(pathUtf8));
    if (imported) {
      diagnostics::DiagnosticLogger::Info("MVR imported: " +
                                          fileInfo.GetFullName().ToStdString());
    } else {
      diagnostics::DiagnosticLogger::Error(
          "MVR import failed: " +
          diagnostics::DiagnosticLogger::FileNameOnly(pathUtf8));
    }
    return imported;
  }

  if (owner->GetStatusBar())
    owner->SetStatusText(_("Unsupported startup file format."), 0);
  if (owner->consolePanel) {
    owner->consolePanel->AppendMessage("Unsupported startup file: " +
                                       wxString::FromUTF8(pathUtf8));
  }
  wxMessageBox(wxString::Format(
                   _("Unsupported startup file. Use %s or .mvr files."),
                   wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION)),
               _("Unsupported file"), wxOK | wxICON_WARNING, owner);
  return false;
}
