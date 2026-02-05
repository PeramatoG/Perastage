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

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <wx/event.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <random>
#include <string>
#include <thread>
#include <tinyxml2.h>
#include <wx/aboutdlg.h>
#include <wx/app.h>
#include <wx/artprov.h>
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
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/log.h>
#include <wx/zipstrm.h>
#include <wx/richtext/richtextbuffer.h>

#include "../external/json.hpp"

using json = nlohmann::json;
#include "addfixturedialog.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "credentialstore.h"
#include "dictionaryeditdialog.h"
#include "exportfixturedialog.h"
#include "exportobjectdialog.h"
#include "exporttrussdialog.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "gdtfsearchdialog.h"
#include "layout2dviewdialog.h"
#include "layoutviewpresets.h"
#include "layoutpanel.h"
#include "layoutviewerpanel.h"
#include "layouttextutils.h"
#include "layoutviewerpanel_shared.h"
#include "legendutils.h"
#include "layerpanel.h"
#include "logindialog.h"
#include "markdown.h"
#include "hoisttablepanel.h"
#include "viewer2dprintdialog.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "preferencesdialog.h"
#include "projectutils.h"
#include "riggingpanel.h"
#include "riderimporter.h"
#include "ridertextdialog.h"
#include "sceneobjecttablepanel.h"
#include "selectfixturetypedialog.h"
#include "selectnamedialog.h"
#include "simplecrypt.h"
#include "splashscreen.h"
#include "summarypanel.h"
#include "tableprinter.h"
#include "print/Viewer2DPrintSettings.h"
#include "viewer2dpdfexporter.h"
#include "print_diagnostics.h"
#include "support.h"
#include "trussloader.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2drenderpanel.h"
#include "viewer2dstate.h"
#include "viewer3dpanel.h"
#include "layouts/LayoutManager.h"
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

wxDEFINE_EVENT(EVT_PROJECT_LOADED, wxCommandEvent);

MainWindow *MainWindow::Instance() { return s_instance; }

void MainWindow::SetInstance(MainWindow *inst) { s_instance = inst; }

namespace {
void LogMissingIcon(const std::filesystem::path &path) {
  wxLogWarning("Main window icon not found at '%s'", path.string().c_str());
}

wxFont BuildDefaultUiFont() {
  wxFont defaultFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  const wxString faceName =
      layoutviewerpanel::detail::ResolveSharedFontFaceName();
  wxString resolvedFaceName = faceName;
  if (resolvedFaceName.empty()) {
    wxFont testFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_NORMAL, false, wxString::FromUTF8("Arial"));
    if (testFont.IsOk() &&
        testFont.GetFaceName().CmpNoCase("Arial") == 0) {
      resolvedFaceName = testFont.GetFaceName();
    }
  }
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
    wxString fallbackFace =
        resolvedFaceName.empty() ? wxString::FromUTF8("Arial")
                                 : resolvedFaceName;
    defaultFont = wxFont(fallbackSize, wxFONTFAMILY_SWISS,
                         wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false,
                         fallbackFace);
    if (wxFontMapper::Get() &&
        wxFontMapper::Get()->IsEncodingAvailable(wxFONTENCODING_UTF8)) {
      defaultFont.SetEncoding(wxFONTENCODING_UTF8);
    }
  }
  return defaultFont;
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
EVT_MENU(ID_Edit_Delete, MainWindow::OnDelete)
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
EVT_MENU(ID_Tools_ImportRiderText, MainWindow::OnImportRiderText)
EVT_MENU(ID_Help_Help, MainWindow::OnShowHelp)
EVT_MENU(ID_Help_About, MainWindow::OnShowAbout)
EVT_MENU(ID_Select_Fixtures, MainWindow::OnSelectFixtures)
EVT_MENU(ID_Select_Trusses, MainWindow::OnSelectTrusses)
EVT_MENU(ID_Select_Supports, MainWindow::OnSelectSupports)
EVT_MENU(ID_Select_Objects, MainWindow::OnSelectObjects)
EVT_MENU(ID_Edit_Preferences, MainWindow::OnPreferences)
EVT_COMMAND(wxID_ANY, EVT_PROJECT_LOADED, MainWindow::OnProjectLoaded)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_SELECTED, MainWindow::OnLayoutSelected)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_VIEW_EDIT, MainWindow::OnLayoutViewEdit)
EVT_COMMAND(wxID_ANY, EVT_LAYOUT_RENDER_READY, MainWindow::OnLayoutRenderReady)
wxEND_EVENT_TABLE()

                                                                    MainWindow::
                                                                        MainWindow(
                                                                            const wxString
                                                                                &title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 950)) {
  SetInstance(this);
  wxFont defaultUiFont = BuildDefaultUiFont();
  if (defaultUiFont.IsOk())
    SetFont(defaultUiFont);
  wxIcon icon;
  const std::filesystem::path resourceRoot = ProjectUtils::GetResourceRoot();
  std::filesystem::path iconPath;
  if (!resourceRoot.empty())
    iconPath = resourceRoot / "Perastage.ico";
  std::error_code ec;
  if (!iconPath.empty() && std::filesystem::exists(iconPath, ec)) {
    icon.LoadFile(iconPath.string(), wxBITMAP_TYPE_ICO);
  } else {
    LogMissingIcon(
        iconPath.empty() ? std::filesystem::path("resources/Perastage.ico")
                         : iconPath);
  }
  if (!icon.IsOk())
    icon = wxArtProvider::GetIcon(wxART_MISSING_IMAGE);
  if (icon.IsOk())
    SetIcon(icon);

  Centre();
  SetupLayout();
  // Ensure the 3D viewport is available even before a project is loaded.
  Ensure3DViewport();

  ApplySavedLayout();

  // Apply camera settings after layout and config are ready
  if (viewportPanel)
    viewportPanel->LoadCameraFromConfig();

  if (layerPanel)
    layerPanel->ReloadLayers();
  if (layoutPanel)
    layoutPanel->ReloadLayouts();

  UpdateTitle();
}

MainWindow::~MainWindow() {
  SaveUserConfigWithViewport2DState();
  if (auiManager) {
    auiManager->UnInit();
    delete auiManager;
    auiManager = nullptr;
  }
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
    ConfigManager &cfg = ConfigManager::Get();
    defaultLayoutPerspective = auiManager->SavePerspective().ToStdString();
    if (!cfg.HasKey("layout_default"))
      cfg.SetValue("layout_default", defaultLayoutPerspective);
    else if (auto val = cfg.GetValue("layout_default"))
      defaultLayoutPerspective = *val;
  }
}

void MainWindow::Ensure2DViewport() {
  if (viewport2DPanel)
    return;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewport2DPanel = new Viewer2DPanel(this);
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
                                                 .Row(1)
                                                 .Position(1)
                                                 .BestSize(200, 100)
                                                 .CloseButton(true)
                                                 .MaximizeButton(true)
                                                 .PaneBorder(true)
                                                 .Hide());

  auiManager->Update();

  if (default2DLayoutPerspective.empty()) {
    auto &pane3d = auiManager->GetPane("3DViewport");
    auto &pane2d = auiManager->GetPane("2DViewport");
    auto &paneRender = auiManager->GetPane("2DRenderOptions");
    pane3d.Hide();
    pane2d.Show();
    paneRender.Show();
    auiManager->Update();
    default2DLayoutPerspective = auiManager->SavePerspective().ToStdString();
    paneRender.Hide();
    pane2d.Hide();
    pane3d.Show();
    auiManager->Update();
  }
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

bool MainWindow::ConfirmSaveIfDirty(const wxString &actionLabel,
                                    const wxString &dialogTitle) {
  if (!ConfigManager::Get().IsDirty())
    return true;

  wxMessageDialog dlg(
      this,
      wxString::Format("Do you want to save changes before %s?", actionLabel),
      dialogTitle, wxYES_NO | wxCANCEL | wxICON_QUESTION);

  int res = dlg.ShowModal();
  if (res == wxID_YES) {
    wxCommandEvent saveEvt;
    OnSave(saveEvt);
    return !ConfigManager::Get().IsDirty();
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

bool MainWindow::LoadProjectFromPath(const std::string &path) {
  if (!ConfigManager::Get().LoadProject(path))
    return false;

  Ensure3DViewport();

  currentProjectPath = path;
  ProjectUtils::SaveLastProjectPath(currentProjectPath);

  ApplySavedLayout();
  if (layoutPanel)
    layoutPanel->ReloadLayouts();

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
  if (viewportPanel) {
    ConfigManager &cfg = ConfigManager::Get();
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
  ConfigManager::Get().MarkSaved();
  UpdateTitle();
  return true;
}

void MainWindow::ResetProject() {
  ConfigManager::Get().Reset();
  ConfigManager::Get().MarkSaved();
  currentProjectPath.clear();
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

void MainWindow::UpdateTitle() {
  wxString title = "Perastage";
  if (!currentProjectPath.empty()) {
    wxFileName fn(wxString::FromUTF8(currentProjectPath));
    title += " - " + fn.GetName();
  } else {
    title += " - Untitled";
  }
  SetTitle(title);
}

void MainWindow::SaveCameraSettings() {
  if (layoutModeActive)
    PersistLayout2DViewState();
  if (viewportPanel) {
    Viewer3DCamera &cam = viewportPanel->GetCamera();
    ConfigManager::Get().SetFloat("camera_yaw", cam.GetYaw());
    ConfigManager::Get().SetFloat("camera_pitch", cam.GetPitch());
    ConfigManager::Get().SetFloat("camera_distance", cam.GetDistance());
    ConfigManager::Get().SetFloat("camera_target_x", cam.GetTargetX());
    ConfigManager::Get().SetFloat("camera_target_y", cam.GetTargetY());
    ConfigManager::Get().SetFloat("camera_target_z", cam.GetTargetZ());
  }
  if (viewport2DPanel)
    viewport2DPanel->SaveViewToConfig();
  if (auiManager) {
    const std::string perspective =
        auiManager->SavePerspective().ToStdString();
    if (!layoutModeActive)
      ConfigManager::Get().SetValue("layout_perspective", perspective);
  }
}

void MainWindow::SaveUserConfigWithViewport2DState() {
  ConfigManager &cfg = ConfigManager::Get();
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
}

void MainWindow::OnLayoutSelected(wxCommandEvent &event) {
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

void MainWindow::ShowLayoutLoadingIndicator(const wxString &message) {
  if (auiManager) {
    auto &layoutPane = auiManager->GetPane("LayoutViewer");
    if (layoutPane.IsOk() && !layoutPane.IsShown())
      return;
  }
  if (layoutViewerPanel && !layoutViewerPanel->IsShownOnScreen())
    return;
  if (GetStatusBar())
    SetStatusText(message);
  if (!layoutRenderCursor)
    layoutRenderCursor = std::make_unique<wxBusyCursor>();
}

void MainWindow::ClearLayoutLoadingIndicator() {
  if (GetStatusBar())
    SetStatusText("");
  layoutRenderCursor.reset();
}

void MainWindow::OnLayoutRenderReady(wxCommandEvent &) {
  ClearLayoutLoadingIndicator();
}

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
    if (!hasViewId && selectedLayout &&
        !selectedLayout->view2dViews.empty()) {
      viewId = selectedLayout->view2dViews.front().id;
      hasViewId = viewId > 0;
    }
    if (hasViewId)
      RestoreLayout2DViewState(viewId);
  }

  if (layoutModeActive)
    ApplyLayoutModePerspective();
}

void MainWindow::SyncSceneData() {
  if (fixturePanel)
    fixturePanel->UpdateSceneData();
  if (trussPanel)
    trussPanel->UpdateSceneData();
  if (hoistPanel)
    hoistPanel->UpdateSceneData();
  if (sceneObjPanel)
    sceneObjPanel->UpdateSceneData();
}

void MainWindow::RefreshAfterSceneChange(bool refreshViewport) {
  if (fixturePanel)
    fixturePanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  RefreshSummary();
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

void MainWindow::OnProjectLoaded(wxCommandEvent &event) {
  bool loaded = event.GetInt() != 0;
  bool clearLastProject = event.GetExtraLong() != 0;
  std::string path = event.GetString().ToStdString();
  Ensure3DViewport();
  if (clearLastProject)
    ProjectUtils::SaveLastProjectPath("");
  if (loaded && !path.empty()) {
    currentProjectPath = path;
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    ApplySavedLayout();
    if (layoutPanel)
      layoutPanel->ReloadLayouts();
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
    if (viewportPanel) {
      ConfigManager &cfg = ConfigManager::Get();
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
    if (layerPanel)
      layerPanel->ReloadLayers();
    RefreshSummary();
    RefreshRigging();
    ConfigManager::Get().MarkSaved();
    UpdateTitle();
  } else {
    ResetProject();
  }
  SplashScreen::SetMessage("Ready");
  SplashScreen::Hide();
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
  if (enable)
    SetAcceleratorTable(m_accel);
  else
    SetAcceleratorTable(wxAcceleratorTable());
}
