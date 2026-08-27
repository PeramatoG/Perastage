/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"
#include "mvrxchange/mvr_xchange_dialog.h"
#include "filesystem_path_utils.h"
#include "mainwindow_io_controller.h"
#include "rider_fixture_resolution_workflow.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/busyinfo.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "guiconfigservices.h"
#include "consolepanel.h"
#include "exportfixturedialog.h"
#include "exportobjectdialog.h"
#include "exporttrussdialog.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "fixturetablepanel.h"
#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "hoisttablepanel.h"
#include "layoutviewerpanel.h"
#include "mvrexporter.h"
#include "mvr_export_result_dialog.h"
#include "mvrimporter.h"
#include "mvr_preferences.h"
#include "projectutils.h"
#include "riderimporter.h"
#include "ridertextdialog.h"
#include "sceneobjecttablepanel.h"
#include "tableprinter.h"
#include "truss_gdtf_builder.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

namespace {

// Converts a filesystem path to a UTF-8 string.
std::string Utf8StringFromPath(const std::filesystem::path &path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Returns the current project name suitable for a suggested output file name.
wxString SuggestedProjectFileBaseName(const std::string &projectPath,
                                      const wxString &projectDisplayName) {
  if (!projectPath.empty())
    return wxFileName(wxString::FromUTF8(projectPath)).GetName();
  if (!projectDisplayName.IsEmpty() && projectDisplayName != "Untitled")
    return projectDisplayName;
  return {};
}

// Copies a generated fixture GDTF into the project MVR root and returns its
// archive-relative file name.
bool CopyFixtureGdtfIntoProject(const std::filesystem::path &sourcePath,
                                const std::filesystem::path &projectBasePath,
                                std::string &relativeSpecOut,
                                std::string &errorOut) {
  relativeSpecOut.clear();
  errorOut.clear();
  if (sourcePath.empty() || projectBasePath.empty()) {
    errorOut = "Could not prepare a project-local fixture GDTF copy.";
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(sourcePath, ec) || ec ||
      !std::filesystem::is_regular_file(sourcePath, ec) || ec) {
    errorOut = "The generated Perastage fixture GDTF does not exist.";
    return false;
  }

  std::filesystem::create_directories(projectBasePath, ec);
  if (ec) {
    errorOut = "Could not prepare the project MVR resource folder.";
    return false;
  }

  const std::filesystem::path targetPath =
      projectBasePath / sourcePath.filename();
  const std::filesystem::path canonicalSource =
      std::filesystem::weakly_canonical(sourcePath, ec);
  if (ec) {
    errorOut = "Could not resolve the generated Perastage fixture GDTF path.";
    return false;
  }
  ec.clear();
  const std::filesystem::path canonicalTarget =
      std::filesystem::weakly_canonical(targetPath, ec);
  const bool sameFile = !ec && canonicalSource == canonicalTarget;
  ec.clear();
  if (!sameFile) {
    std::filesystem::copy_file(
        sourcePath, targetPath,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      errorOut =
          "Could not copy the generated Perastage fixture GDTF into the "
          "project MVR root.";
      return false;
    }
  }

  const std::filesystem::path relativePath =
      std::filesystem::relative(targetPath, projectBasePath, ec);
  if (ec) {
    errorOut = "Could not compute the project MVR fixture GDTF reference.";
    return false;
  }

  relativeSpecOut = relativePath.generic_string();
  return true;
}

// Ensures saved project files use the Perastage project extension.
std::string EnsureProjectFileExtension(const std::string &path) {
  if (path.empty())
    return path;

  const std::filesystem::path candidate = PathUtils::PathFromUtf8(path);
  const std::string ext = candidate.extension().string();
  const std::string requiredExt = ProjectUtils::PROJECT_EXTENSION;
  if (ext == requiredExt)
    return Utf8StringFromPath(candidate);
  if (!ext.empty())
    return Utf8StringFromPath(candidate);

  std::filesystem::path withExtension = candidate;
  withExtension += requiredExt;
  return Utf8StringFromPath(withExtension);
}

class ScopeExit {
public:
  explicit ScopeExit(std::function<void()> fn) : fn_(std::move(fn)) {}
  ~ScopeExit() {
    if (fn_)
      fn_();
  }

  ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;

private:
  std::function<void()> fn_;
};

} // namespace

void MainWindow::LockViewportInteraction() {
  ++viewportInteractionLockDepth;
  if (viewportInteractionLockDepth > 1)
    return;

  if (viewportPanel)
    viewportPanel->Disable();
  if (viewport2DPanel)
    viewport2DPanel->Disable();
  if (layoutViewerPanel)
    layoutViewerPanel->Disable();
}

void MainWindow::UnlockViewportInteraction() {
  if (viewportInteractionLockDepth <= 0)
    return;

  --viewportInteractionLockDepth;
  if (viewportInteractionLockDepth > 0)
    return;

  if (viewportPanel)
    viewportPanel->Enable();
  if (viewport2DPanel)
    viewport2DPanel->Enable();
  if (layoutViewerPanel)
    layoutViewerPanel->Enable();
}

// Opens a project file selected by the user.
void MainWindow::OnLoad(wxCommandEvent &event) {
  CancelActiveSceneClipboardPlacement();
  diagnostics::DiagnosticLogger::Info("Project open requested.");
  if (!GuardStartupProjectLoadAction("opening another project"))
    return;

  if (!ConfirmSaveIfDirty("loading a project", "Open Project"))
    return;

  const wxString projectExtension =
      wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION);
  wxString filter = wxString::Format(_("Perastage files (*%s)|*%s"),
                                     projectExtension, projectExtension);
  wxString projDir;
  if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("projects"));
  wxFileDialog dlg(this, _("Open Project"), projDir, "", filter,
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  wxString path = dlg.GetPath();
  const std::filesystem::path selectedPath(path.ToStdWstring());
  diagnostics::DiagnosticLogger::Info(
      "Project open selected: " +
      diagnostics::DiagnosticLogger::FileNameOnly(Utf8StringFromPath(selectedPath)));
  if (!LoadProjectFromPath(Utf8StringFromPath(selectedPath)))
    wxMessageBox(_("Failed to load project."), _("Error"), wxOK | wxICON_ERROR,
                 this);
}

// Reports whether external-open requests can be processed immediately by the IO controller.
bool MainWindow::CanProcessExternalOpenPath() const {
  return ioController != nullptr && !IsBeingDeleted();
}

// Delegates startup/open-file requests to the IO controller when it is still available.
bool MainWindow::OpenPathFromCommandLine(const std::string &path) {
  if (!CanProcessExternalOpenPath())
    return false;
  return ioController->OpenPathFromCommandLine(path);
}

// Imports an MVR file through the normal user choice dialog.
bool MainWindow::ImportMvrWithUserChoice(const std::string &path) {
  if (!CanProcessExternalOpenPath())
    return false;
  return ioController->ImportMvrWithUserChoice(path);
}

// Queues an external open path and processes it immediately only when startup is fully ready.
void MainWindow::EnqueueExternalOpenPath(const std::string &path) {
  QueueDeferredStartupOpenPath(path);
  if (IsStartupProjectLoadPending() || IsStartupInitializationPending() ||
      !CanProcessExternalOpenPath())
    return;
  CallAfter([this]() {
    if (!CanProcessExternalOpenPath() || IsStartupProjectLoadPending() ||
        IsStartupInitializationPending())
      return;
    ProcessDeferredStartupOpenPath();
  });
}

// Stores the latest deferred startup-open request so it can be processed when startup is ready.
void MainWindow::QueueDeferredStartupOpenPath(const std::string &path) {
  if (path.empty())
    return;
  deferredStartupOpenPath = path;
}

// Executes the deferred startup-open request when startup state and IO controller allow it.
void MainWindow::ProcessDeferredStartupOpenPath() {
  if (!deferredStartupOpenPath.has_value())
    return;
  if (IsStartupProjectLoadPending() || IsStartupInitializationPending() ||
      !CanProcessExternalOpenPath())
    return;

  const std::string path = *deferredStartupOpenPath;
  deferredStartupOpenPath.reset();
  startupDeferredOpenInProgress = true;
  diagnostics::DiagnosticLogger::Info(
      "Opening explicit startup path: " +
      diagnostics::DiagnosticLogger::FileNameOnly(path));
  const bool opened = OpenPathFromCommandLine(path);
  startupDeferredOpenInProgress = false;
  if (!opened)
    deferredStartupOpenPath = path;
}

// Save the current project to its existing path or route to Save As when no path exists.
void MainWindow::OnSave(wxCommandEvent &event) {
  CancelActiveSceneClipboardPlacement();
  diagnostics::DiagnosticLogger::Info("Project save requested.");
  if (currentProjectPath.empty()) {
    OnSaveAs(event);
    return;
  }

  std::unique_ptr<wxWindowDisabler> saveDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> saveOverlay =
      std::make_unique<wxBusyInfo>("Saving project...");
  wxYieldIfNeeded();

  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  // Always sync table edits before saving; each panel now applies only real changes.
  SyncSceneData();
  SaveUserConfigWithViewport2DState();
  if (!cfg.SaveProject(currentProjectPath)) {
    const std::string reason = cfg.GetLastProjectSaveError().empty()
                                   ? "See the diagnostic log for details."
                                   : cfg.GetLastProjectSaveError();
    diagnostics::DiagnosticLogger::Error(
        "Project save failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(currentProjectPath) +
        " - " + reason);
    wxMessageBox(_("Failed to save project.\n\n") + wxString::FromUTF8(reason),
                 _("Error"), wxOK | wxICON_ERROR, this);
  } else {
    diagnostics::DiagnosticLogger::Info(
        "Project save completed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(currentProjectPath));
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Saved " +
                                  wxString::FromUTF8(currentProjectPath));
    saveOverlay.reset();
    saveDisabler.reset();
    if (viewportPanel) {
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
  }
}

// Prompt for a destination path and save the current project under that file name.
void MainWindow::OnSaveAs(wxCommandEvent &event) {
  CancelActiveSceneClipboardPlacement();
  diagnostics::DiagnosticLogger::Info("Project save-as requested.");
  const wxString projectExtension =
      wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION);
  wxString filter = wxString::Format(_("Perastage files (*%s)|*%s"),
                                     projectExtension, projectExtension);
  wxString projDir;
  if (!currentProjectPath.empty())
    projDir = wxString::FromUTF8(
        std::filesystem::path(currentProjectPath).parent_path().string());
  else if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("projects"));

  const wxString suggestedProjectName = SuggestedProjectFileBaseName(
      currentProjectPath, currentProjectDisplayName);
  const wxString suggestedFileName =
      suggestedProjectName.IsEmpty() ? wxString()
                                     : suggestedProjectName + projectExtension;

  wxFileDialog dlg(this, _("Save Project"), projDir, suggestedFileName, filter,
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  const std::filesystem::path selectedPath(dlg.GetPath().ToStdWstring());
  currentProjectPath = EnsureProjectFileExtension(Utf8StringFromPath(selectedPath));
  currentProjectDisplayName.clear();

  std::unique_ptr<wxWindowDisabler> saveDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> saveOverlay =
      std::make_unique<wxBusyInfo>("Saving project...");
  wxYieldIfNeeded();

  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  // Always sync table edits before saving; each panel now applies only real changes.
  SyncSceneData();
  SaveUserConfigWithViewport2DState();
  if (!cfg.SaveProject(currentProjectPath)) {
    const std::string reason = cfg.GetLastProjectSaveError().empty()
                                   ? "See the diagnostic log for details."
                                   : cfg.GetLastProjectSaveError();
    diagnostics::DiagnosticLogger::Error(
        "Project save-as failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(currentProjectPath) +
        " - " + reason);
    wxMessageBox(_("Failed to save project.\n\n") + wxString::FromUTF8(reason),
                 _("Error"), wxICON_ERROR);
  } else {
    diagnostics::DiagnosticLogger::Info(
        "Project save-as completed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(currentProjectPath));
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Saved " +
                                  wxString::FromUTF8(currentProjectPath));
    saveOverlay.reset();
    saveDisabler.reset();
    if (viewportPanel) {
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
  }
  UpdateTitle();
}

// Import fixtures and trusses from a rider (.txt/.pdf)
void MainWindow::OnImportRider(wxCommandEvent &event) {
  diagnostics::DiagnosticLogger::Info("Text/rider import requested.");
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("misc"));
  wxFileDialog dlg(this, _("Import Rider"), miscDir, "",
                   "Rider files (*.txt;*.pdf)|*.txt;*.pdf",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  const std::filesystem::path selectedPath(dlg.GetPath().ToStdWstring());
  const auto pathU8 = selectedPath.u8string();
  const std::string pathUtf8(pathU8.begin(), pathU8.end());
  const std::string riderText = RiderImporter::LoadText(pathUtf8);
  if (riderText.empty()) {
    wxMessageBox(_("Failed to read rider."), _("Error"), wxICON_ERROR, this);
    return;
  }
  std::string preparedRiderText;
  RiderImporter::ImportPlan importPlan;
  const auto preflightResult =
      rider_fixture_resolution_gui::RunCreateFromTextPreflight(
          this, GetDefaultGuiConfigServices().LegacyConfigManager(), riderText,
          &preparedRiderText, &importPlan);
  if (preflightResult != rider_fixture_resolution_gui::PreflightResult::Proceed)
    return;
  std::unique_ptr<wxWindowDisabler> importDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> importOverlay =
      std::make_unique<wxBusyInfo>("Creating scene from rider...");
  wxYieldIfNeeded();

  LockViewportInteraction();
  ScopeExit viewportUnlock([this]() { UnlockViewportInteraction(); });
  if (!RiderImporter::ImportText(
          preparedRiderText.empty() ? riderText : preparedRiderText, {}, true,
          &importPlan)) {
    wxMessageBox(_("Failed to import rider."), _("Error"), wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to import " + dlg.GetPath());
  } else {
    viewer2d::ReconcileFixtureLabelOverridesWithScene(
        GetDefaultGuiConfigServices().LegacyConfigManager());
    importOverlay.reset();
    importDisabler.reset();
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Imported " + dlg.GetPath());
    importDisabler = std::make_unique<wxWindowDisabler>();
    importOverlay = std::make_unique<wxBusyInfo>("Refreshing scene view...");
    wxYieldIfNeeded();
    RefreshAfterSceneChange();
  }
}

void MainWindow::OnImportRiderText(wxCommandEvent &WXUNUSED(event)) {
  RiderTextDialog dlg(this);
  if (dlg.ShowModal() != wxID_OK)
    return;

  const std::string riderText = dlg.GetRiderTextUtf8();
  if (riderText.empty()) {
    wxMessageBox(_("Rider text is empty."), _("Error"), wxICON_ERROR);
    return;
  }

  std::string preparedRiderText;
  RiderImporter::ImportPlan importPlan;
  const auto preflightResult =
      rider_fixture_resolution_gui::RunCreateFromTextPreflight(
          this, GetDefaultGuiConfigServices().LegacyConfigManager(), riderText,
          &preparedRiderText, &importPlan);
  if (preflightResult !=
      rider_fixture_resolution_gui::PreflightResult::Proceed) {
    diagnostics::DiagnosticLogger::Info(
        preflightResult == rider_fixture_resolution_gui::PreflightResult::Cancelled
            ? "Text/rider fixture preflight cancelled."
            : "Text/rider fixture preflight failed.");
    return;
  }

  LockViewportInteraction();
  ScopeExit viewportUnlock([this]() { UnlockViewportInteraction(); });
  std::unique_ptr<wxWindowDisabler> createDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> createOverlay =
      std::make_unique<wxBusyInfo>("Generating scene from text...");
  std::unique_ptr<wxProgressDialog> createProgress;
  wxYieldIfNeeded();

  if (!RiderImporter::ImportText(
          preparedRiderText.empty() ? riderText : preparedRiderText,
          [&](const RiderImporter::ProgressState &progress) {
            if (!GetStatusBar())
              return;
            const wxString stageText = wxString::FromUTF8(progress.stage);
            if (progress.HasCount()) {
              const int safeTotal = std::max(progress.total, 1);
              const int clampedCompleted =
                  std::clamp(progress.completed, 0, safeTotal);
              if (!createProgress) {
                createOverlay.reset();
                createProgress = std::make_unique<wxProgressDialog>(
                    _("Text scene creation progress"), stageText, safeTotal, this,
                    wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_APP_MODAL);
              } else {
                createProgress->SetRange(safeTotal);
              }
              createProgress->Update(clampedCompleted, stageText);
              SetStatusText(
                  wxString::Format(_("Text import: %s (%d/%d)"), stageText,
                                   clampedCompleted, safeTotal),
                  0);
            } else {
              if (createProgress)
                createProgress->Pulse(stageText);
              SetStatusText(wxString::Format(_("Text import: %s"), stageText), 0);
            }
            GetStatusBar()->Update();
          },
          true, &importPlan)) {
    wxMessageBox(_("Failed to import rider text."), _("Error"), wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to import rider from text.");
    if (GetStatusBar())
      SetStatusText(_("Text import failed."), 0);
    return;
  }

  createProgress.reset();
  viewer2d::ReconcileFixtureLabelOverridesWithScene(
      GetDefaultGuiConfigServices().LegacyConfigManager());
  if (consolePanel)
    consolePanel->AppendMessage("[INFO] Imported rider from text.");
  // Keep behavior aligned with MVR import: assign deterministic colors after
  // scene creation so fixtures without dictionary colors are still grouped by
  // fixture type/mode.
  wxCommandEvent autoColorEvent;
  OnAutoColor(autoColorEvent);
  if (currentProjectPath.empty() && currentProjectDisplayName.IsEmpty()) {
    const wxString loadedFileTitle = dlg.GetLoadedFileTitle();
    if (!loadedFileTitle.IsEmpty()) {
      currentProjectDisplayName = loadedFileTitle;
      UpdateTitle();
    }
  }
  if (GetStatusBar())
    SetStatusText(_("Text import completed."), 0);
}

// Handles MVR file selection, import, and updates fixture/truss panels
// accordingly
void MainWindow::OnImportMVR(wxCommandEvent &event) {
  diagnostics::DiagnosticLogger::Info("MVR import requested.");
  if (ioController)
    ioController->OnImportMVR(event);
}

// Exports the current scene to an MVR file without mutating the live scene.
void MainWindow::OnExportMVR(wxCommandEvent &event) {
  CancelActiveSceneClipboardPlacement();
  diagnostics::DiagnosticLogger::Info("MVR export requested.");
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("misc"));
  const wxString suggestedProjectName = SuggestedProjectFileBaseName(
      currentProjectPath, currentProjectDisplayName);
  const wxString suggestedFileName = suggestedProjectName.IsEmpty()
                                         ? wxString()
                                         : suggestedProjectName + ".mvr";
  wxFileDialog saveFileDialog(this, _("Export MVR file"), miscDir,
                              suggestedFileName, "MVR files (*.mvr)|*.mvr",
                              wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (saveFileDialog.ShowModal() == wxID_CANCEL)
    return;

  MvrExporter exporter;
  wxString path = saveFileDialog.GetPath();
  diagnostics::DiagnosticLogger::Info(
      "MVR export selected: " +
      diagnostics::DiagnosticLogger::FileNameOnly(path.ToStdString()));
  // Apply the same explicit GUI-to-domain preparation used by project save.
  SyncSceneData();
  const MvrExportOptions exportOptions = mvr::preferences::LoadExportOptions(
      guiConfigServices->LegacyConfigManager());
  wxBusyInfo *busy = new wxBusyInfo(_("Saving project..."), this);
  const bool exported =
      exporter.ExportToFile(path.ToStdString(), exportOptions);
  delete busy;
  if (!exported) {
    diagnostics::DiagnosticLogger::Error(
        "MVR export failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(path.ToStdString()));
    ShowMvrExportResult(this, false, exporter.GetExportDiagnostics());
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to export " + path);
  } else {
    diagnostics::DiagnosticLogger::Info(
        "MVR export completed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(path.ToStdString()));
    ShowMvrExportResult(this, true, exporter.GetExportDiagnostics());
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Exported " + path);
  }
}

// Exports a selected truss as a Perastage-owned canonical GDTF.
void MainWindow::OnExportTruss(wxCommandEvent &WXUNUSED(event)) {
  auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  auto &trusses = scene.trusses;
  std::set<std::string> names;
  for (const auto &[uuid, t] : trusses)
    names.insert(t.name);
  if (names.empty()) {
    wxMessageBox(_("No truss data available."), _("Export Truss"),
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(names.begin(), names.end());
  ExportTrussDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedName();
  Truss *chosen = nullptr;
  for (auto &entry : trusses) {
    Truss &t = entry.second;
    if (t.name == sel) {
      chosen = &t;
      break;
    }
  }
  if (!chosen)
    return;

  const std::string canonicalFileName =
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(
          chosen->manufacturer,
          chosen->model.empty() ? chosen->name : chosen->model,
          chosen->name.empty() ? sel : chosen->name);
  wxString trussDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
  wxFileDialog saveDlg(this, _("Save Truss"), trussDir,
                       wxString::FromUTF8(canonicalFileName),
                       "GDTF files (*.gdtf)|*.gdtf",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  namespace fs = std::filesystem;
  std::string modelPath = chosen->symbolFile;
  if (fs::path(modelPath).is_relative() && !scene.basePath.empty())
    modelPath = (fs::path(scene.basePath) / modelPath).string();
  if (!fs::exists(modelPath)) {
    wxMessageBox(_("Model file not found."), _("Error"), wxOK | wxICON_ERROR);
    return;
  }

  Truss exportTruss = *chosen;
  exportTruss.symbolFile = modelPath;
  std::string error;
  const std::string exportedPath = std::string(saveDlg.GetPath().mb_str());
  if (!BuildTrussGdtfFromInstance(exportTruss, exportedPath, &error)) {
    wxMessageBox(error.empty() ? _("Failed to export truss GDTF.") : error,
                 _("Error"), wxOK | wxICON_ERROR);
    return;
  }

  chosen->gdtfSpec = exportedPath;
  chosen->modelFile = exportedPath;
  chosen->perastageAuxGdtfArchivePath = canonicalFileName;
  chosen->gdtfMode = chosen->gdtfMode.empty() ? "Default" : chosen->gdtfMode;

  wxMessageBox(_("Truss exported successfully."), _("Export Truss"),
               wxOK | wxICON_INFORMATION);
}

// Exports the effective current GDTF for a selected fixture type.
void MainWindow::OnExportFixture(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto &fixtures = scene.fixtures;
  std::set<std::string> types;
  for (const auto &[uuid, f] : fixtures)
    if (!f.typeName.empty())
      types.insert(f.typeName);
  if (types.empty()) {
    wxMessageBox(_("No fixture data available."), _("Export Fixture"),
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(types.begin(), types.end());
  ExportFixtureDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedType();
  const Fixture *chosen = nullptr;
  for (const auto &[uuid, f] : fixtures) {
    if (f.typeName == sel) {
      chosen = &f;
      break;
    }
  }
  if (!chosen || chosen->gdtfSpec.empty())
    return;

  const std::string chosenTypeName = chosen->typeName;
  const std::string chosenGdtfSpec = chosen->gdtfSpec;
  fs::path originalSrc = PathUtils::PathFromUtf8(chosenGdtfSpec);
  const std::string &base = scene.basePath;
  if (originalSrc.is_relative() && !base.empty())
    originalSrc = fs::path(base) / originalSrc;
  if (!fs::exists(originalSrc)) {
    wxMessageBox(_("GDTF file not found."), _("Error"), wxOK | wxICON_ERROR);
    return;
  }

  fs::path effectiveSrc = originalSrc;
  bool usingDerivative =
      GdtfDictionary::IsPerastageNamedGdtfFile(effectiveSrc.string());
  if (!usingDerivative &&
      (chosen->weightKg != 0.0f || chosen->powerConsumptionW != 0.0f)) {
    auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        chosenTypeName, effectiveSrc.string(), chosen->gdtfMode,
        chosen->category);
    if (!derivative || derivative->path.empty()) {
      wxMessageBox(_("Could not create the Perastage fixture derivative."),
                   _("Export Fixture"), wxOK | wxICON_ERROR);
      return;
    }
    effectiveSrc = PathUtils::PathFromUtf8(derivative->path);
    usingDerivative = true;
    if (!SetGdtfProperties(effectiveSrc.string(), chosen->weightKg,
                           chosen->powerConsumptionW,
                           GdtfMutationAudit::BuildPerastageModifiedBy())) {
      wxMessageBox(_("Could not update the Perastage fixture derivative."),
                   _("Export Fixture"), wxOK | wxICON_ERROR);
      return;
    }
    std::string derivativeSpec = effectiveSrc.string();
    if (!scene.basePath.empty()) {
      std::string copyError;
      if (!CopyFixtureGdtfIntoProject(effectiveSrc, fs::path(scene.basePath),
                                      derivativeSpec, copyError)) {
        wxMessageBox(wxString::FromUTF8(copyError), _("Export Fixture"),
                     wxOK | wxICON_ERROR);
        return;
      }
      effectiveSrc = fs::path(scene.basePath) / fs::path(derivativeSpec);
    }
    for (auto &[uuid, fixture] : scene.fixtures) {
      if (fixture.typeName == chosenTypeName &&
          fixture.gdtfSpec == chosenGdtfSpec)
        fixture.gdtfSpec = derivativeSpec;
    }
    if (fixturePanel)
      fixturePanel->ReloadData();
    RefreshAfterSceneChange();
  }

  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  const std::string defaultName =
      usingDerivative ? effectiveSrc.filename().string()
                      : PathUtils::PathFromUtf8(chosenGdtfSpec)
                            .filename()
                            .string();
  wxFileDialog saveDlg(this, _("Save Fixture"), fixDir,
                       wxString::FromUTF8(defaultName), "*.gdtf",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  const fs::path target =
      PathUtils::PathFromUtf8(std::string(saveDlg.GetPath().ToUTF8()));
  std::error_code equivalentError;
  if (fs::exists(target, equivalentError) &&
      fs::equivalent(target, originalSrc, equivalentError)) {
    const int answer = wxMessageBox(
        _("This will overwrite the original GDTF library asset. Perastage "
        "normally preserves originals and exports a derived copy instead.\n\n"
        "Do you want to overwrite the original GDTF file?"),
        _("Overwrite Original GDTF"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
        this);
    if (answer != wxYES)
      return;
  }

  std::error_code copyError;
  const bool targetIsEffectiveSource =
      fs::exists(target, copyError) &&
      fs::equivalent(target, effectiveSrc, copyError);
  if (!targetIsEffectiveSource) {
    fs::create_directories(target.parent_path(), copyError);
    copyError.clear();
    fs::copy_file(effectiveSrc, target, fs::copy_options::overwrite_existing,
                  copyError);
    if (copyError) {
      wxMessageBox(
          wxString::Format(_("Failed to write file: %s"),
                           wxString::FromUTF8(copyError.message())),
          _("Error"), wxOK | wxICON_ERROR);
      return;
    }
  }

  wxMessageBox(_("Fixture exported successfully."), _("Export Fixture"),
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportSceneObject(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto &objs = scene.sceneObjects;
  std::set<std::string> names;
  for (const auto &[uuid, obj] : objs)
    if (!obj.name.empty())
      names.insert(obj.name);
  if (names.empty()) {
    wxMessageBox(_("No scene objects available."), _("Export Scene Object"),
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(names.begin(), names.end());
  ExportObjectDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedName();
  const SceneObject *chosen = nullptr;
  for (const auto &[uuid, obj] : objs) {
    if (obj.name == sel) {
      chosen = &obj;
      break;
    }
  }
  if (!chosen || chosen->modelFile.empty())
    return;

  fs::path src = chosen->modelFile;
  if (src.is_relative() && !scene.basePath.empty())
    src = fs::path(scene.basePath) / src;
  if (!fs::exists(src)) {
    wxMessageBox(_("Model file not found."), _("Error"), wxOK | wxICON_ERROR);
    return;
  }

  wxString defName =
      wxString::FromUTF8(sel) + wxString(src.extension().string());
  wxString objDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("scene_objects"));
  wxFileDialog saveDlg(this, _("Save Object"), objDir, defName,
                       wxString("*") + wxString(src.extension().string()),
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  fs::path dest = std::string(saveDlg.GetPath().mb_str());
  std::error_code ec;
  fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    wxMessageBox(_("Failed to copy file."), _("Error"), wxOK | wxICON_ERROR);
    return;
  }

  wxMessageBox(_("Object exported successfully."), _("Export Scene Object"),
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportCSV(wxCommandEvent &WXUNUSED(event)) {
  wxArrayString options;
  if (fixturePanel)
    options.Add("Fixtures");
  if (trussPanel)
    options.Add("Trusses");
  if (hoistPanel)
    options.Add("Hoists");
  if (sceneObjPanel)
    options.Add("Objects");
  if (options.IsEmpty())
    return;

  wxSingleChoiceDialog dlg(this, _("Select table"), _("Export CSV"), options);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString choice = dlg.GetStringSelection();
  wxDataViewListCtrl *ctrl = nullptr;
  TablePrinter::TableType type = TablePrinter::TableType::Fixtures;
  if (choice == "Fixtures" && fixturePanel) {
    ctrl = fixturePanel->GetTableCtrl();
    type = TablePrinter::TableType::Fixtures;
  } else if (choice == "Trusses" && trussPanel) {
    ctrl = trussPanel->GetTableCtrl();
    type = TablePrinter::TableType::Trusses;
  } else if (choice == "Hoists" && hoistPanel) {
    ctrl = hoistPanel->GetTableCtrl();
    type = TablePrinter::TableType::Supports;
  } else if (choice == "Objects" && sceneObjPanel) {
    ctrl = sceneObjPanel->GetTableCtrl();
    type = TablePrinter::TableType::SceneObjects;
  }

  if (ctrl)
    TablePrinter::ExportCSV(this, ctrl, type, GetDefaultGuiConfigServices().LegacyConfigManager());
}

// Opens the MVR-xchange publisher dialog.
void MainWindow::OnMvrXchange(wxCommandEvent &WXUNUSED(event)) {
  MvrXchangeDialog dialog(this);
  dialog.ShowModal();
}
