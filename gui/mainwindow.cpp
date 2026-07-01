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
#include "diagnostics/DiagnosticLogger.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tinyxml2.h>
#include <wx/aboutdlg.h>
#include <wx/app.h>
#include <wx/artprov.h>
#include <wx/busyinfo.h>
#include <wx/colour.h>
#include <wx/event.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/fontenum.h>
#include <wx/fontmap.h>
#include <wx/html/htmlwin.h>
#include <wx/iconbndl.h>
#include <wx/image.h>
#include <wx/notebook.h>
#include <wx/numdlg.h>
#include <wx/progdlg.h>
#include <wx/settings.h>
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/log.h>
#include <wx/richtext/richtextbuffer.h>
#include <wx/zipstrm.h>

#include "json.hpp"

using json = nlohmann::json;
#include "LayoutManager.h"
#include "Viewer2DPrintSettings.h"
#include "addfixturedialog.h"
#include "app_version.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "credentialstore.h"
#include "dictionaryeditdialog.h"
#include "exportfixturedialog.h"
#include "exportobjectdialog.h"
#include "exporttrussdialog.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "gdtfsearchdialog.h"
#include "guiconfigservices.h"
#include "highlight_status_bar.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "layout2dviewdialog.h"
#include "layout_render_profiler.h"
#include "layout_render_status_notifier.h"
#include "layoutpanel.h"
#include "layouttextutils.h"
#include "layoutviewerpanel.h"
#include "layoutviewerpanel_shared.h"
#include "layoutviewpresets.h"
#include "legendutils.h"
#include "logger.h"
#include "logindialog.h"
#include "magnet_snap.h"
#include "mainwindow_io_controller.h"
#include "mainwindow_layout_controller.h"
#include "mainwindow_print_controller.h"
#include "mainwindow_view_controller.h"
#include "markdown.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "preferencesdialog.h"
#include "print_diagnostics.h"
#include "projectutils.h"
#include "riderimporter.h"
#include "ridertextdialog.h"
#include "riggingpanel.h"
#include "sceneobjecttablepanel.h"
#include "selection_movement_settings.h"
#include "selectfixturetypedialog.h"
#include "selectnamedialog.h"
#include "simplecrypt.h"
#include "splashscreen.h"
#include "summarypanel.h"
#include "support.h"
#include "tableprinter.h"
#include "trussloader.h"
#include "trusstablepanel.h"
#include "units/units.h"
#include "update/app_update_service.h"
#include "update/update_check_preferences.h"
#include "update/update_notification_dialog.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"
#include "viewer2dpdfexporter.h"
#include "viewer2dprintdialog.h"
#include "viewer2drenderpanel.h"
#include "viewer2dstate.h"
#include "viewer3dcontroller.h"
#include "viewer3dpanel.h"
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

wxDEFINE_EVENT(EVT_PROJECT_LOADED, wxCommandEvent);

MainWindow *MainWindow::Instance() { return s_instance; }

void MainWindow::SetInstance(MainWindow *inst) { s_instance = inst; }

namespace {

// Returns true when the status bar text corresponds to fixture symbol
// auto-update progress.
bool IsFixtureSymbolStatusMessage(const wxString &statusText) {
  return statusText.StartsWith("Fixture symbol auto-update");
}

// Formats a world-space position for the status bar using the active distance
// units.
wxString
FormatWorldPositionStatusText(const std::array<float, 3> &positionMeters,
    Units::DistanceUnitSystem distanceUnitSystem) {
  const std::string unitSuffix = Units::DistanceUnitSuffix(distanceUnitSystem);
  std::ostringstream stream;
  stream << "X: "
         << Units::FormatDistanceFromMillimeters(
                static_cast<double>(positionMeters[0]) * 1000.0,
                distanceUnitSystem, Units::ValueFormatContext::Label)
         << " " << unitSuffix << "  Y: "
         << Units::FormatDistanceFromMillimeters(
                static_cast<double>(positionMeters[1]) * 1000.0,
                distanceUnitSystem, Units::ValueFormatContext::Label)
         << " " << unitSuffix << "  Z: "
         << Units::FormatDistanceFromMillimeters(
                static_cast<double>(positionMeters[2]) * 1000.0,
                distanceUnitSystem, Units::ValueFormatContext::Label)
         << " " << unitSuffix;
  return wxString::FromUTF8(stream.str());
}

constexpr const char *kActiveLayoutNameConfigKey = "layout_active_name";

// Reports whether the loaded project contains a layout with the provided name.
bool ProjectContainsLayout(const std::string &layoutName) {
  if (layoutName.empty())
    return false;
  for (const auto &layout :
       layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (layout.name == layoutName)
      return true;
  }
  return false;
}

// Resolves the project layout that should be activated after a project load.
std::string ResolveProjectStartupLayoutName(const ConfigManager &cfg) {
  const std::string savedLayoutName =
      cfg.GetValue(kActiveLayoutNameConfigKey).value_or("");
  if (ProjectContainsLayout(savedLayoutName))
    return savedLayoutName;
  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  if (!layouts.empty())
    return layouts.front().name;
  return {};
}

// Persists missing fixture type auto-colors before scene save and
// synchronization.
void PersistFixtureTypeAutoColors(ConfigManager &configManager) {
  auto &scene = configManager.GetScene();
  for (auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    if (!fixture.visualColorHex.empty())
      continue;

    fixture.visualColorHex =
        Viewer3DController::BuildFixtureTypeAutoColorHex(fixture.gdtfSpec);
  }
}

wxString PathToWxString(const std::filesystem::path &path) {
  const std::u8string utf8 = path.u8string();
  const std::string utf8Str(utf8.begin(), utf8.end());
  return wxString::FromUTF8(utf8Str.c_str());
}

void LogMissingIcon(const std::filesystem::path &path) {
  const wxString message =
      "Main window icon not found at '" + PathToWxString(path) + "'";
  Logger::Instance().Log(Logger::Level::Warn, message.ToStdString());
}

// Builds the default UI font with shared sans-serif face resolution and UTF-8
// encoding.
wxFont BuildDefaultUiFont() {
  wxFont defaultFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  wxString resolvedFaceName =
      layoutviewerpanel::detail::ResolveSharedFontFaceName();
  if (!resolvedFaceName.empty()) {
    defaultFont.SetFaceName(resolvedFaceName);
  }
  if (wxFontMapper::Get() &&
      wxFontMapper::Get()->IsEncodingAvailable(wxFONTENCODING_UTF8)) {
    defaultFont.SetEncoding(wxFONTENCODING_UTF8);
  }
  if (!defaultFont.IsOk()) {
    const int fallbackSize =
        defaultFont.IsOk() ? defaultFont.GetPointSize() : 10;
    if (resolvedFaceName.empty()) {
      defaultFont = wxFont(fallbackSize, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                           wxFONTWEIGHT_NORMAL);
    } else {
      defaultFont = wxFont(fallbackSize, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                           wxFONTWEIGHT_NORMAL, false, resolvedFaceName);
    }
    if (wxFontMapper::Get() &&
        wxFontMapper::Get()->IsEncodingAvailable(wxFONTENCODING_UTF8)) {
      defaultFont.SetEncoding(wxFONTENCODING_UTF8);
    }
  }
  return defaultFont;
}

const std::vector<std::string> &GetPreferencesDialogConfigKeys() {
  static const std::vector<std::string> kKeys = {
      "rider_autopatch",         "rider_layer_mode",
      "ui_distance_unit_system", "ui_weight_unit_system",
      "app_update_startup_mode", "viewer3d_render_style",
      "viewer3d_invert_orbit",   "rider_lx1_height",
      "rider_lx2_height",        "rider_lx3_height",
      "rider_lx4_height",        "rider_lx5_height",
      "rider_lx6_height",        "rider_lx1_pos",
      "rider_lx2_pos",           "rider_lx3_pos",
      "rider_lx4_pos",           "rider_lx5_pos",
      "rider_lx6_pos",           "rider_lx1_margin",
      "rider_lx2_margin",        "rider_lx3_margin",
      "rider_lx4_margin",        "rider_lx5_margin",
      "rider_lx6_margin",
      magnet_snap::kMagnetEnabledConfigKey,
      selection_movement_settings::kAxisConstrainedMovementConfigKey,
      selection_movement_settings::kLeftDragSelectionMovementConfigKey,
  };
  return kKeys;
}

std::vector<std::pair<std::string, std::string>>
CapturePreferencesDialogValues(const IGuiPreferencesService &preferences) {
  std::vector<std::pair<std::string, std::string>> values;
  for (const auto &key : GetPreferencesDialogConfigKeys()) {
    const auto value = preferences.GetValue(key);
    if (value.has_value())
      values.emplace_back(key, *value);
  }
  return values;
}

void RestorePreferencesDialogValues(
    IGuiPreferencesService &preferences,
    const std::vector<std::pair<std::string, std::string>> &values) {
  for (const auto &[key, value] : values)
    preferences.SetValue(key, value);
}

// Runs a startup update check and prompts for newer versions that were not
// dismissed.
void RunSilentStartupUpdateCheck(MainWindow *window) {
  if (!window)
    return;
  std::thread([window]() {
    gui::update::AppUpdateService service;
    const gui::update::CheckResult result = service.CheckForUpdates();
    if (result.status != gui::update::CheckStatus::UpdateAvailable)
      return;
    window->CallAfter([window, result]() {
      auto &preferences = GetDefaultGuiConfigServices().Preferences();
      if (!gui::update::ShouldShowStartupUpdateReminder(preferences,
                                                        result.latestVersion))
        return;

      const gui::update::UpdateNotificationChoice choice =
          gui::update::ShowAvailableUpdateDialog(window, result, true);
      if (choice.suppressVersionReminder) {
        gui::update::WriteDismissedStartupReminderVersion(preferences,
                                                          result.latestVersion);
        preferences.SaveUserConfig();
      }
      if (choice.openReleasePage)
        wxLaunchDefaultBrowser(wxString::FromUTF8(result.releaseUrl));
    });
  }).detach();
}

}

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_MENU(ID_File_New, MainWindow::OnNew)
EVT_MENU(ID_File_Load, MainWindow::OnLoad)
EVT_MENU(ID_File_Save, MainWindow::OnSave)
EVT_MENU(ID_File_SaveAs, MainWindow::OnSaveAs)
EVT_MENU(ID_File_ImportRider, MainWindow::OnImportRider)
EVT_MENU(ID_File_ImportMVR, MainWindow::OnImportMVR)
EVT_MENU(ID_File_ExportMVR, MainWindow::OnExportMVR)
EVT_MENU(ID_File_MvrXchange, MainWindow::OnMvrXchange)
EVT_MENU(ID_File_PrintViewer2D, MainWindow::OnPrintViewer2D)
EVT_MENU(ID_File_PrintLayout, MainWindow::OnPrintLayout)
EVT_MENU(ID_File_PrintTable, MainWindow::OnPrintTable)
EVT_MENU(ID_File_PrintMenu, MainWindow::OnPrintMenu)
EVT_MENU(ID_File_ExportCSV, MainWindow::OnExportCSV)
EVT_MENU(ID_File_Close, MainWindow::OnClose)
EVT_CLOSE(MainWindow::OnCloseWindow)
EVT_MENU(ID_Edit_Undo, MainWindow::OnUndo)
EVT_MENU(ID_Edit_Redo, MainWindow::OnRedo)
EVT_MENU(ID_Edit_AddFixture, MainWindow::OnAddFixture)
EVT_MENU(ID_Edit_AddTruss, MainWindow::OnAddTruss)
EVT_MENU(ID_Edit_AddSceneObject, MainWindow::OnAddSceneObject)
EVT_MENU(ID_Edit_AddPrimitiveSphere, MainWindow::OnAddPrimitiveSphere)
EVT_MENU(ID_Edit_AddPrimitiveCube, MainWindow::OnAddPrimitiveCube)
EVT_MENU(ID_Edit_AddPrimitiveCylinder, MainWindow::OnAddPrimitiveCylinder)
EVT_MENU(ID_Edit_Delete, MainWindow::OnDelete)
EVT_MENU(ID_Edit_Group, MainWindow::OnGroupSelection)
EVT_MENU(ID_Edit_Ungroup, MainWindow::OnUngroupSelection)
EVT_MENU(ID_Edit_ReplaceFixtures, MainWindow::OnReplaceSelectedFixtures)
EVT_MENU(ID_View_ToggleConsole, MainWindow::OnToggleConsole)
EVT_MENU(ID_View_ToggleFixtures, MainWindow::OnToggleFixtures)
EVT_MENU(ID_View_ToggleViewport, MainWindow::OnToggleViewport)
EVT_MENU(ID_View_ToggleViewport2D, MainWindow::OnToggleViewport2D)
EVT_MENU(ID_View_ToggleRender2D, MainWindow::OnToggleRender2D)
EVT_MENU(ID_View_ToggleLayers, MainWindow::OnToggleLayers)
EVT_MENU(ID_View_ToggleLayouts, MainWindow::OnToggleLayouts)
EVT_MENU(ID_View_ToggleSummary, MainWindow::OnToggleSummary)
EVT_MENU(ID_View_ToggleRigging, MainWindow::OnToggleRigging)
EVT_MENU(ID_View_Layout_Default, MainWindow::OnApplyDefaultLayout)
EVT_MENU(ID_View_Layout_2D, MainWindow::OnApply2DLayout)
EVT_MENU(ID_View_Layout_Mode, MainWindow::OnApplyLayoutModeLayout)
EVT_MENU(ID_View_Viewport_Top, MainWindow::OnViewportTopView)
EVT_MENU(ID_View_Viewport_Front, MainWindow::OnViewportFrontView)
EVT_MENU(ID_View_Viewport_Side, MainWindow::OnViewportSideView)
EVT_MENU(ID_View_Viewport_SelectTool, MainWindow::OnViewportSelectTool)
EVT_MENU(ID_View_Viewport_MeasureTool, MainWindow::OnViewportMeasureTool)
EVT_MENU(ID_View_Viewport_AxisConstraint, MainWindow::OnViewportAxisConstraint)
EVT_MENU(ID_View_Viewport_LeftDragMove, MainWindow::OnViewportLeftDragMove)
EVT_MENU(ID_View_Viewport_Magnet, MainWindow::OnViewportMagnet)
EVT_MENU(ID_View_Layout_2DView, MainWindow::OnLayoutAdd2DView)
EVT_MENU(ID_View_Layout_Legend, MainWindow::OnLayoutAddLegend)
EVT_MENU(ID_View_Layout_EventTable, MainWindow::OnLayoutAddEventTable)
EVT_MENU(ID_View_Layout_Text, MainWindow::OnLayoutAddText)
EVT_MENU(ID_View_Layout_Image, MainWindow::OnLayoutAddImage)
EVT_MENU(ID_Tools_DownloadGdtf, MainWindow::OnDownloadGdtf)
EVT_MENU(ID_Tools_EditDictionaries, MainWindow::OnEditDictionaries)
EVT_MENU(ID_Tools_ExportFixture, MainWindow::OnExportFixture)
EVT_MENU(ID_Tools_ExportTruss, MainWindow::OnExportTruss)
EVT_MENU(ID_Tools_ExportSceneObject, MainWindow::OnExportSceneObject)
EVT_MENU(ID_Tools_AutoPatch, MainWindow::OnAutoPatch)
EVT_MENU(ID_Tools_AutoColor, MainWindow::OnAutoColor)
EVT_MENU(ID_Tools_ConvertToHoist, MainWindow::OnConvertToHoist)
EVT_MENU(ID_Tools_GenerateFixtureSymbols, MainWindow::OnGenerateFixtureSymbols)
EVT_MENU(ID_Tools_AssignSelectedFixtureCategory,
         MainWindow::OnAssignSelectedFixtureCategory)
EVT_MENU(ID_Tools_OpenUserLibraryFolder, MainWindow::OnOpenUserLibraryFolder)
EVT_MENU(ID_Tools_ImportRiderText, MainWindow::OnImportRiderText)
EVT_MENU(ID_Tools_DistributeHoistWeights, MainWindow::OnDistributeHoistWeights)
EVT_MENU(ID_Help_Help, MainWindow::OnShowHelp)
EVT_MENU(ID_Help_OnlineDocumentation, MainWindow::OnOpenOnlineDocumentation)
EVT_MENU(ID_Help_OpenLogsFolder, MainWindow::OnOpenLogsFolder)
EVT_MENU(ID_Help_ExportDiagnosticReport, MainWindow::OnExportDiagnosticReport)
EVT_MENU(ID_Help_About, MainWindow::OnShowAbout)
EVT_MENU(ID_Help_CheckForUpdates, MainWindow::OnCheckForUpdates)
EVT_MENU(ID_Select_Fixtures, MainWindow::OnSelectFixtures)
EVT_MENU(ID_Select_Trusses, MainWindow::OnSelectTrusses)
EVT_MENU(ID_Select_Supports, MainWindow::OnSelectSupports)
EVT_MENU(ID_Select_Objects, MainWindow::OnSelectObjects)
EVT_MENU(ID_Edit_Preferences, MainWindow::OnPreferences)
EVT_COMMAND(wxID_ANY, EVT_PROJECT_LOADED, MainWindow::OnProjectLoaded)
EVT_COMMAND(wxID_ANY, EVT_UI_UNITS_CHANGED, MainWindow::OnUiUnitsChanged)
EVT_COMMAND(wxID_ANY, EVT_UI_PREFERENCES_APPLIED, MainWindow::OnPreferencesApplied)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_SELECTED, MainWindow::OnLayoutSelected)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_VIEW_EDIT, MainWindow::OnLayoutViewEdit)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_VIEW_SELECTED, MainWindow::OnLayoutViewSelected)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_RENDER_STATUS, MainWindow::OnLayoutRenderStatus)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_RENDER_READY, MainWindow::OnLayoutRenderReady)
wxEND_EVENT_TABLE()

// Constructs the main window and prepares startup UI state before project loading.
MainWindow::MainWindow(const wxString &title, IGuiConfigServices *services)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 950)),
      guiConfigServices(services ? services : &GetDefaultGuiConfigServices()) {
  SetInstance(this);
  ioController = std::make_unique<MainWindowIoController>(*this);
  layoutController = std::make_unique<MainWindowLayoutController>(*this);
  printController = std::make_unique<MainWindowPrintController>(*this);
  viewController = std::make_unique<MainWindowViewController>(*this);
  wxFont defaultUiFont = BuildDefaultUiFont();
  if (defaultUiFont.IsOk())
    SetFont(defaultUiFont);
  wxIcon icon;
  const std::filesystem::path iconPath =
      ProjectUtils::ResolveResourcePath("Perastage.ico");
  std::error_code ec;
  if (!iconPath.empty() && std::filesystem::exists(iconPath, ec)) {
    icon.LoadFile(PathToWxString(iconPath), wxBITMAP_TYPE_ICO);
  } else {
    LogMissingIcon(iconPath.empty()
                       ? std::filesystem::path("resources/Perastage.ico")
                         : iconPath);
  }
  if (!icon.IsOk())
    icon = wxArtProvider::GetIcon(wxART_MISSING_IMAGE);
  if (icon.IsOk())
    SetIcon(icon);

  Centre();
  SetupLayout();
  guiConfigServices->LegacyConfigManager().SetProjectArchiveResourceProvider(
      [this]() -> std::vector<ProjectSession::ArchiveResource> {
        if (!layoutViewerPanel)
          return {};
        return layoutViewerPanel->CollectPersistentViewCacheResources();
      });
  // Ensure the 3D viewport is available even before a project is loaded.
  Ensure3DViewport();

  SetStartupProjectLoadPending(true);
  ApplySavedLayout();

  if (layerPanel)
    layerPanel->ReloadLayers();
  if (layoutPanel)
    layoutPanel->ReloadLayouts();

  const auto shortcutRegistryErrors = gui::ValidateShortcutRegistry();
  for (const std::string &error : shortcutRegistryErrors) {
    const wxString errorText = wxString::FromUTF8(error);
    wxLogError("Shortcut registry validation failed: %s", errorText.c_str());
  }
  wxASSERT_MSG(shortcutRegistryErrors.empty(),
               "Shortcut registry contains scope collisions");

  Bind(wxEVT_IDLE, &MainWindow::OnStartupSplashCloseIdle, this);
  Bind(wxEVT_CHAR_HOOK, &MainWindow::OnGlobalCharHook, this);

  UpdateTitle();

  auto &preferences = guiConfigServices->Preferences();
  const auto now = std::chrono::system_clock::now();
  if (gui::update::ShouldRunStartupCheckNow(preferences, now)) {
    gui::update::MarkStartupCheckRun(preferences, now);
    preferences.SaveUserConfig();
    RunSilentStartupUpdateCheck(this);
  }
}

MainWindow::~MainWindow() {
  guiConfigServices->LegacyConfigManager().SetProjectArchiveResourceProvider(
      {});
  CleanupFixtureAutoUpdateStatusTimer();
  cursorStatusCallbackLifetimeToken.reset();
  if (viewport2DPanel)
    viewport2DPanel->SetCursorWorldPositionCallback({});
  if (layout2DViewEditPanel)
    layout2DViewEditPanel->SetCursorWorldPositionCallback({});
  if (!userConfigPersistedOnClose)
    SaveUserConfigWithViewport2DState();
  if (auiManager) {
    auiManager->UnInit();
    delete auiManager;
    auiManager = nullptr;
  }
  SetInstance(nullptr);
  ProjectUtils::SaveLastProjectPath(currentProjectPath);
}

void MainWindow::Ensure3DViewport() {
  if (viewportPanel)
    return;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewportPanel = new Viewer3DPanel(this);
  Viewer3DPanel::SetInstance(viewportPanel);
  viewportPanel->LoadCameraFromConfig();
  auiManager->AddPane(viewportPanel, wxAuiPaneInfo()
                                         .Name("3DViewport")
                                         .Caption("3D Viewport")
                                         .Center()
                                         .Dockable(true)
                                         .CaptionVisible(true)
                                         .PaneBorder(false)
                                         .BestSize(halfWidth, 600)
                                         .MinSize(wxSize(200, 600))
                                         .CloseButton(true)
                                         .MaximizeButton(true));
  auiManager->Update();

  if (defaultLayoutPerspective.empty()) {
    defaultLayoutPerspective = auiManager->SavePerspective().ToStdString();
  }

  if (summaryPanel)
    summaryPanel->SetVisibleRefreshTargets(viewport2DPanel, viewportPanel);
  ApplyViewportMovementToolState();
}

void MainWindow::Ensure2DViewport() {
  if (viewport2DPanel)
    return;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewport2DPanel = new Viewer2DPanel(this);
  std::weak_ptr<int> lifetimeToken = cursorStatusCallbackLifetimeToken;
  viewport2DPanel->SetCursorWorldPositionCallback(
      [this,
       lifetimeToken](const std::optional<std::array<float, 3>> &positionMeters,
          bool highlighted) {
        if (lifetimeToken.expired() || !GetStatusBar())
          return;
        if (highlighted)
          UpdateHighlightedWorldPositionInStatusBar(positionMeters);
        else
          UpdateCursorWorldPositionInStatusBar(positionMeters);
      });
  Viewer2DPanel::SetInstance(viewport2DPanel);
  viewport2DPanel->LoadViewFromConfig();
  auiManager->AddPane(viewport2DPanel, wxAuiPaneInfo()
                                           .Name("2DViewport")
                                           .Caption("2D Viewport")
                                           .Center()
                                           .Dockable(true)
                                           .CaptionVisible(true)
                                           .PaneBorder(false)
                                           .BestSize(halfWidth, 600)
                                           .MinSize(wxSize(200, 600))
                                           .CloseButton(true)
                                           .MaximizeButton(true)
                                           .Hide());
  viewport2DPanel->UpdateScene();

  viewport2DRenderPanel = new Viewer2DRenderPanel(this);
  Viewer2DRenderPanel::SetInstance(viewport2DRenderPanel);
  auiManager->AddPane(viewport2DRenderPanel, wxAuiPaneInfo()
                                                 .Name("2DRenderOptions")
                                                 .Caption("2D Render Options")
                                                 .Right()
                                                 .Layer(0)
                                                 .Row(0)
                                                 .Position(0)
                                                 .BestSize(220, 100)
                                                 .CloseButton(true)
                                                 .MaximizeButton(true)
                                                 .PaneBorder(true)
                                                 .Hide());

  if (summaryPanel)
    summaryPanel->SetVisibleRefreshTargets(viewport2DPanel, viewportPanel);
  ApplyViewportMovementToolState();

  auiManager->Update();

  if (default2DLayoutPerspective.empty()) {
    auto &pane3d = auiManager->GetPane("3DViewport");
    auto &pane2d = auiManager->GetPane("2DViewport");
    auto &paneRender = auiManager->GetPane("2DRenderOptions");
    auto &paneLayers = auiManager->GetPane("LayerPanel");
    auto &paneSummary = auiManager->GetPane("SummaryPanel");

    // 2D default: keep Layers/Summary in the outer right column and place
    // Render Options in the inner right column between viewport and side
    // column.
    if (paneLayers.IsOk()) {
      paneLayers.Right().Layer(1).Row(0).Position(0);
    }
    if (paneSummary.IsOk()) {
      paneSummary.Right().Layer(1).Row(0).Position(1);
    }
    if (paneRender.IsOk()) {
      paneRender.Right().Layer(0).Row(0).Position(0);
    }

    pane3d.Hide();
    pane2d.Show();
    paneRender.Show();
    auiManager->Update();
    default2DLayoutPerspective = auiManager->SavePerspective().ToStdString();

    // Restore base (3D) side column layout: Layers above Summary.
    if (paneLayers.IsOk()) {
      paneLayers.Right().Layer(1).Row(0).Position(0);
    }
    if (paneSummary.IsOk()) {
      paneSummary.Right().Layer(1).Row(0).Position(1);
    }
    if (paneRender.IsOk()) {
      paneRender.Right().Layer(0).Row(0).Position(0);
    }

    paneRender.Hide();
    pane2d.Hide();
    pane3d.Show();
    auiManager->Update();
  }
}

// Updates the status bar with a world-space position formatted in the active
// distance units.
void MainWindow::UpdateCursorWorldPositionInStatusBar(
    const std::optional<std::array<float, 3>> &positionMeters) {
  if (!GetStatusBar())
    return;
  if (!positionMeters.has_value()) {
    ClearCursorWorldPositionInStatusBar();
    return;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  SetStatusText(
      FormatWorldPositionStatusText(*positionMeters, distanceUnitSystem), 1);
}

// Clears the world-position status text and restores the normal status-bar font
// color.
void MainWindow::ClearCursorWorldPositionInStatusBar() {
  if (!GetStatusBar())
    return;
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const wxString unitSuffix =
      wxString::FromUTF8(Units::DistanceUnitSuffix(distanceUnitSystem));
  const wxString text =
      "X: -- " + unitSuffix + "  Y: -- " + unitSuffix + "  Z: -- " + unitSuffix;
  if (auto *statusBar = dynamic_cast<HighlightStatusBar *>(GetStatusBar())) {
    statusBar->ClearHighlightedField(1, text);
  } else {
    SetStatusText(text, 1);
  }
}

// Updates the status bar with a highlighted world-space position during drag
// interactions.
void MainWindow::UpdateHighlightedWorldPositionInStatusBar(
    const std::optional<std::array<float, 3>> &positionMeters) {
  if (!GetStatusBar())
    return;
  if (!positionMeters.has_value()) {
    ClearHighlightedWorldPositionInStatusBar();
    return;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const wxString text =
      FormatWorldPositionStatusText(*positionMeters, distanceUnitSystem);
  if (auto *statusBar = dynamic_cast<HighlightStatusBar *>(GetStatusBar())) {
    statusBar->SetHighlightedFieldText(1, text, wxColour(30, 115, 210));
  } else {
    SetStatusText(text, 1);
  }
}

// Restores the status bar after a highlighted drag-position display ends.
void MainWindow::ClearHighlightedWorldPositionInStatusBar() {
  ClearCursorWorldPositionInStatusBar();
}

void MainWindow::Ensure2DViewportAvailable() { Ensure2DViewport(); }

Viewer2DPanel *MainWindow::GetLayoutCapturePanel() const {
  if (layout2DViewEditing && layout2DViewEditPanel)
    return layout2DViewEditPanel;
  return viewport2DPanel;
}

Viewer2DOffscreenRenderer *MainWindow::GetOffscreenRenderer() {
  if (!offscreenViewer2DRenderer) {
    offscreenViewer2DRenderer =
        std::make_unique<Viewer2DOffscreenRenderer>(this);
  }
  return offscreenViewer2DRenderer.get();
}

bool MainWindow::IsLayout2DViewEditing() const { return layout2DViewEditing; }

// Prompts to save pending project changes while ignoring transient startup
// state before any project is loaded.
bool MainWindow::ConfirmSaveIfDirty(const wxString &actionLabel,
                                    const wxString &dialogTitle) {
  if ((startupProjectLoadPending || startupSplashInitializationPending ||
       startupDeferredOpenInProgress) &&
      currentProjectPath.empty())
    return true;

  if (!GetDefaultGuiConfigServices().LegacyConfigManager().IsDirty())
    return true;

  wxMessageDialog dlg(this,
      "Do you want to save changes before " + actionLabel + "?",
      dialogTitle, wxYES_NO | wxCANCEL | wxICON_QUESTION);

  int res = dlg.ShowModal();
  if (res == wxID_YES) {
    wxCommandEvent saveEvt;
    OnSave(saveEvt);
    return !GetDefaultGuiConfigServices().LegacyConfigManager().IsDirty();
  }
  if (res == wxID_CANCEL)
    return false;

  return true;
}

// Import fixtures and trusses from a rider (.txt/.pdf)
// Handles MVR file selection, import, and updates fixture/truss panels
// accordingly
void MainWindow::OnPaneClose(wxAuiManagerEvent &event) {
  event.Skip();
  CallAfter(&MainWindow::UpdateViewMenuChecks);
}

void MainWindow::SetStartupProjectLoadPending(bool pending) {
  startupProjectLoadPending = pending;

  wxMenuBar *menuBar = GetMenuBar();
  if (menuBar) {
    menuBar->Enable(ID_File_New, !pending);
    menuBar->Enable(ID_File_Load, !pending);
  }

  if (fileToolBar) {
    fileToolBar->EnableTool(ID_File_New, !pending);
    fileToolBar->EnableTool(ID_File_Load, !pending);
    fileToolBar->Refresh();
  }
}

// Cancels pending startup-project loading and defers an external file-open
// path.
void MainWindow::CancelStartupProjectLoadForExternalOpenPath(
    const std::string &path) {
  ProjectUtils::SaveLastProjectPath("");
  QueueDeferredStartupOpenPath(path);
  diagnostics::DiagnosticLogger::Info(
      "Startup project load canceled for external open: " +
      diagnostics::DiagnosticLogger::FileNameOnly(path));
  SetStartupProjectLoadPending(false);
  RequestStartupSplashCompletion();
}

bool MainWindow::GuardStartupProjectLoadAction(const wxString &actionLabel) {
  if (!startupProjectLoadPending)
    return true;

  wxMessageBox(
      "Please wait until the startup project loading finishes before " +
                   actionLabel + ".",
               "Perastage is still loading", wxOK | wxICON_INFORMATION, this);
  return false;
}

// Loads a project archive and refreshes only the active layout preview during
// startup.
bool MainWindow::LoadProjectFromPath(const std::string &path,
                                     bool showBlockingLoadUi) {
  diagnostics::DiagnosticLogger::Info(
      "Project load started: " +
      diagnostics::DiagnosticLogger::FileNameOnly(path));
  constexpr int kProjectLoadProgressSteps = 10;
  int projectLoadProgressStep = 0;
  std::unique_ptr<wxProgressDialog> projectLoadProgressDialog;
  auto reportProjectLoadProgress = [&](const wxString &message,
                                       bool advanceStep = false,
                                       int completed = -1, int total = -1) {
    if (advanceStep)
      projectLoadProgressStep =
          std::min(projectLoadProgressStep + 1, kProjectLoadProgressSteps);

    if (GetStatusBar())
      SetStatusText(message, 0);

    if (showBlockingLoadUi) {
      if (!projectLoadProgressDialog) {
        projectLoadProgressDialog = std::make_unique<wxProgressDialog>(
            "Project loading", message, kProjectLoadProgressSteps, this,
            wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_APP_MODAL);
      }
      if (total > 0) {
        const int safeTotal = std::max(total, 1);
        const int safeCompleted = std::clamp(completed, 0, safeTotal);
        projectLoadProgressDialog->SetRange(safeTotal);
        projectLoadProgressDialog->Update(safeCompleted, message);
      } else {
        projectLoadProgressDialog->Pulse(message);
      }
    } else {
      SplashScreen::SetMessage(message);
    }
  };

  gui::layoutperf::LayoutRenderProfiler loadProfiler(
      "project_load_layout_startup");
  loadProfiler.BeginPhase("project_archive_load");

  if (showBlockingLoadUi) {
    blockingProjectLoadDisabler = std::make_unique<wxWindowDisabler>();
    blockingProjectLoadOverlay.reset();
  } else {
    blockingProjectLoadOverlay.reset();
  }
  reportProjectLoadProgress("Loading project file...");

  LockViewportInteraction();
  auto finishLoad = [this]() { UnlockViewportInteraction(); };

  if (!GetDefaultGuiConfigServices().LegacyConfigManager().LoadProject(
          path, [&](const std::string &stage, int completed, int total) {
            reportProjectLoadProgress(wxString::FromUTF8(stage), false,
                                      completed, total);
          })) {
    projectLoadProgressDialog.reset();
    ClearBlockingProjectLoadUi();
    finishLoad();
    loadProfiler.Finish("project_load_failed");
    diagnostics::DiagnosticLogger::Error(
        "Project load failed: " +
        diagnostics::DiagnosticLogger::FileNameOnly(path));
    return false;
  }
  loadProfiler.EndPhase();
  if (layoutViewerPanel)
    layoutViewerPanel->LoadPersistentViewCacheFromProject(path);
  viewer2d::ReconcileFixtureLabelOverridesWithScene(
      GetDefaultGuiConfigServices().LegacyConfigManager());

  loadProfiler.BeginPhase("scene_setup");
  reportProjectLoadProgress("Building scene...", true);
  Ensure3DViewport();
  loadProfiler.EndPhase();

  currentProjectPath = path;
  currentProjectDisplayName.clear();
  ProjectUtils::SaveLastProjectPath(currentProjectPath);

  loadProfiler.BeginPhase("layout_selection_reload");
  reportProjectLoadProgress("Applying saved layout...", true);
  activeLayoutName.clear();
  const std::string startupLayoutName = ResolveProjectStartupLayoutName(
      GetDefaultGuiConfigServices().LegacyConfigManager());
  ApplySavedLayout();
  if (!startupLayoutName.empty())
    ActivateLayoutView(startupLayoutName);
  if (layoutPanel) {
    layoutPanel->SetCurrentLayout(activeLayoutName);
    layoutPanel->ReloadLayouts();
  }
  // Inactive layouts stay in LayoutManager, but preview caches build lazily
  // when selected.
  if (const auto &loadedLayouts =
          layouts::LayoutManager::Get().GetLayouts().Items();
      !loadedLayouts.empty()) {
    const layouts::LayoutDefinition *profiledLayout = &loadedLayouts.front();
    for (const auto &layout : loadedLayouts) {
      if (layout.name == activeLayoutName) {
        profiledLayout = &layout;
        break;
      }
    }
    loadProfiler.SetLayoutContext(loadedLayouts.size(), *profiledLayout);
  }
  loadProfiler.EndPhase();

  loadProfiler.BeginPhase("data_panels_reload");
  reportProjectLoadProgress("Reloading fixture/truss/support tables...", true);
  if (consolePanel)
    consolePanel->AppendMessage("Loaded " + wxString::FromUTF8(path));
  if (fixturePanel)
    fixturePanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (sceneObjPanel)
    sceneObjPanel->ReloadData();

  loadProfiler.EndPhase();
  loadProfiler.BeginPhase("viewport_refresh");
  reportProjectLoadProgress("Updating 3D viewport...", true);
  if (viewportPanel) {
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    Viewer3DCamera &cam = viewportPanel->GetCamera();
    cam.SetOrientation(cfg.GetFloat("camera_yaw"),
                       cfg.GetFloat("camera_pitch"));
    cam.SetDistance(cfg.GetFloat("camera_distance"));
    cam.SetTarget(cfg.GetFloat("camera_target_x"),
                  cfg.GetFloat("camera_target_y"),
                  cfg.GetFloat("camera_target_z"));
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }

  reportProjectLoadProgress("Updating 2D viewport...", true);
  if (viewport2DPanel) {
    if (!HasActiveLayout2DView())
      viewport2DPanel->LoadViewFromConfig();
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (viewport2DRenderPanel)
    viewport2DRenderPanel->ApplyConfig();
  ApplyViewportMovementToolState();
  if (layerPanel)
    layerPanel->ReloadLayers();

  loadProfiler.EndPhase();
  loadProfiler.BeginPhase("summary_rigging_refresh");
  reportProjectLoadProgress("Refreshing panels...", true);
  RefreshSummary();
  reportProjectLoadProgress("Refreshing rigging...", true);
  RefreshRigging();
  GetDefaultGuiConfigServices().LegacyConfigManager().MarkSaved();
  loadProfiler.EndPhase();
  loadProfiler.BeginPhase("fixture_symbol_startup");
  reportProjectLoadProgress("Creating fixture symbols...", true);

  if (showBlockingLoadUi) {
    auto previousCompletionCallback = fixtureSymbolAutoUpdateCompletionCallback;
    fixtureSymbolAutoUpdateCompletionCallback = [this,
                                                 previousCompletionCallback]() {
          if (previousCompletionCallback)
            previousCompletionCallback();
          ClearBlockingProjectLoadUi();
        };
  }

  StartFixtureSymbolAutoUpdateForLoadedScene();
  loadProfiler.EndPhase();
  loadProfiler.BeginPhase("finalize_project_load");
  reportProjectLoadProgress("Updating window title...", true);
  UpdateTitle();
  projectLoadProgressStep = kProjectLoadProgressSteps;
  reportProjectLoadProgress("Finalizing project load...");
  projectLoadProgressDialog.reset();
  finishLoad();
  loadProfiler.Finish("project_load_completed");
  diagnostics::DiagnosticLogger::Info(
      "Project load completed: " +
      diagnostics::DiagnosticLogger::FileNameOnly(path));
  return true;
}

void MainWindow::ClearBlockingProjectLoadUi() {
  blockingProjectLoadOverlay.reset();
  blockingProjectLoadDisabler.reset();
}

void MainWindow::LoadStartupProjectFromPath(const std::string &path) {
  namespace fs = std::filesystem;

  std::error_code ec;
  const fs::path projectPath = PathUtils::PathFromUtf8(path);
  if (!fs::is_regular_file(projectPath, ec)) {
    ProjectUtils::SaveLastProjectPath("");
    ResetProject(true);
    SetStartupProjectLoadPending(false);
    RequestStartupSplashCompletion();
    return;
  }

  fixtureSymbolAutoUpdateCompletionCallback = [this]() {
    RequestStartupSplashCompletion();
  };

  SplashScreen::SetMessage("Loading project file...");
  wxYieldIfNeeded();
  if (!LoadProjectFromPath(path, false)) {
    fixtureSymbolAutoUpdateCompletionCallback = nullptr;
    ProjectUtils::SaveLastProjectPath("");
    ResetProject(true);
    RequestStartupSplashCompletion();
  }

  SetStartupProjectLoadPending(false);
}

// Resets project state, UI-bound data, and active layout context for a fresh
// session.
void MainWindow::ResetProject(bool applyLayoutDefaultsForNewProject) {
  activeLayoutName.clear();
  if (layoutViewerPanel)
    layoutViewerPanel->SetLayoutDefinition(layouts::LayoutDefinition{});

  auto &guiConfigServices = GetDefaultGuiConfigServices();
  auto &preferences = guiConfigServices.Preferences();
  const auto preservedPreferences = CapturePreferencesDialogValues(preferences);
  auto &cfg = guiConfigServices.LegacyConfigManager();
  cfg.Reset();
  if (applyLayoutDefaultsForNewProject)
    layouts::LayoutManager::Get().LoadDefaultsForNewProject(cfg);
  RestorePreferencesDialogValues(preferences, preservedPreferences);
  cfg.MarkSaved();
  fixtureSymbolAutoUpdateQueue.clear();
  fixtureSymbolAutoUpdateProcessedKeys.clear();
  fixtureSymbolPendingLibrarySyncUuids.clear();
  fixtureSymbolAutoUpdateGeneratedTypes.clear();
  fixtureSymbolAutoUpdateErrors.clear();
  fixtureSymbolAutoUpdateGeneratedTypeSet.clear();
  fixtureSymbolAutoUpdateRunning = false;
  fixtureSymbolAutoUpdateCompletionCallback = nullptr;
  startupSplashCloseRequested = false;
  currentProjectPath.clear();
  currentProjectDisplayName.clear();
  if (layoutPanel)
    layoutPanel->ReloadLayouts();
  if (fixturePanel)
    fixturePanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  if (viewport2DPanel) {
    if (!HasActiveLayout2DView())
      viewport2DPanel->LoadViewFromConfig();
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  if (viewport2DRenderPanel)
    viewport2DRenderPanel->ApplyConfig();
  if (layerPanel)
    layerPanel->ReloadLayers();
  RefreshSummary();
  RefreshRigging();
  UpdateTitle();
}


// Returns the current project name displayed in the main window title.
wxString MainWindow::GetCurrentProjectDisplayName() const {
  if (!currentProjectPath.empty()) {
    wxFileName fn(wxString::FromUTF8(currentProjectPath));
    return fn.GetName();
  }
  if (!currentProjectDisplayName.IsEmpty()) return currentProjectDisplayName;
  return "Untitled";
}

void MainWindow::UpdateTitle() {
  wxString title = app::kName;
  if (!currentProjectPath.empty()) {
    wxFileName fn(wxString::FromUTF8(currentProjectPath));
    title += " - " + fn.GetName();
  } else if (!currentProjectDisplayName.IsEmpty()) {
    title += " - " + currentProjectDisplayName;
  } else {
    title += " - Untitled";
  }
  SetTitle(title);
}

void MainWindow::SaveCameraSettings() {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  if (layoutModeActive)
    PersistLayout2DViewState();
  if (viewportPanel) {
    Viewer3DCamera &cam = viewportPanel->GetCamera();
    cfg.SetFloat("camera_yaw", cam.GetYaw());
    cfg.SetFloat("camera_pitch", cam.GetPitch());
    cfg.SetFloat("camera_distance", cam.GetDistance());
    cfg.SetFloat("camera_target_x", cam.GetTargetX());
    cfg.SetFloat("camera_target_y", cam.GetTargetY());
    cfg.SetFloat("camera_target_z", cam.GetTargetZ());
  }
  if (viewport2DPanel)
    viewport2DPanel->SaveViewToConfig();

  if (auiManager) {
    const std::string perspective = auiManager->SavePerspective().ToStdString();
    cfg.SetValue("layout_perspective", perspective);
  }
}

void MainWindow::SaveUserConfigWithViewport2DState() {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  std::optional<viewer2d::Viewer2DState> layoutState;
  if (layoutModeActive && viewport2DPanel)
    layoutState = viewer2d::CaptureState(viewport2DPanel, cfg);

  SaveCameraSettings();

  if (layoutModeActive) {
    if (!standalone2DState)
      standalone2DState = viewer2d::CaptureState(nullptr, cfg);
    if (standalone2DState)
      viewer2d::ApplyState(nullptr, nullptr, cfg, *standalone2DState, true,
                           false);
  }

  cfg.SaveUserConfig();

  if (layoutModeActive && layoutState)
    viewer2d::ApplyState(nullptr, nullptr, cfg, *layoutState, true, false);
}

void MainWindow::UpdateToolBarAvailability() {
  if (!auiManager)
    return;

  const auto paneShown = [this](const char *name) {
    auto &pane = auiManager->GetPane(name);
    return pane.IsOk() && pane.IsShown();
  };

  const bool hasLayoutViewer = paneShown("LayoutViewer");
  const bool hasCreationTarget = paneShown("2DViewport") ||
                                 paneShown("3DViewport") ||
                                 paneShown("DataNotebook");
  const bool enableViewportTools = !hasLayoutViewer;

  if (layoutToolBar) {
    const struct {
      int id;
      const char *help;
    } layoutTools[] = {
        {ID_View_Layout_2DView, "Add 2D View to Layout"},
        {ID_View_Layout_Legend, "Add fixture legend to layout"},
        {ID_View_Layout_EventTable, "Add event table to layout"},
        {ID_View_Layout_Text, "Add text box to layout"},
        {ID_View_Layout_Image, "Add image to layout"},
    };

    for (const auto &tool : layoutTools) {
      layoutToolBar->EnableTool(tool.id, hasLayoutViewer);
      layoutToolBar->SetToolShortHelp(
          tool.id, hasLayoutViewer
                       ? tool.help
                          : "Layout tools require an open Layout viewer window");
    }
    layoutToolBar->Refresh();
  }

  if (toolsToolBar) {
    const struct {
      int id;
      const char *help;
    } sceneTools[] = {
        {ID_Edit_AddFixture, "Add fixture"},
        {ID_Edit_AddTruss, "Add truss"},
        {ID_Edit_AddSceneObject, "Add object"},
    };

    for (const auto &tool : sceneTools) {
      toolsToolBar->EnableTool(tool.id, hasCreationTarget);
      toolsToolBar->SetToolShortHelp(
          tool.id,
          hasCreationTarget
              ? tool.help
              : "Requires an open 2D viewport, 3D viewport, or Data Views");
    }
    toolsToolBar->Refresh();
  }

  if (layoutViewsToolBar) {
    layoutViewsToolBar->EnableTool(ID_View_Viewport_SelectTool,
                                   enableViewportTools);
    layoutViewsToolBar->EnableTool(ID_View_Viewport_MeasureTool,
                                   enableViewportTools);
    layoutViewsToolBar->EnableTool(ID_View_Viewport_AxisConstraint,
                                   enableViewportTools);
    layoutViewsToolBar->EnableTool(ID_View_Viewport_LeftDragMove,
                                   enableViewportTools);
    layoutViewsToolBar->EnableTool(ID_View_Viewport_Magnet,
                                   enableViewportTools);
    layoutViewsToolBar->SetToolShortHelp(
        ID_View_Viewport_SelectTool,
        enableViewportTools ? "Switch to standard selection mode"
                            : "Disabled while editing Layout views");
    layoutViewsToolBar->SetToolShortHelp(
        ID_View_Viewport_MeasureTool,
        enableViewportTools ? "Toggle center-to-center measure tool"
                            : "Disabled while editing Layout views");
    layoutViewsToolBar->SetToolShortHelp(
        ID_View_Viewport_AxisConstraint,
        enableViewportTools ? "Toggle axis-constrained selection movement"
                            : "Disabled while editing Layout views");
    layoutViewsToolBar->SetToolShortHelp(
        ID_View_Viewport_LeftDragMove,
        enableViewportTools ? "Toggle left-click selection dragging"
                            : "Disabled while editing Layout views");
    layoutViewsToolBar->SetToolShortHelp(
        ID_View_Viewport_Magnet, enableViewportTools
                                     ? "Toggle Magnet snapping while dragging"
                            : "Disabled while editing Layout views");
    layoutViewsToolBar->Refresh();
  }
}

void MainWindow::UpdateViewMenuChecks() {
  if (!auiManager || !GetMenuBar())
    return;

  auto &consolePane = auiManager->GetPane("Console");
  GetMenuBar()->Check(ID_View_ToggleConsole,
                      consolePane.IsOk() && consolePane.IsShown());

  auto &dataPane = auiManager->GetPane("DataNotebook");
  GetMenuBar()->Check(ID_View_ToggleFixtures,
                      dataPane.IsOk() && dataPane.IsShown());

  auto &viewPane3D = auiManager->GetPane("3DViewport");
  GetMenuBar()->Check(ID_View_ToggleViewport,
                      viewPane3D.IsOk() && viewPane3D.IsShown());

  auto &viewPane2D = auiManager->GetPane("2DViewport");
  GetMenuBar()->Check(ID_View_ToggleViewport2D,
                      viewPane2D.IsOk() && viewPane2D.IsShown());

  auto &renderPane = auiManager->GetPane("2DRenderOptions");
  GetMenuBar()->Check(ID_View_ToggleRender2D,
                      renderPane.IsOk() && renderPane.IsShown());

  auto &layerPane = auiManager->GetPane("LayerPanel");
  GetMenuBar()->Check(ID_View_ToggleLayers,
                      layerPane.IsOk() && layerPane.IsShown());

  auto &layoutPane = auiManager->GetPane("LayoutPanel");
  GetMenuBar()->Check(ID_View_ToggleLayouts,
                      layoutPane.IsOk() && layoutPane.IsShown());

  auto &summaryPane = auiManager->GetPane("SummaryPanel");
  GetMenuBar()->Check(ID_View_ToggleSummary,
                      summaryPane.IsOk() && summaryPane.IsShown());

  auto &riggingPane = auiManager->GetPane("RiggingPanel");
  GetMenuBar()->Check(ID_View_ToggleRigging,
                      riggingPane.IsOk() && riggingPane.IsShown());

  UpdateToolBarAvailability();
}

// Activates a layout selected from the layout panel after startup loading is
// complete.
void MainWindow::OnLayoutSelected(wxCommandEvent &event) {
  if (startupProjectLoadPending)
    return;
  ActivateLayoutView(event.GetString().ToStdString());
}

bool MainWindow::HasActiveLayout2DView() const {
  if (activeLayoutName.empty())
    return false;

  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    if (layout.name != activeLayoutName)
      continue;
    for (const auto &view : layout.view2dViews) {
      if (view.id > 0)
        return true;
    }
    return false;
  }
  return false;
}

// Shows layout-loading status text and enables the busy cursor when the layout
// viewer is visible.
void MainWindow::ShowLayoutLoadingIndicator(const wxString &message) {
  if (GetStatusBar())
    SetStatusText(message, 0);

  bool layoutViewerVisible = true;
  if (auiManager) {
    auto &layoutPane = auiManager->GetPane("LayoutViewer");
    if (layoutPane.IsOk() && !layoutPane.IsShown())
      layoutViewerVisible = false;
  }
  if (layoutViewerPanel && !layoutViewerPanel->IsShownOnScreen())
    layoutViewerVisible = false;

  if (layoutViewerVisible && !layoutRenderCursor)
    layoutRenderCursor = std::make_unique<wxBusyCursor>();
}

// Clears layout-loading status text and releases the busy cursor indicator.
void MainWindow::ClearLayoutLoadingIndicator() {
  if (GetStatusBar())
    SetStatusText("", 0);
  layoutRenderCursor.reset();
}

// Clears the loading indicator when the layout render pipeline reports
// completion.
void MainWindow::OnLayoutRenderReady(wxCommandEvent &) {
  ClearLayoutLoadingIndicator();
}

// Mirrors layout-render updates unless fixture symbol generation is currently
// reporting progress.
void MainWindow::OnLayoutRenderStatus(wxCommandEvent &event) {
  const wxString statusMessage = event.GetString();
  if (statusMessage.CmpNoCase("Layout render completed.") == 0) {
    ClearLayoutLoadingIndicator();
    return;
  }
  if (fixtureSymbolAutoUpdateRunning ||
      (GetStatusBar() &&
       IsFixtureSymbolStatusMessage(GetStatusBar()->GetStatusText(0)))) {
    return;
  }
  ShowLayoutLoadingIndicator(statusMessage);
}

// Applies the named project layout to the layout viewer and persists it as
// active.
void MainWindow::ActivateLayoutView(const std::string &layoutName) {
  if (!auiManager || layoutName.empty()) {
    ClearLayoutLoadingIndicator();
    return;
  }

  if (!activeLayoutName.empty() && activeLayoutName != layoutName) {
    PersistLayout2DViewState();
  }
  activeLayoutName = layoutName;

  std::optional<layouts::LayoutDefinition> selectedLayout;
  bool appliedLayout = false;
  const auto &layouts = layouts::LayoutManager::Get().GetLayouts().Items();
  for (const auto &layout : layouts) {
    if (layout.name == layoutName) {
      selectedLayout = layout;
      if (layoutViewerPanel) {
        ShowLayoutLoadingIndicator("Rendering layout...");
        if (GetStatusBar())
          GetStatusBar()->Update();
        else
          Update();
        layoutViewerPanel->SetLayoutDefinition(layout);
        appliedLayout = true;
      }
      break;
    }
  }

  if (!selectedLayout || !layoutViewerPanel || !appliedLayout) {
    ClearLayoutLoadingIndicator();
  } else {
    GetDefaultGuiConfigServices().LegacyConfigManager().SetValue(
        kActiveLayoutNameConfigKey, activeLayoutName);
  }

  if (viewport2DPanel && layoutModeActive) {
    int viewId = 0;
    bool hasViewId = false;
    if (layoutViewerPanel) {
      if (const auto *view = layoutViewerPanel->GetEditableView()) {
        viewId = view->id;
        hasViewId = viewId > 0;
      }
    }
    if (!hasViewId && selectedLayout && !selectedLayout->view2dViews.empty()) {
      viewId = selectedLayout->view2dViews.front().id;
      hasViewId = viewId > 0;
    }
    if (hasViewId)
      RestoreLayout2DViewState(viewId);
  }

  if (layoutModeActive)
    ApplyLayoutModePerspective();
}

// Synchronizes pending edits from the active table panel into the scene before
// save/close operations.
void MainWindow::SyncSceneData() {
  // Save uses scene data as source of truth; avoid table-wide resyncs that can
  // overwrite cross-table transforms.
  PersistFixtureTypeAutoColors(
      GetDefaultGuiConfigServices().LegacyConfigManager());
}

void MainWindow::RefreshAfterSceneChange(bool refreshViewport) {
  PersistFixtureTypeAutoColors(
      GetDefaultGuiConfigServices().LegacyConfigManager());
  if (fixturePanel)
    fixturePanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  RefreshSummary();
  if (layoutViewerPanel)
    layoutViewerPanel->RefreshAfterSceneContentUpdate();
  if (refreshViewport) {
    if (viewportPanel) {
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
    if (viewport2DPanel) {
      viewport2DPanel->UpdateScene();
      viewport2DPanel->Refresh();
    }
  }
}

void MainWindow::SyncLayerVisibilityPanels() {
  if (layerPanel)
    layerPanel->ReloadLayers();

  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }

  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene(false);
    viewport2DPanel->Refresh();
  }
}

// Handles startup project-load completion and queues deferred external file
// opens when needed.
void MainWindow::OnProjectLoaded(wxCommandEvent &event) {
  bool loaded = event.GetInt() != 0;
  bool clearLastProject = event.GetExtraLong() != 0;
  std::string path;
  {
    const wxString eventPath = event.GetString();
    const wxCharBuffer eventPathUtf8 = eventPath.ToUTF8();
    path = eventPathUtf8 ? std::string(eventPathUtf8.data())
                         : eventPath.ToStdString();
  }
  Ensure3DViewport();
  if (clearLastProject)
    ProjectUtils::SaveLastProjectPath("");
  if (loaded && !path.empty()) {
    currentProjectPath = path;
    currentProjectDisplayName.clear();
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    activeLayoutName.clear();
    const std::string startupLayoutName = ResolveProjectStartupLayoutName(
        GetDefaultGuiConfigServices().LegacyConfigManager());
    ApplySavedLayout();
    if (!startupLayoutName.empty())
      ActivateLayoutView(startupLayoutName);
    if (layoutPanel) {
      layoutPanel->SetCurrentLayout(activeLayoutName);
      layoutPanel->ReloadLayouts();
    }
    if (consolePanel)
      consolePanel->AppendMessage("Loaded " + wxString::FromUTF8(path));
    SplashScreen::SetMessage("Loading tables...");
    if (fixturePanel)
      fixturePanel->ReloadData();
    if (trussPanel)
      trussPanel->ReloadData();
    if (hoistPanel)
      hoistPanel->ReloadData();
    if (sceneObjPanel)
      sceneObjPanel->ReloadData();
    if (viewportPanel) {
      ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      Viewer3DCamera &cam = viewportPanel->GetCamera();
      cam.SetOrientation(cfg.GetFloat("camera_yaw"),
                         cfg.GetFloat("camera_pitch"));
      cam.SetDistance(cfg.GetFloat("camera_distance"));
      cam.SetTarget(cfg.GetFloat("camera_target_x"),
                    cfg.GetFloat("camera_target_y"),
                    cfg.GetFloat("camera_target_z"));
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
    if (viewport2DPanel) {
      // During startup the panel has already loaded the persisted standalone
      // camera state once (in Ensure2DViewport). Re-loading here can cause a
      // second camera jump right after launch.
      viewport2DPanel->UpdateScene();
      viewport2DPanel->Refresh();
    }
    if (viewport2DRenderPanel)
      viewport2DRenderPanel->ApplyConfig();
    ApplyViewportMovementToolState();
    SyncViewportToolToggleState(
        (viewport2DPanel && viewport2DPanel->IsMeasureToolEnabled()) ||
        (viewportPanel && viewportPanel->IsMeasureToolEnabled()));
    if (layerPanel)
      layerPanel->ReloadLayers();
    SplashScreen::SetMessage("Refreshing panels...");
    RefreshSummary();
    RefreshRigging();
    GetDefaultGuiConfigServices().LegacyConfigManager().MarkSaved();
    SplashScreen::SetMessage("Creating fixture symbols...");
    fixtureSymbolAutoUpdateCompletionCallback = [this]() {
      RequestStartupSplashCompletion();
    };
    StartFixtureSymbolAutoUpdateForLoadedScene();
    UpdateTitle();
  } else {
    ResetProject(true);
    if (!path.empty())
      QueueDeferredStartupOpenPath(path);
    // Some platforms can delay or skip idle delivery during startup.
    // Force an explicit completion pass so deferred startup open paths
    // can be processed deterministically after startup reset.
    CallAfter([this]() { CompleteStartupSplashInitialization(); });
    SetStartupProjectLoadPending(false);
    RequestStartupSplashCompletion();
    return;
  }
  SetStartupProjectLoadPending(false);
}

void MainWindow::OnUiUnitsChanged(wxCommandEvent &WXUNUSED(event)) {
  RefreshAfterUnitSystemChange();
}

void MainWindow::OnPreferencesApplied(wxCommandEvent &WXUNUSED(event)) {
  if (viewportPanel)
    viewportPanel->Refresh();
  if (viewport2DPanel)
    viewport2DPanel->Refresh();
}

void MainWindow::OnNotebookPageChanged(wxBookCtrlEvent &event) {
  RefreshSummary();
  event.Skip();
}

void MainWindow::RefreshSummary() {
  if (summaryPanel && notebook) {
    int sel = notebook->GetSelection();
    if (notebook->GetPage(sel) == fixturePanel)
      summaryPanel->ShowFixtureSummary();
    else if (notebook->GetPage(sel) == trussPanel)
      summaryPanel->ShowTrussSummary();
    else if (notebook->GetPage(sel) == hoistPanel)
      summaryPanel->ShowHoistSummary();
    else if (notebook->GetPage(sel) == sceneObjPanel)
      summaryPanel->ShowSceneObjectSummary();
  }

  if (layoutViewerPanel)
    layoutViewerPanel->RefreshLegendData();

  RefreshRigging();
}

void MainWindow::RefreshRigging() {
  if (riggingPanel)
    riggingPanel->RefreshData();
}

void MainWindow::EnableShortcuts(bool enable) {
  shortcutHandlingEnabled = enable;
  if (enable)
    SetAcceleratorTable(m_accel);
  else
    SetAcceleratorTable(wxAcceleratorTable());
}

void MainWindow::RefreshAfterFixtureSymbolUpdate() {
  if (viewport2DPanel) {
    viewport2DPanel->InvalidateBottomSymbolCache();
    viewport2DPanel->UpdateScene(true);
    viewport2DPanel->Refresh();
  }
  ApplyViewportMovementToolState();
  if (layoutViewerPanel)
    layoutViewerPanel->RefreshAfterFixtureSymbolUpdate();
}

void MainWindow::RefreshAfterToolSceneUpdate() { RefreshAfterSceneChange(); }

void MainWindow::RefreshAfterUnitSystemChange() {
  RefreshAfterSceneChange();
  if (layoutViewerPanel) {
    layoutViewerPanel->RefreshLegendData();
    layoutViewerPanel->RefreshAfterSelectionOnlyUpdate();
  }
  ClearCursorWorldPositionInStatusBar();
}
