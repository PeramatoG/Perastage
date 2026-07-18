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
#include "about_dialog.h"
#include "filesystem_path_utils.h"
#include "mainwindow.h"
#include "mainwindow/controllers/mainwindow_io_controller.h"
#include "mainwindow_menu_builders.h"
#include "mainwindow_menu_helpers.h"
#include "mainwindow_menu_text_utils.h"
#include "mainwindow_view_controller.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <wx/artprov.h>
#include <wx/busyinfo.h>
#include <wx/choicdlg.h>
#include <wx/choice.h>
#include <wx/datetime.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/html/htmlwin.h>
#include <wx/numdlg.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "addfixturedialog.h"
#include "addsceneobjectdialog.h"
#include "addtrussdialog.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "credentialstore.h"
#include "diagnostics/DiagnosticLogger.h"
#include "diagnostics/DiagnosticPaths.h"
#include "diagnostics/DiagnosticReport.h"
#include "dictionaryeditdialog.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtf_catalog_service.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "gdtf_share_workflow.h"
#include "gdtfsearchdialog.h"
#include "guiconfigservices.h"
#include "hoist_weight_distribution.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"
#include "loader_obj.h"
#include "logger.h"
#include "logindialog.h"
#include "magnet_snap.h"
#include "../viewport_interaction_scope.h"
#include "mainwindow_gdtf_credentials.h"
#include "markdown.h"
#include "preferencesdialog.h"
#include "projectutils.h"
#include "resource_path_utils.h"
#include "rigging_extra_weight_settings.h"
#include "riggingpanel.h"
#include "scene_grouping.h"
#include "scene_object_truss_converter.h"
#include "scene_object_primitive_creation.h"
#include "scene_object_primitive_dialogs.h"
#include "sceneobjecttablepanel.h"
#include "selectfixturetypedialog.h"
#include "selection_movement_settings.h"
#include "selectnamedialog.h"
#include "support.h"
#include "tools/fixture_category_assignment_tool.h"
#include "tools/fixture_symbol_generation_tool.h"
#include "truss_creation_source.h"
#include "trussloader.h"
#include "trusstablepanel.h"
#include "ui_feature_flags.h"
#include "update/app_update_service.h"
#include "update/update_notification_dialog.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

// Builds and registers the main application toolbars.
void MainWindow::CreateToolBars() {
  const long toolbarStyle = (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL);
  fileToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, toolbarStyle);
  fileToolBar->SetToolBitmapSize(wxSize(16, 16));

  const auto loadToolbarIcon = [](const std::string &name,
                                  const wxArtID &fallbackArtId) {
    auto svgPath = ProjectUtils::ResolveResourcePath(
        std::filesystem::path("icons") / "outline" / (name + ".svg"));
    if (std::filesystem::exists(svgPath)) {
      wxBitmapBundle bundle =
          wxBitmapBundle::FromSVGFile(svgPath.string(), wxSize(16, 16));
      if (bundle.IsOk()) {
        return bundle;
      }
    }
    return wxArtProvider::GetBitmapBundle(fallbackArtId, wxART_TOOLBAR,
                                          wxSize(16, 16));
  };
  const auto loadToolbarDisabledIcon = [&](const std::string &name,
                                           const wxArtID &fallbackArtId) {
    auto svgPath = ProjectUtils::ResolveResourcePath(
        std::filesystem::path("icons") / "outline" / (name + "-disabled.svg"));
    if (std::filesystem::exists(svgPath)) {
      wxBitmapBundle bundle =
          wxBitmapBundle::FromSVGFile(svgPath.string(), wxSize(16, 16));
      if (bundle.IsOk())
        return bundle;
    }
    return loadToolbarIcon(name, fallbackArtId);
  };
  const auto addToolWithDisabledIcon =
      [&](wxAuiToolBar *toolbar, int id, const wxString &label,
          const std::string &iconName, const wxArtID &fallbackArtId,
          const wxString &shortHelp, wxItemKind kind = wxITEM_NORMAL) {
        toolbar->AddTool(id, label, loadToolbarIcon(iconName, fallbackArtId),
                         shortHelp, kind);

        wxAuiToolBarItem *toolItem = toolbar->FindTool(id);
        if (toolItem) {
          toolItem->SetDisabledBitmap(
              loadToolbarDisabledIcon(iconName, fallbackArtId)
                  .GetBitmap(wxSize(16, 16)));
        }
      };
  fileToolBar->AddTool(ID_File_New, _("New"),
                       loadToolbarIcon("file", wxART_NEW),
                       _("Create a new project"));
  fileToolBar->AddTool(ID_File_Load, _("Open"),
                       loadToolbarIcon("folder-open", wxART_FILE_OPEN),
                       _("Open an existing project"));
  fileToolBar->AddTool(ID_File_Save, _("Save"),
                       loadToolbarIcon("save", wxART_FILE_SAVE),
                       _("Save the current project"));
  fileToolBar->AddTool(ID_File_SaveAs, _("Save As"),
                       loadToolbarIcon("save-all", wxART_FILE_SAVE),
                       _("Save the current project with a new name"));
  fileToolBar->AddTool(ID_File_ImportMVR, _("Import MVR"),
                       loadToolbarIcon("file-input", wxART_FILE_OPEN),
                       _("Import an MVR file"));
  fileToolBar->AddTool(ID_File_ExportMVR, _("Export MVR"),
                       loadToolbarIcon("file-output", wxART_FILE_SAVE),
                       _("Export the project to MVR"));
  fileToolBar->AddTool(ID_File_PrintMenu, _("Print"),
                       loadToolbarIcon("printer", wxART_PRINT),
                       _("Choose what to print"));
  fileToolBar->Realize();

  auiManager->AddPane(fileToolBar, wxAuiPaneInfo()
                                       .Name("FileToolbar")
                                       .Caption(_("File"))
                                       .ToolbarPane()
                                       .Top());

  editToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, toolbarStyle);
  editToolBar->SetToolBitmapSize(wxSize(16, 16));
  editToolBar->AddTool(ID_Edit_Undo, _("Undo"),
                       loadToolbarIcon("undo-2", wxART_UNDO),
                       _("Undo last action"));
  editToolBar->AddTool(ID_Edit_Redo, _("Redo"),
                       loadToolbarIcon("redo-2", wxART_REDO),
                       _("Redo last undone action"));
  editToolBar->Realize();
  auiManager->AddPane(editToolBar, wxAuiPaneInfo()
                                       .Name("EditToolbar")
                                       .Caption(_("Edit"))
                                       .ToolbarPane()
                                       .Top());

  layoutViewsToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                        wxDefaultSize, toolbarStyle);
  layoutViewsToolBar->SetToolBitmapSize(wxSize(16, 16));
  layoutViewsToolBar->AddTool(ID_View_Layout_Default, _("3D Layout View"),
                              loadToolbarIcon("box", wxART_MISSING_IMAGE),
                              _("Switch to 3D Layout View"));
  layoutViewsToolBar->AddTool(
      ID_View_Layout_2D, _("2D Layout View"),
      loadToolbarIcon("panels-right-bottom", wxART_MISSING_IMAGE),
      _("Switch to 2D Layout View"));
  layoutViewsToolBar->AddTool(
      ID_View_Layout_Mode, _("Layout Mode"),
      loadToolbarIcon("square-asterisk", wxART_MISSING_IMAGE),
      _("Switch to Layout Mode View"));
  layoutViewsToolBar->AddSeparator();
  layoutViewsToolBar->AddTool(
      ID_View_Viewport_Top, _("Top View"),
      loadToolbarIcon("cube-view-top", wxART_MISSING_IMAGE),
      _("Apply top view to active viewport"));
  layoutViewsToolBar->AddTool(
      ID_View_Viewport_Front, _("Front View"),
      loadToolbarIcon("cube-view-front", wxART_MISSING_IMAGE),
      _("Apply front view to active viewport"));
  layoutViewsToolBar->AddTool(
      ID_View_Viewport_Side, _("Side View"),
      loadToolbarIcon("cube-view-side", wxART_MISSING_IMAGE),
      _("Apply side view to active viewport"));
  layoutViewsToolBar->AddSeparator();
  addToolWithDisabledIcon(layoutViewsToolBar, ID_View_Viewport_SelectTool,
                          _("Select Tool"), "mouse-pointer-2",
                          wxART_NORMAL_FILE,
                          _("Switch to standard selection mode"), wxITEM_CHECK);
  addToolWithDisabledIcon(
      layoutViewsToolBar, ID_View_Viewport_MeasureTool, _("Measure Tool"),
      "ruler-dimension-line-center", wxART_MISSING_IMAGE,
      _("Toggle center-to-center measure tool"), wxITEM_CHECK);
  addToolWithDisabledIcon(
      layoutViewsToolBar, ID_View_Viewport_GapMeasureTool, _("Gap Measure Tool"),
      "ruler-dimension-line", wxART_MISSING_IMAGE,
      _("Toggle edge-to-edge gap measure tool"), wxITEM_CHECK);
  addToolWithDisabledIcon(layoutViewsToolBar, ID_View_Viewport_AxisConstraint,
                          _("Axis Lock"), "move-3d", wxART_MISSING_IMAGE,
                          _("Toggle axis-constrained selection movement"),
                          wxITEM_CHECK);
  addToolWithDisabledIcon(layoutViewsToolBar, ID_View_Viewport_LeftDragMove,
                          _("Drag Move"), "move", wxART_MISSING_IMAGE,
                          _("Toggle left-click selection dragging"),
                          wxITEM_CHECK);
  addToolWithDisabledIcon(layoutViewsToolBar, ID_View_Viewport_LocalAxes,
                          _("Local Axes"), "file-axis-3d",
                          wxART_MISSING_IMAGE,
                          _("Use local axes for viewport transforms"),
                          wxITEM_CHECK);
  addToolWithDisabledIcon(layoutViewsToolBar, ID_View_Viewport_Magnet,
                          _("Magnet"), "magnet", wxART_MISSING_IMAGE,
                          _("Toggle Magnet snapping while dragging"),
                          wxITEM_CHECK);
  addToolWithDisabledIcon(layoutViewsToolBar,
                          ID_View_Viewport_CrossTableActions,
                          _("Cross-table Actions"), "layers",
                          wxART_MISSING_IMAGE,
                          _("Toggle viewport actions across all tables"),
                          wxITEM_CHECK);
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_SelectTool, true);
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_MeasureTool, false);
  layoutViewsToolBar->ToggleTool(ID_View_Viewport_GapMeasureTool, false);
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_AxisConstraint,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kAxisConstrainedMovementConfigKey) !=
          "0");
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_LeftDragMove,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kLeftDragSelectionMovementConfigKey) ==
          "1");
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_LocalAxes,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          selection_movement_settings::kLocalTransformSpaceConfigKey) ==
          "1");
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_Magnet,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          magnet_snap::kMagnetEnabledConfigKey) == "1");
  layoutViewsToolBar->ToggleTool(
      ID_View_Viewport_CrossTableActions,
      GetDefaultGuiConfigServices().Preferences().GetValue(
          viewport_interaction_scope::kCrossTableActionsConfigKey) == "1");
  layoutViewsToolBar->Realize();
  auiManager->AddPane(layoutViewsToolBar, wxAuiPaneInfo()
                                              .Name("LayoutViewsToolbar")
                                              .Caption(_("Layout Views"))
                                              .ToolbarPane()
                                              .Top());

  toolsToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                  wxDefaultSize, toolbarStyle);
  toolsToolBar->SetToolBitmapSize(wxSize(16, 16));
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddFixture, _("Add Fixture"),
                          "spotlight", wxART_MISSING_IMAGE,
                          _("Insert a new fixture"));
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddTruss, _("Add Truss"),
                          "truss", wxART_MISSING_IMAGE,
                          _("Insert a new truss"));
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddSceneObject,
                          _("Add Scene Object"), "guitar", wxART_MISSING_IMAGE,
                          _("Insert a new scene object"));
  toolsToolBar->AddSeparator();
  toolsToolBar->AddTool(ID_Tools_DownloadGdtf, _("Download GDTF"),
                        loadToolbarIcon("cloud-download", wxART_MISSING_IMAGE),
                        _("Download GDTF"));
  toolsToolBar->AddTool(ID_Tools_ImportRiderText, _("Create from text"),
                        loadToolbarIcon("notepad-text", wxART_TIP),
                        _("Create from text"));
  toolsToolBar->Realize();
  auiManager->AddPane(toolsToolBar, wxAuiPaneInfo()
                                        .Name("ToolsToolbar")
                                        .Caption(_("Tools"))
                                        .ToolbarPane()
                                        .Top());

  layoutToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, toolbarStyle);
  layoutToolBar->SetToolBitmapSize(wxSize(16, 16));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_2DView,
                          _("Add 2D View"), "panel-top-bottom-dashed",
                          wxART_MISSING_IMAGE, _("Add 2D View to Layout"));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Legend, _("Add Legend"),
                          "layout-list", wxART_MISSING_IMAGE,
                          _("Add fixture legend to layout"));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_EventTable,
                          _("Add Event Table"), "table", wxART_LIST_VIEW,
                          _("Add event table to layout"));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Text, _("Add Text"),
                          "text-select", wxART_TIP,
                          _("Add text box to layout"));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Image, _("Add Image"),
                          "image-plus", wxART_MISSING_IMAGE,
                          _("Add image to layout"));
  layoutToolBar->Realize();
  auiManager->AddPane(layoutToolBar, wxAuiPaneInfo()
                                         .Name("LayoutToolbar")
                                         .Caption(_("Layout"))
                                         .ToolbarPane()
                                         .Top());

  UpdateToolBarAvailability();
}

// Builds and assigns the main application menu bar.
void MainWindow::CreateMenuBar() { SetMenuBar(BuildMainWindowMenuBar()); }

// Starts a new project after guarding startup and save state.
void MainWindow::OnNew(wxCommandEvent &WXUNUSED(event)) {
  if (!GuardStartupProjectLoadAction(_("creating a new project")))
    return;

  if (!ConfirmSaveIfDirty(_("creating a new project"), _("New Project")))
    return;

  ResetProject(true);
}

// Opens the GDTF search flow, refreshing the remote catalog when credentials
// are available. Opens the GDTF download workflow and optionally adds the
// selected fixture.
void MainWindow::OnDownloadGdtf(wxCommandEvent &WXUNUSED(event)) {
  diagnostics::DiagnosticLogger::Info("GDTF download workflow opened.");
  ConfigManager &configManager =
      GetDefaultGuiConfigServices().LegacyConfigManager();
  CredentialStore::LoadResult loadedCredentials =
      LoadGdtfCredentialsForGuiDetailed(configManager);
  std::optional<CredentialStore::Credentials> activeCredentials =
      loadedCredentials.credentials;

  GdtfShareClient gdtfClient;
  gdtf_share_workflow::WorkflowState gdtfWorkflowState;
  gdtfWorkflowState.credentialAvailability =
      gdtf_share_workflow::DetermineCredentialAvailability(loadedCredentials);

  const auto catalogResolveStart = std::chrono::steady_clock::now();
  GdtfCatalogService catalogService;
  const std::string nowUtc =
      WxToUtf8(wxDateTime::UNow().FormatISOCombined(' '));

  std::unique_ptr<wxWindowDisabler> refreshDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> refreshOverlay =
      std::make_unique<wxBusyInfo>("Updating GDTF catalog...");
  wxYieldIfNeeded();

  GdtfShareResult initialLoginResult;
  GdtfShareResult initialCatalogResult;
  bool initialLoginAttempted = false;
  const GdtfCatalogRefreshResult catalogResult =
      catalogService.RefreshCatalogIfStale(
          [&](std::string &onlineListData) {
            if (!activeCredentials || activeCredentials->username.empty() ||
                activeCredentials->password.empty()) {
              return false;
            }

            initialLoginAttempted = true;
            initialLoginResult = gdtfClient.Login(activeCredentials->username,
                                                  activeCredentials->password);
            gdtfWorkflowState.lastAuthenticationResult = initialLoginResult;
            if (!initialLoginResult.Succeeded())
              return false;
            gdtfWorkflowState.sessionAuthenticated = true;
            gdtfWorkflowState.authenticatedUsername = activeCredentials->username;

            initialCatalogResult = gdtfClient.GetCatalog();
            gdtfWorkflowState.lastCatalogResult = initialCatalogResult;
            onlineListData = initialCatalogResult.payload;
            return initialCatalogResult.Succeeded() && !onlineListData.empty();
          },
          nowUtc, 0);

  refreshOverlay.reset();
  refreshDisabler.reset();

  const auto catalogResolveElapsedMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - catalogResolveStart)
          .count();

  std::string effectiveListData = "{}";
  std::string effectiveUpdatedAt = "unknown";
  if (catalogResult.snapshot) {
    effectiveListData = catalogResult.snapshot->listData;
    effectiveUpdatedAt = catalogResult.snapshot->updatedAt;
    gdtfWorkflowState.catalogSource = catalogResult.metrics.refreshSucceeded
        ? gdtf_share_workflow::CatalogSource::Online
        : gdtf_share_workflow::CatalogSource::Cached;
  }
  diagnostics::DiagnosticLogger::Info(
      std::string("GDTF catalog resolved: cache_hit=") +
      (catalogResult.metrics.cacheHit ? "true" : "false") +
      " refresh_attempted=" +
      (catalogResult.metrics.refreshAttempted ? "true" : "false") +
      " refresh_succeeded=" +
      (catalogResult.metrics.refreshSucceeded ? "true" : "false") +
      " credential_state=" +
      gdtf_share_workflow::CredentialAvailabilityName(
          gdtfWorkflowState.credentialAvailability) +
      " catalog_source=" +
      gdtf_share_workflow::CatalogSourceName(gdtfWorkflowState.catalogSource) +
      " session_authenticated=" +
      (gdtfWorkflowState.sessionAuthenticated ? "true" : "false"));

  if (consolePanel) {
    consolePanel->AppendMessage(wxString::Format(
        "[METRIC] GDTF catalog cache_hit=%d cache_miss=%d cache_age_s=%lld "
        "refresh_attempted=%d refresh_succeeded=%d resolve_ms=%lld",
        catalogResult.metrics.cacheHit ? 1 : 0,
        catalogResult.metrics.cacheMiss ? 1 : 0,
        static_cast<long long>(catalogResult.metrics.cacheAgeSeconds),
        catalogResult.metrics.refreshAttempted ? 1 : 0,
        catalogResult.metrics.refreshSucceeded ? 1 : 0,
        static_cast<long long>(catalogResolveElapsedMs)));
  }

  std::unique_ptr<wxBusyInfo> preparingCatalogOverlay =
      std::make_unique<wxBusyInfo>("Loading GDTF catalog...");
  wxYieldIfNeeded();
  GdtfSearchDialog searchDlg(
      this, effectiveListData, effectiveUpdatedAt, nullptr,
      gdtfWorkflowState.catalogSource == gdtf_share_workflow::CatalogSource::Online
          ? GdtfCatalogDisplaySource::Online
          : (gdtfWorkflowState.catalogSource == gdtf_share_workflow::CatalogSource::Cached
                 ? GdtfCatalogDisplaySource::Cached
                 : GdtfCatalogDisplaySource::None),
      !gdtfWorkflowState.sessionAuthenticated);
  preparingCatalogOverlay.reset();

  const int searchDialogResult = searchDlg.ShowModal();

  if (searchDialogResult == wxID_OK) {

    std::unique_ptr<wxWindowDisabler> gdtfDownloadDisabler =
        std::make_unique<wxWindowDisabler>();
    std::unique_ptr<wxBusyInfo> gdtfDownloadBusyOverlay;
    auto clearGdtfDownloadBlockingUi = [&]() {
      gdtfDownloadBusyOverlay.reset();
      gdtfDownloadDisabler.reset();
    };
    auto updateGdtfDownloadBusyOverlay = [&](const wxString &message) {
      gdtfDownloadBusyOverlay = std::make_unique<wxBusyInfo>(message);
      wxYieldIfNeeded();
    };
    auto showGdtfDownloadError = [&](const wxString &message,
                                     const wxString &caption) {
      clearGdtfDownloadBlockingUi();
      wxMessageBox(message, caption, wxOK | wxICON_ERROR, this);
    };
    auto requestCredentialsFromDialog = [&]() -> bool {
      const std::string initialUser =
          activeCredentials ? activeCredentials->username :
          loadedCredentials.usernameHint.value_or(std::string());
      const std::string initialPassword =
          activeCredentials ? activeCredentials->password : std::string();

      clearGdtfDownloadBlockingUi();
      GdtfLoginDialog loginDlg(this, initialUser, initialPassword);
      if (loginDlg.ShowModal() != wxID_OK)
        return false;

      gdtfDownloadDisabler = std::make_unique<wxWindowDisabler>();

      CredentialStore::Credentials dialogCredentials;
      dialogCredentials.username = WxToUtf8(
          wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false));
      dialogCredentials.password = loginDlg.GetPassword();

      if (dialogCredentials.username.empty() ||
          dialogCredentials.password.empty()) {
        showGdtfDownloadError("Please provide username and password.",
                              "Login Error");
        return false;
      }

      activeCredentials = dialogCredentials;
      return true;
    };
    auto showPersistenceWarning = [&](const CredentialStore::Result &result) {
      if (result.Succeeded())
        return;
      const wxString message = result.status == CredentialStore::Status::SecureStoreUnavailable
          ? _("The username was saved, but secure password storage is unavailable. The password must be entered again after restart.")
          : wxString::Format(_("GDTF Share credentials were authenticated for this operation, but were not saved (%s)."),
                             wxString::FromUTF8(CredentialStore::StatusName(result.status)));
      wxMessageBox(message, _("GDTF Share credentials"), wxOK | wxICON_WARNING, this);
    };
    auto ensureAuthenticated = [&](GdtfShareResult &loginResult) -> bool {
      if (activeCredentials && gdtfClient.IsAuthenticated() &&
          gdtfClient.AuthenticatedUsername() == activeCredentials->username) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth: reused_existing_session=true");
        return true;
      }
      if (!activeCredentials || activeCredentials->username.empty() ||
          activeCredentials->password.empty()) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=missing_credentials");
        if (!requestCredentialsFromDialog())
          return false;
      }
      if (!activeCredentials)
        return false;
      if (initialLoginAttempted &&
          initialLoginResult.category == GdtfShareResultCategory::AuthenticationRejected) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=rejected_credentials");
        if (!requestCredentialsFromDialog())
          return false;
      }
      gdtfClient.ResetSession();
      if (consolePanel)
        consolePanel->AppendMessage(
            "[INFO] Logging into GDTF Share using libcurl");
      updateGdtfDownloadBusyOverlay("Logging in to GDTF Share...");
      loginResult = gdtfClient.Login(activeCredentials->username,
                                     activeCredentials->password);
      gdtfWorkflowState.lastAuthenticationResult = loginResult;
      gdtfWorkflowState.sessionAuthenticated = loginResult.Succeeded();
      if (loginResult.Succeeded()) {
        gdtfWorkflowState.authenticatedUsername = activeCredentials->username;
        const CredentialStore::Result persistResult =
            PersistGdtfCredentialsForGui(*activeCredentials, configManager);
        loadedCredentials.usernameHint = activeCredentials->username;
        loadedCredentials.credentials = activeCredentials;
        showPersistenceWarning(persistResult);
      } else if (loginResult.category == GdtfShareResultCategory::AuthenticationRejected) {
        diagnostics::DiagnosticLogger::Info(
            "GDTF download auth prompt reason=rejected_credentials");
        if (!requestCredentialsFromDialog())
          return false;
        gdtfClient.ResetSession();
        loginResult = gdtfClient.Login(activeCredentials->username,
                                       activeCredentials->password);
        gdtfWorkflowState.lastAuthenticationResult = loginResult;
        gdtfWorkflowState.sessionAuthenticated = loginResult.Succeeded();
        if (loginResult.Succeeded()) {
          gdtfWorkflowState.authenticatedUsername = activeCredentials->username;
          const CredentialStore::Result persistResult =
              PersistGdtfCredentialsForGui(*activeCredentials, configManager);
          loadedCredentials.usernameHint = activeCredentials->username;
          loadedCredentials.credentials = activeCredentials;
          showPersistenceWarning(persistResult);
        }
      }
      return loginResult.Succeeded();
    };

    GdtfShareResult loginResult;
    if (!ensureAuthenticated(loginResult)) {
      showGdtfDownloadError(wxString::FromUTF8(
                                FormatGdtfShareUserMessage(loginResult, "login")),
                            "Login Error");
      return;
    }

    wxString rid = wxString::FromUTF8(searchDlg.GetSelectedId());
    wxString name = wxString::FromUTF8(searchDlg.GetSelectedName());

    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    wxFileDialog saveDlg(this, "Save GDTF file", fixDir, name + ".gdtf",
                         "*.gdtf", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    clearGdtfDownloadBlockingUi();
    if (saveDlg.ShowModal() == wxID_OK) {
      wxString dest = saveDlg.GetPath();
      diagnostics::DiagnosticLogger::Info(
          "GDTF download selected: " +
          diagnostics::DiagnosticLogger::FileNameOnly(WxToUtf8(dest)));
      if (!rid.empty()) {
        gdtfDownloadDisabler = std::make_unique<wxWindowDisabler>();
        updateGdtfDownloadBusyOverlay("Downloading GDTF from GDTF Share...");
        if (consolePanel)
          consolePanel->AppendMessage("[INFO] Downloading via libcurl rid=" +
                                      rid);
        GdtfShareResult downloadResult =
            gdtfClient.DownloadRevision(WxToUtf8(rid), WxToUtf8(dest));
        if (downloadResult.category == GdtfShareResultCategory::AuthenticationRejected) {
          diagnostics::DiagnosticLogger::Info(
              "GDTF download auth prompt reason=expired_session");
          activeCredentials.reset();
          if (requestCredentialsFromDialog()) {
            gdtfClient.ResetSession();
            GdtfShareResult retryLogin;
            if (ensureAuthenticated(retryLogin)) {
              downloadResult =
                  gdtfClient.DownloadRevision(WxToUtf8(rid), WxToUtf8(dest));
            } else {
              downloadResult = retryLogin;
            }
          }
        }
        long dlCode = downloadResult.httpStatus;
        bool ok = downloadResult.Succeeded();
        clearGdtfDownloadBlockingUi();
        if (consolePanel)
          consolePanel->AppendMessage(
              wxString::Format("[INFO] Download HTTP code: %ld", dlCode));
        if (ok && dlCode == 200) {
          diagnostics::DiagnosticLogger::Info(
              "GDTF download completed: " +
              diagnostics::DiagnosticLogger::FileNameOnly(WxToUtf8(dest)));
          int addNow =
              wxMessageBox("GDTF downloaded successfully. Do you want to add "
                           "it to the project now?",
                           "Success", wxYES_NO | wxICON_QUESTION, this);
          if (addNow == wxYES)
            AddFixtureFromGdtfPath(WxToUtf8(dest));
        } else {
          diagnostics::DiagnosticLogger::Error("GDTF download failed: http=" +
                                               std::to_string(dlCode));
          wxMessageBox(wxString::FromUTF8(FormatGdtfShareUserMessage(downloadResult, "download")), "Error",
                       wxOK | wxICON_ERROR);
        }
      } else {
        wxMessageBox("Download information missing.", "Error",
                     wxOK | wxICON_ERROR);
      }
    }
  }

}

// Opens the dictionary editor dialog.
void MainWindow::OnEditDictionaries(wxCommandEvent &WXUNUSED(event)) {
  DictionaryEditDialog dlg(this);
  dlg.ShowModal();
}

// Opens the writable user library folder in the system file browser.
void MainWindow::OnOpenUserLibraryFolder(wxCommandEvent &WXUNUSED(event)) {
  const std::string fixturesPath =
      ProjectUtils::GetWritableLibraryPath("fixtures");
  if (fixturesPath.empty()) {
    wxMessageBox("Could not resolve writable user library path.",
                 "Open user library folder", wxOK | wxICON_ERROR);
    return;
  }

  const std::filesystem::path libraryRoot =
      PathUtils::PathFromUtf8(fixturesPath).parent_path();
  const std::u8string folderPathUtf8 = libraryRoot.u8string();
  const std::string folderPathBytes(folderPathUtf8.begin(),
                                    folderPathUtf8.end());
  const wxString folderPath = wxString::FromUTF8(folderPathBytes.c_str());
  if (!wxLaunchDefaultApplication(folderPath)) {
    wxMessageBox("Could not open the user library folder.",
                 "Open user library folder", wxOK | wxICON_ERROR);
  }
}

// Automatically assigns DMX patch values to fixtures.
void MainWindow::OnAutoPatch(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("auto patch");
  const std::vector<std::string> selectedFixtureUuids =
      cfg.GetSelectedFixtures();
  if (!selectedFixtureUuids.empty())
    AutoPatcher::AutoPatchSelection(cfg.GetScene(), selectedFixtureUuids);
  else
    AutoPatcher::AutoPatch(cfg.GetScene());
  RefreshAfterSceneChange();
}

// Distributes rigging weight totals across selected hoists.
void MainWindow::OnDistributeHoistWeights(wxCommandEvent &WXUNUSED(event)) {
  SyncSceneData();
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();

  auto normalizePosition = [](const std::string &positionName) {
    return positionName.empty() ? std::string("Unassigned") : positionName;
  };

  std::set<std::string> hoistPositions;
  for (const auto &[uuid, support] : scene.supports) {
    (void)uuid;
    hoistPositions.insert(normalizePosition(support.positionName));
  }

  if (hoistPositions.empty()) {
    wxMessageBox("No hoists available for weight distribution.",
                 "Distribute hoist weights", wxOK | wxICON_INFORMATION);
    return;
  }

  wxArrayString choices;
  choices.Add("All positions");
  for (const std::string &positionName : hoistPositions)
    choices.Add(wxString::FromUTF8(positionName));

  wxSingleChoiceDialog dialog(
      this, "Select hang position(s) to distribute hoist weights.",
      "Distribute hoist weights", choices);
  dialog.SetSelection(0);
  if (dialog.ShowModal() != wxID_OK)
    return;

  const wxString selectedChoice = dialog.GetStringSelection();
  const bool applyAllPositions = selectedChoice == "All positions";
  const std::string selectedPosition = WxToUtf8(selectedChoice);

  std::vector<std::string> selectedSupportUuids;
  selectedSupportUuids.reserve(scene.supports.size());
  for (const auto &[uuid, support] : scene.supports) {
    if (applyAllPositions ||
        normalizePosition(support.positionName) == selectedPosition) {
      selectedSupportUuids.push_back(uuid);
    }
  }

  if (selectedSupportUuids.empty()) {
    wxMessageBox("No hoists found for the selected position.",
                 "Distribute hoist weights", wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.PushUndoState("distribute hoist weights");
  const auto extraWeights = RiggingExtraWeightSettings::ParseEntries(
      cfg.GetValue(RiggingExtraWeightSettings::ConfigKey()));
  const auto roundedTotalsByPosition =
      HoistWeightDistribution::BuildRoundedRiggingTotalByHangPosition(
          scene,
          RiggingExtraWeightSettings::BuildKilogramsByPosition(extraWeights));
  HoistWeightDistribution::ApplyForImportedSupports(scene, selectedSupportUuids,
                                                    roundedTotalsByPosition);
  RefreshAfterSceneChange();
}

// Auto-assigns layer and fixture colors based on scene state.
void MainWindow::OnAutoColor(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("auto color");
  auto &scene = cfg.GetScene();
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);
  auto randHex = [&]() {
    return wxString::Format("#%02X%02X%02X", dist(rng), dist(rng), dist(rng))
        .ToStdString();
  };
  auto isWhiteColor = [](const std::string &color) {
    if (color.empty())
      return false;
    std::string normalized = color;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (normalized.rfind('#', 0) == 0)
      normalized.erase(0, 1);
    return normalized == "fff" || normalized == "ffffff" ||
           normalized == "white";
  };
  const std::string trussColor = "#D3D3D3";

  std::set<std::string> layerNames;
  for (const auto &[uuid, layer] : scene.layers)
    layerNames.insert(layer.name);
  for (const auto &[u, f] : scene.fixtures)
    layerNames.insert(f.layer);
  for (const auto &[u, t] : scene.trusses)
    layerNames.insert(t.layer);
  for (const auto &[u, o] : scene.sceneObjects)
    layerNames.insert(o.layer);
  layerNames.insert(DEFAULT_LAYER_NAME);

  for (const auto &name : layerNames) {
    auto current = cfg.GetLayerColor(name);
    if (!current || current->empty()) {
      std::string lower = name;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      std::string c = lower.rfind("truss", 0) == 0 ? trussColor : randHex();
      cfg.SetLayerColor(name, c);
      if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetLayerColor(name, c);
    }
  }

  const auto selectedFixtureIds = cfg.GetSelectedFixtures();
  const bool hasFixtureSelection = !selectedFixtureIds.empty();
  std::set<std::string> selectedFixtureSet(selectedFixtureIds.begin(),
                                           selectedFixtureIds.end());

  auto buildFixtureGroupKey = [](const auto &fixture) {
    return fixture.gdtfSpec + "\n" + fixture.gdtfMode;
  };

  std::map<std::string, std::string> fixtureGroupColors;
  for (auto &[uuid, f] : scene.fixtures) {
    if (hasFixtureSelection &&
        selectedFixtureSet.find(uuid) == selectedFixtureSet.end())
      continue;

    if (hasFixtureSelection) {
      if (f.gdtfSpec.empty()) {
        f.visualColorHex = randHex();
        continue;
      }
      const std::string groupKey = buildFixtureGroupKey(f);
      auto existing = fixtureGroupColors.find(groupKey);
      if (existing == fixtureGroupColors.end()) {
        const auto dictColor = GdtfDictionary::GetDefaultVisualColorForFixture(
            f.typeName, f.gdtfSpec, f.gdtfMode);
        const std::string chosenColor = dictColor ? *dictColor : randHex();
        fixtureGroupColors[groupKey] = chosenColor;
        f.visualColorHex = chosenColor;
      } else {
        f.visualColorHex = existing->second;
      }
      continue;
    }

    if (!f.gdtfSpec.empty()) {
      auto it = fixtureGroupColors.find(buildFixtureGroupKey(f));
      if (it == fixtureGroupColors.end()) {
        const std::string c =
            (f.visualColorHex.empty() || isWhiteColor(f.visualColorHex))
                ? randHex()
                : f.visualColorHex;
        fixtureGroupColors[buildFixtureGroupKey(f)] = c;
        f.visualColorHex = c;
      } else {
        f.visualColorHex = it->second;
      }
    } else if (hasFixtureSelection || f.visualColorHex.empty() ||
               isWhiteColor(f.visualColorHex)) {
      f.visualColorHex = randHex();
    }
  }

  if (layerPanel)
    layerPanel->ReloadLayers();
  RefreshAfterSceneChange();
}

// Converts selected fixtures into hoist supports.
void MainWindow::OnConvertToHoist(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto selected = cfg.GetSelectedFixtures();
  if (selected.empty()) {
    wxMessageBox("Please select fixtures to convert first.", "Convert to Hoist",
                 wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.PushUndoState("convert fixtures to hoists");
  auto &scene = cfg.GetScene();

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  int idx = 0;
  std::vector<std::string> newIds;
  for (const auto &uuid : selected) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;

    const auto &fixture = it->second;

    Support s;
    s.uuid =
        wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId), idx++)
            .ToStdString();
    s.name = fixture.instanceName;
    s.gdtfSpec = fixture.gdtfSpec;
    s.gdtfMode = fixture.gdtfMode;
    s.function = fixture.function.empty() ? "Hoist" : fixture.function;
    s.chainLength = 0.0f;
    s.position = fixture.position;
    s.positionName = fixture.positionName;
    s.layer = fixture.layer;
    s.capacityKg = 0.0f;
    s.weightKg = 0.0f;
    s.loadKg = fixture.weightKg;
    s.motorName =
        fixture.instanceName.empty() ? fixture.typeName : fixture.instanceName;
    s.motorModel = fixture.gdtfMode;
    s.motorFixtureUuid = fixture.uuid;
    s.hoistDataSource = "Manual";
    s.hoistFunction = NormalizeHoistFunction(s.function);
    s.transform = fixture.transform;

    scene.supports[s.uuid] = s;
    newIds.push_back(s.uuid);
  }

  for (const auto &uuid : selected)
    scene.fixtures.erase(uuid);

  cfg.SetSelectedSupports(newIds);
  cfg.SetSelectedFixtures({});

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
  RefreshRigging();

  wxMessageBox(
      wxString::Format("Converted %zu fixture(s) to hoists.", newIds.size()),
      "Convert to Hoist", wxOK | wxICON_INFORMATION);
}


// Converts selected scene objects sharing the same model file into trusses.
void MainWindow::OnConvertSceneObjectsToTruss(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto selected = cfg.GetSelectedSceneObjects();
  if (selected.empty()) {
    wxMessageBox("Please select a scene object to convert first.",
                 "Convert Scene Objects to Truss", wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.PushUndoState("convert scene objects to trusses");
  auto &scene = cfg.GetScene();
  const SceneObjectToTrussConversionResult result =
      ConvertSceneObjectsWithSameModelToTrusses(scene, selected.front());
  if (result.convertedUuids.empty()) {
    wxMessageBox("No scene objects with a valid model file were converted.",
                 "Convert Scene Objects to Truss", wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.SetSelectedSceneObjects({});
  cfg.SetSelectedTrusses(result.convertedUuids);

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  RefreshSummary();
  RefreshRigging();

  wxMessageBox(wxString::Format("Converted %zu scene object(s) with model '%s' to truss.",
                                result.convertedUuids.size(),
                                wxString::FromUTF8(result.modelFile).c_str()),
               "Convert Scene Objects to Truss", wxOK | wxICON_INFORMATION);
}

// Runs the fixture symbol generation tool when the feature is enabled.
void MainWindow::OnGenerateFixtureSymbols(wxCommandEvent &WXUNUSED(event)) {
  if (!ui::IsFeatureEnabled(ui::FeatureFlag::GenerateFixtureSymbols))
    return;

  tools::RunFixtureSymbolGeneration(*this);
}

// Runs automatic category assignment for selected fixtures when enabled.
void MainWindow::OnAssignSelectedFixtureCategory(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ui::IsFeatureEnabled(ui::FeatureFlag::AssignSelectedFixtureCategory)) {
    return;
  }

  tools::RunFixtureCategoryAssignment(*this);
}

// Initiates window closing through the standard close flow.
void MainWindow::OnClose(wxCommandEvent &event) {
  // Allow the close event to be vetoed when the user chooses Cancel
  Close(false);
}

// Persists state, stops live workers, and detaches IO callbacks before
// destroying the main window. Finalizes shutdown by saving state and stopping
// runtime services.
void MainWindow::OnCloseWindow(wxCloseEvent &event) {
  SaveUserConfigWithViewport2DState();
  if (!ConfirmSaveIfDirty(_("exiting"), _("Exit"))) {
    event.Veto();
    return;
  }

  userConfigPersistedOnClose = true;

  if (viewportPanel)
    viewportPanel->StopRefreshThread();
  viewportPanel = nullptr;
  ioController.reset();

  Destroy();
}

// Toggles visibility of the console panel.
void MainWindow::OnToggleConsole(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleConsole(event);
}

// Toggles visibility of the fixtures panel.
void MainWindow::OnToggleFixtures(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleFixtures(event);
}

// Toggles visibility of the 3D viewport panel.
void MainWindow::OnToggleViewport(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleViewport(event);
}

// Toggles visibility of the 2D viewport panel.
void MainWindow::OnToggleViewport2D(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleViewport2D(event);
}

// Toggles visibility of the 2D render options panel.
void MainWindow::OnToggleRender2D(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleRender2D(event);
}

// Toggles visibility of the layers panel.
void MainWindow::OnToggleLayers(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleLayers(event);
}

// Toggles visibility of the layouts panel.
void MainWindow::OnToggleLayouts(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleLayouts(event);
}

// Toggles visibility of the summary panel.
void MainWindow::OnToggleSummary(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleSummary(event);
}

// Toggles visibility of the rigging panel.
void MainWindow::OnToggleRigging(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleRigging(event);
}

// Displays the help dialog from the packaged markdown documentation.
void MainWindow::OnShowHelp(wxCommandEvent &WXUNUSED(event)) {
  // Attempt to load the Markdown help file located alongside the executable.
  wxFileName helpPath(wxStandardPaths::Get().GetExecutablePath());
  helpPath.SetFullName("help.md");
  if (helpPath.Exists()) {
    // Read the file contents.
    std::ifstream in(WxToUtf8(helpPath.GetFullPath()));
    std::string markdown((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    HelpMarkdown help = SplitHelpMarkdown(markdown);

    // Create a resizable dialog containing a wxHtmlWindow to render the
    // generated HTML.
    const wxSize parentSize = GetSize();
    const wxSize dialogSize(
        std::max(900, static_cast<int>(parentSize.x * 0.85)),
        std::max(700, static_cast<int>(parentSize.y * 0.85)));
    wxDialog dlg(this, wxID_ANY, wxString::FromUTF8("Perastage Help"),
                 wxDefaultPosition, dialogSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *langSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *langLabel = new wxStaticText(&dlg, wxID_ANY, _("Language:"));
    wxChoice *langChoice = new wxChoice(&dlg, wxID_ANY);
    langChoice->Append(wxString::FromUTF8("English"));
    langChoice->Append(wxString::FromUTF8("Español"));
    langChoice->SetSelection(0);
    langSizer->Add(langLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    langSizer->Add(langChoice, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(langSizer, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    wxHtmlWindow *htmlWin = new wxHtmlWindow(
        &dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO);

    auto setHelpPage = [&](const std::string &markdownBody) {
      std::string html = MarkdownToHtml(markdownBody);
      std::string wrapped = WrapHelpHtml(html);
      htmlWin->SetPage(wxString::FromUTF8(wrapped));
    };

    setHelpPage(help.english);
    langChoice->Bind(wxEVT_CHOICE, [&](wxCommandEvent &) {
      if (langChoice->GetSelection() == 1)
        setHelpPage(help.spanish);
      else
        setHelpPage(help.english);
    });

    sizer->Add(htmlWin, 1, wxEXPAND | wxALL, 5);
    dlg.SetSizer(sizer);
    dlg.ShowModal();
  } else {
    wxMessageBox(wxString::FromUTF8("help.md file not found"),
                 wxString::FromUTF8("Perastage Help"), wxOK | wxICON_ERROR,
                 this);
  }
}

// Opens the public Perastage documentation website in the default browser.
void MainWindow::OnOpenOnlineDocumentation(wxCommandEvent &WXUNUSED(event)) {
  wxLaunchDefaultBrowser("https://perastage.luismaperamato.com/");
}

// Opens the local diagnostics folder in the system file explorer.
void MainWindow::OnOpenLogsFolder(wxCommandEvent &WXUNUSED(event)) {
  std::string error;
  const std::filesystem::path logsDirectory =
      diagnostics::DiagnosticPaths::LogsDirectory();
  if (!diagnostics::DiagnosticPaths::EnsureDirectory(logsDirectory, &error)) {
    diagnostics::DiagnosticLogger::Error("Unable to open logs folder: " +
                                         error);
    wxMessageBox(
        wxString::FromUTF8("Could not create the logs folder.\n\n" + error),
        wxString::FromUTF8("Perastage Diagnostics"), wxOK | wxICON_ERROR, this);
    return;
  }

  diagnostics::DiagnosticLogger::Info("Open logs folder requested.");
  const wxString quotedPath =
      "\"" + wxString::FromUTF8(logsDirectory.string()) + "\"";
#if defined(__WXMSW__)
  const wxString command = "explorer " + quotedPath;
#elif defined(__WXOSX__)
  const wxString command = "open " + quotedPath;
#else
  const wxString command = "xdg-open " + quotedPath;
#endif
  const bool launched = wxExecute(command, wxEXEC_ASYNC) != 0;
  if (!launched) {
    wxMessageBox(wxString::FromUTF8("Could not open the logs folder.\n\n" +
                                    logsDirectory.string()),
                 wxString::FromUTF8("Perastage Diagnostics"),
                 wxOK | wxICON_WARNING, this);
  }
}

// Exports a local text diagnostic report for manual sharing.
void MainWindow::OnExportDiagnosticReport(wxCommandEvent &WXUNUSED(event)) {
  diagnostics::DiagnosticLogger::Info("Diagnostic report export requested.");
  std::filesystem::path reportPath;
  std::string error;
  if (!diagnostics::DiagnosticReport::ExportToFile(&reportPath, &error)) {
    diagnostics::DiagnosticLogger::Error("Diagnostic report export failed: " +
                                         error);
    wxMessageBox(wxString::FromUTF8(
                     "Could not export the diagnostic report.\n\n" + error),
                 wxString::FromUTF8("Perastage Diagnostics"),
                 wxOK | wxICON_ERROR, this);
    return;
  }

  diagnostics::DiagnosticLogger::Info("Diagnostic report exported.");
  wxMessageBox(
      wxString::FromUTF8("Diagnostic report exported successfully.\n\n" +
                         reportPath.string()),
      wxString::FromUTF8("Perastage Diagnostics"), wxOK | wxICON_INFORMATION,
      this);
}

// Checks the latest release asynchronously and shows a result dialog with
// update actions.
void MainWindow::OnCheckForUpdates(wxCommandEvent &WXUNUSED(event)) {
  auto busyInfo = std::make_shared<wxBusyInfo>("Checking for updates...");
  auto disabler = std::make_shared<wxWindowDisabler>();

  std::thread([this, busyInfo, disabler]() {
    gui::update::AppUpdateService service;
    const gui::update::CheckResult result = service.CheckForUpdates();

    CallAfter([this, result, busyInfo, disabler]() mutable {
      // Releases busy UI guards before any modal dialog so result popups remain
      // visible.
      busyInfo.reset();
      disabler.reset();

      wxString title = "Perastage Updates";
      if (result.status == gui::update::CheckStatus::CheckFailed) {
        wxString message = "Could not check for updates.\n\n";
        message +=
            result.errorMessage.empty()
                ? wxString::FromUTF8(
                      "Please verify your network connection and try again.")
                : wxString::FromUTF8(result.errorMessage);
        wxMessageBox(message, title, wxOK | wxICON_WARNING, this);
        return;
      }

      const wxString currentVersion = wxString::FromUTF8(result.currentVersion);
      const wxString latestVersion = wxString::FromUTF8(result.latestVersion);
      wxString message = "Current version: " + currentVersion + "\n" +
                         "Latest version: " + latestVersion + "\n\n";

      if (result.status == gui::update::CheckStatus::UpToDate) {
        message += "You are up to date.";
        wxMessageBox(message, title, wxOK | wxICON_INFORMATION, this);
        return;
      }

      const gui::update::UpdateNotificationChoice choice =
          gui::update::ShowAvailableUpdateDialog(this, result, false);
      if (choice.openReleasePage) {
        wxLaunchDefaultBrowser(wxString::FromUTF8(result.releaseUrl));
      }
    });
  }).detach();
}

// Displays the application About dialog.
void MainWindow::OnShowAbout(wxCommandEvent &WXUNUSED(event)) {
  gui::ShowAboutDialog(this);
}

// Switches the notebook to the fixtures tab.
void MainWindow::OnSelectFixtures(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(0);
}

// Switches the notebook to the trusses tab.
void MainWindow::OnSelectTrusses(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(1);
}

// Switches the notebook to the supports tab.
void MainWindow::OnSelectSupports(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(2);
}

// Switches the notebook to the scene objects tab.
void MainWindow::OnSelectObjects(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(3);
}

// Opens the preferences dialog.
void MainWindow::OnPreferences(wxCommandEvent &WXUNUSED(event)) {
  PreferencesDialog dlg(this);
  dlg.ShowModal();
}

// Undoes the last action and refreshes dependent UI panels.
void MainWindow::OnUndo(wxCommandEvent &WXUNUSED(event)) {
  const bool placementUndoHandled =
      (viewport2DPanel && viewport2DPanel->UndoContinuousPlacement()) ||
      (viewportPanel && viewportPanel->UndoContinuousPlacement());
  if (placementUndoHandled) {
    if (consolePanel)
      consolePanel->AppendMessage("Undo continuous placement");
    RefreshSummary();
    return;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  if (!cfg.CanUndo())
    return;
  std::string action = cfg.Undo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Undo" : "Undo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures(), false);
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses(), false);
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports(), false);
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects(), false);
  }
  if (layoutPanel)
    layoutPanel->ReloadLayouts();
  if (!activeLayoutName.empty())
    ActivateLayoutView(activeLayoutName);
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (viewportPanel) {
    std::vector<std::string> mergedSelection;
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedFixtures().begin(),
                           cfg.GetSelectedFixtures().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedTrusses().begin(),
                           cfg.GetSelectedTrusses().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedSupports().begin(),
                           cfg.GetSelectedSupports().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedSceneObjects().begin(),
                           cfg.GetSelectedSceneObjects().end());
    viewportPanel->UpdateScene();
    viewportPanel->SetSelectedFixtures(mergedSelection);
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Redoes the last undone action and refreshes dependent UI panels.
void MainWindow::OnRedo(wxCommandEvent &WXUNUSED(event)) {
  if ((viewport2DPanel && viewport2DPanel->IsContinuousPlacementActive()) ||
      (viewportPanel && viewportPanel->IsContinuousPlacementActive())) {
    return;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  if (!cfg.CanRedo())
    return;
  std::string action = cfg.Redo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Redo" : "Redo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures(), false);
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses(), false);
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports(), false);
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects(), false);
  }
  if (layoutPanel)
    layoutPanel->ReloadLayouts();
  if (!activeLayoutName.empty())
    ActivateLayoutView(activeLayoutName);
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (viewportPanel) {
    std::vector<std::string> mergedSelection;
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedFixtures().begin(),
                           cfg.GetSelectedFixtures().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedTrusses().begin(),
                           cfg.GetSelectedTrusses().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedSupports().begin(),
                           cfg.GetSelectedSupports().end());
    mergedSelection.insert(mergedSelection.end(),
                           cfg.GetSelectedSceneObjects().begin(),
                           cfg.GetSelectedSceneObjects().end());
    viewportPanel->UpdateScene();
    viewportPanel->SetSelectedFixtures(mergedSelection);
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Adds fixtures to the scene from existing types or a selected GDTF file.
void MainWindow::OnAddFixture(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();

  std::string gdtfPath;

  if (!scene.fixtures.empty()) {
    std::map<std::string, std::string> typeToSpec;
    for (const auto &[uuid, f] : scene.fixtures)
      if (!f.typeName.empty() && !f.gdtfSpec.empty())
        typeToSpec.try_emplace(f.typeName, f.gdtfSpec);
    std::vector<std::string> types;
    types.reserve(typeToSpec.size());
    for (const auto &[name, spec] : typeToSpec)
      types.push_back(name);

    SelectFixtureTypeDialog chooseDlg(this, types);
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString fixDir =
          wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
      wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString,
                        "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      AddFixtureFromGdtfPath(WxToUtf8(fdlg.GetPath()));
      return;
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(types.size()))
        return;
      std::string defaultName = types[sel];
      std::string spec = typeToSpec[defaultName];
      namespace fs = std::filesystem;
      if (fs::path(spec).is_absolute())
        gdtfPath = spec;
      else
        gdtfPath = (fs::path(scene.basePath) / spec).string();
      AddFixtureFromGdtfPath(gdtfPath, defaultName);
      return;
    }
  } else {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    AddFixtureFromGdtfPath(WxToUtf8(fdlg.GetPath()));
    return;
  }
}

// Adds one or more trusses to the scene from a selected truss definition.
void MainWindow::OnAddTruss(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;

  if (!scene.trusses.empty()) {
    const std::vector<gui::TrussCreationSource> trussSources =
        gui::CollectTrussCreationSources(scene.trusses, scene.basePath);
    std::vector<std::string> names;
    names.reserve(trussSources.size());
    for (const gui::TrussCreationSource &source : trussSources)
      names.push_back(source.displayName);

    SelectNameDialog chooseDlg(this, names, "Select Truss", "Choose a truss:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString trussDir =
          wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
      wxFileDialog fdlg(
          this, "Select Truss file", trussDir, wxEmptyString,
          wxString::FromUTF8(GetTrussDefinitionFileDialogWildcard()),
          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = WxToUtf8(fn.GetName());
      path = WxToUtf8(fdlg.GetPath());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = trussSources[sel].displayName;
      path = trussSources[sel].definitionPath;
    }
  } else {
    wxString trussDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
    wxFileDialog fdlg(
        this, "Select Truss file", trussDir, wxEmptyString,
        wxString::FromUTF8(GetTrussDefinitionFileDialogWildcard()),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = WxToUtf8(fn.GetName());
    path = WxToUtf8(fdlg.GetPath());
  }

  Truss baseTruss;
  namespace fs = std::filesystem;
  fs::path selectedTrussPath = PathUtils::PathFromUtf8(path);
  std::string selectedExtension = selectedTrussPath.extension().string();
  std::transform(selectedExtension.begin(), selectedExtension.end(),
                 selectedExtension.begin(), [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  Logger::Instance().Log(Logger::Level::Info,
                         "Add truss: selected extension='" + selectedExtension +
                             "' path='" + path + "'.");
  if (!LoadTrussDefinition(path, baseTruss)) {
    Logger::Instance().Log(Logger::Level::Warn,
                           "Add truss validation failed: extension='" +
                               selectedExtension + "' path='" + path + "'.");
    wxMessageBox("Unsupported or unreadable truss file. Supported formats are "
                 "GDTF, GTruss, GLB, and 3DS.",
                 "Error", wxOK | wxICON_ERROR);
    return;
  }
  if (!baseTruss.name.empty())
    defaultName = baseTruss.name;

  AddTrussDialog addDialog(this);
  if (addDialog.ShowModal() != wxID_OK)
    return;
  const AddTrussRequest addRequest = addDialog.GetRequest();
  const bool continuousPlacement = addRequest.continuousPlacement;
  const long qty = continuousPlacement ? 1 : addRequest.quantity;
  if (qty <= 0)
    return;

  cfg.PushUndoState("add truss");
  const std::string base = scene.basePath;
  baseTruss.symbolFile = gui::MakeSceneRelativeResourcePathOrOriginal(
      base, baseTruss.symbolFile, "Add truss symbol path");
  if (!baseTruss.modelFile.empty()) {
    baseTruss.modelFile = gui::MakeSceneRelativeResourcePathOrOriginal(
        base, baseTruss.modelFile, "Add truss model path");
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  std::vector<std::string> addedTrussUuids;
  addedTrussUuids.reserve(static_cast<size_t>(qty));
  const float trussSpacingMm =
      baseTruss.lengthMm > 0.0f ? baseTruss.lengthMm : 0.0f;
  for (long i = 0; i < qty; ++i) {
    Truss t = baseTruss;
    t.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                 .ToStdString();
    if (qty > 1)
      t.name = defaultName + " " + std::to_string(i + 1);
    else
      t.name = defaultName;
    t.layer = layerName;
    t.parentGroupUuid.clear();
    t.hasLocalTransform = false;
    t.transform.o = {addRequest.insertionPointMm[0] + trussSpacingMm * i,
                     addRequest.insertionPointMm[1],
                     addRequest.insertionPointMm[2]};
    t.localTransform = t.transform;
    scene.trusses[t.uuid] = t;
    addedTrussUuids.push_back(t.uuid);
  }

  if (addRequest.createGroup && addedTrussUuids.size() > 1) {
    scene_grouping::ObjectSelection selection;
    selection.trusses = addedTrussUuids;
    scene_grouping::GroupSelection(scene, selection);
  }

  if (trussPanel)
    trussPanel->ReloadData();
  const bool updateSceneRequested = viewportPanel != nullptr;
  Logger::Instance().Log(
      Logger::Level::Info,
      "Add truss complete: extension='" + selectedExtension +
          "' parsingSucceeded=true symbolFile='" + baseTruss.symbolFile +
          "' dimensionsMm=" + std::to_string(baseTruss.lengthMm) + "x" +
          std::to_string(baseTruss.widthMm) + "x" +
          std::to_string(baseTruss.heightMm) + " updateSceneRequested=" +
          (updateSceneRequested ? std::string("true") : std::string("false")) +
          ".");
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (continuousPlacement && !addedTrussUuids.empty()) {
    if (viewport2DPanel && viewport2DPanel->IsShownOnScreen()) {
      viewport2DPanel->BeginContinuousPlacement(ContinuousPlacementType::Truss,
                                                addedTrussUuids.front());
    } else if (viewportPanel && viewportPanel->IsShownOnScreen()) {
      viewportPanel->BeginContinuousPlacement(ContinuousPlacementType::Truss,
                                              addedTrussUuids.front());
    }
  }
  RefreshSummary();
}

// Converts imported OBJ files into GLB assets so MVR exports always reference
// official GLB resources.
std::string
NormalizeImportedObjectModelPathToGlb(const std::string &selectedPath,
                                      const std::string &sceneBasePath,
                                      std::string &consoleError) {
  namespace fs = std::filesystem;
  fs::path sourcePath = PathUtils::PathFromUtf8(selectedPath);
  std::string ext = sourcePath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (ext != ".obj")
    return selectedPath;

  std::error_code ec;
  const fs::path tempRoot = fs::temp_directory_path(ec);
  const fs::path importTempDir = tempRoot / "perastage_imported_objects";
  fs::create_directories(importTempDir, ec);
  if (ec) {
    consoleError =
        "Failed preparing temporary directory for OBJ import conversion";
    return selectedPath;
  }

  const std::string uniqueSuffix = std::to_string(static_cast<long long>(
      std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::string targetName =
      sourcePath.stem().string() + "_" + uniqueSuffix + ".glb";
  const fs::path targetPath = importTempDir / targetName;

  std::string conversionError;
  if (!ConvertObjFileToGlb(sourcePath.string(), targetPath.string(),
                           &conversionError)) {
    consoleError = "Failed converting OBJ to GLB: " + conversionError;
    return selectedPath;
  }

  if (!sceneBasePath.empty()) {
    const fs::path absTarget = fs::weakly_canonical(targetPath, ec);
    const fs::path absBase =
        fs::weakly_canonical(PathUtils::PathFromUtf8(sceneBasePath), ec);
    if (!ec && !absTarget.empty() && !absBase.empty() &&
        absTarget.string().rfind(absBase.string(), 0) == 0) {
      return fs::relative(absTarget, absBase, ec).string();
    }
  }

  return targetPath.string();
}

// Adds one or more scene objects to the scene from a selected model file.
void MainWindow::OnAddSceneObject(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;
  const SceneObject *templateObject = nullptr;

  if (!scene.sceneObjects.empty()) {
    std::map<std::string, const SceneObject *> nameToObject;
    for (const auto &[uuid, o] : scene.sceneObjects)
      if (!o.name.empty() && !o.GetPrimaryModel().empty())
        nameToObject.try_emplace(o.name, &o);
    std::vector<std::string> names;
    names.reserve(nameToObject.size());
    for (const auto &[n, _] : nameToObject)
      names.push_back(n);

    SelectNameDialog chooseDlg(this, names, "Select Scene Object",
                               "Choose an object:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString objDir = wxString::FromUTF8(
          ProjectUtils::GetWritableLibraryPath("scene_objects"));
      wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString,
                        "3D Models (*.3ds;*.glb;*.obj)|*.3ds;*.glb;*.obj",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = WxToUtf8(fn.GetName());
      path = WxToUtf8(fdlg.GetPath());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = names[sel];
      auto objectIt = nameToObject.find(defaultName);
      if (objectIt == nameToObject.end() || objectIt->second == nullptr)
        return;
      templateObject = objectIt->second;
      path = templateObject->GetPrimaryModel();
    }
  } else {
    wxString objDir = wxString::FromUTF8(
        ProjectUtils::GetWritableLibraryPath("scene_objects"));
    wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString,
                      "3D Models (*.3ds;*.glb;*.obj)|*.3ds;*.glb;*.obj",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = WxToUtf8(fn.GetName());
    path = WxToUtf8(fdlg.GetPath());
  }

  AddSceneObjectDialog addDialog(this);
  if (addDialog.ShowModal() != wxID_OK)
    return;
  const AddSceneObjectRequest addRequest = addDialog.GetRequest();
  const bool continuousPlacement = addRequest.continuousPlacement;
  const long qty = continuousPlacement ? 1 : addRequest.quantity;
  if (qty <= 0)
    return;

  namespace fs = std::filesystem;
  cfg.PushUndoState("add scene object");
  std::string base = scene.basePath;
  const bool useTemplateGeometry =
      templateObject != nullptr && !templateObject->geometries.empty();
  if (!useTemplateGeometry) {
    std::string conversionError;
    path = NormalizeImportedObjectModelPathToGlb(path, base, conversionError);
    if (!conversionError.empty() && ConsolePanel::Instance())
      ConsolePanel::Instance()->AppendMessage(
          wxString::FromUTF8(conversionError));
    if (!base.empty()) {
      fs::path abs = fs::absolute(path);
      fs::path b = fs::absolute(base);
      if (abs.string().rfind(b.string(), 0) == 0)
        path = fs::relative(abs, b).string();
    }
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  std::vector<std::string> addedObjectUuids;
  addedObjectUuids.reserve(static_cast<size_t>(qty));
  for (long i = 0; i < qty; ++i) {
    SceneObject obj;
    obj.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                   .ToStdString();
    if (qty > 1)
      obj.name = defaultName + " " + std::to_string(i + 1);
    else
      obj.name = defaultName;
    if (useTemplateGeometry) {
      obj.geometries = templateObject->geometries;
      obj.modelFile = templateObject->modelFile;
    } else {
      obj.modelFile = path;
    }
    obj.layer = layerName;
    scene.sceneObjects[obj.uuid] = obj;
    addedObjectUuids.push_back(obj.uuid);
  }

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (continuousPlacement && !addedObjectUuids.empty()) {
    if (viewport2DPanel && viewport2DPanel->IsShownOnScreen()) {
      viewport2DPanel->BeginContinuousPlacement(
          ContinuousPlacementType::SceneObject, addedObjectUuids.front());
    } else if (viewportPanel && viewportPanel->IsShownOnScreen()) {
      viewportPanel->BeginContinuousPlacement(
          ContinuousPlacementType::SceneObject, addedObjectUuids.front());
    }
  }
  RefreshSummary();
}

// Adds sphere primitives to the scene from dialog input.
void MainWindow::OnAddPrimitiveSphere(wxCommandEvent &WXUNUSED(event)) {
  scene_object_primitives::SphereRequest request;
  if (!scene_object_primitives::ShowSphereDialog(this, request))
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("add primitive sphere");
  scene_object_primitives::AddSphereObjects(cfg, request);

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Adds cube primitives to the scene from dialog input.
void MainWindow::OnAddPrimitiveCube(wxCommandEvent &WXUNUSED(event)) {
  scene_object_primitives::CubeRequest request;
  if (!scene_object_primitives::ShowCubeDialog(this, request))
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("add primitive cube");
  scene_object_primitives::AddCubeObjects(cfg, request);

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Adds cylinder primitives to the scene from dialog input.
void MainWindow::OnAddPrimitiveCylinder(wxCommandEvent &WXUNUSED(event)) {
  scene_object_primitives::CylinderRequest request;
  if (!scene_object_primitives::ShowCylinderDialog(this, request))
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("add primitive cylinder");
  scene_object_primitives::AddCylinderObjects(cfg, request);

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Deletes currently selected layout elements or scene entities.
void MainWindow::OnDelete(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();

  if (layoutViewerPanel && layoutViewerPanel->HasFocus() &&
      layoutViewerPanel->DeleteSelectedElement()) {
    return;
  }

  auto ensureFixtureSelection = [&]() {
    if (fixturePanel && fixturePanel->GetSelectedUuids().empty())
      fixturePanel->SelectByUuid(cfg.GetSelectedFixtures(), false);
  };
  auto ensureTrussSelection = [&]() {
    if (trussPanel && trussPanel->GetSelectedUuids().empty())
      trussPanel->SelectByUuid(cfg.GetSelectedTrusses(), false);
  };
  auto ensureSupportSelection = [&]() {
    if (hoistPanel && hoistPanel->GetSelectedUuids().empty())
      hoistPanel->SelectByUuid(cfg.GetSelectedSupports(), false);
  };
  auto ensureObjectSelection = [&]() {
    if (sceneObjPanel && sceneObjPanel->GetSelectedUuids().empty())
      sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects(), false);
  };

  const bool hasAnySelection = !cfg.GetSelectedFixtures().empty() ||
                               !cfg.GetSelectedTrusses().empty() ||
                               !cfg.GetSelectedSupports().empty() ||
                               !cfg.GetSelectedSceneObjects().empty();
  if (hasAnySelection)
    cfg.PushUndoState("delete selected elements");

  if (fixturePanel && !cfg.GetSelectedFixtures().empty()) {
    ensureFixtureSelection();
    fixturePanel->DeleteSelected(false);
  }
  if (trussPanel && !cfg.GetSelectedTrusses().empty()) {
    ensureTrussSelection();
    trussPanel->DeleteSelected(false);
  }
  if (hoistPanel && !cfg.GetSelectedSupports().empty()) {
    ensureSupportSelection();
    hoistPanel->DeleteSelected(false);
  }
  if (sceneObjPanel && !cfg.GetSelectedSceneObjects().empty()) {
    ensureObjectSelection();
    sceneObjPanel->DeleteSelected(false);
  }
}
