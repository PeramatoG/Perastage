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
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <random>
#include <string>
#include <thread>
#include <tinyxml2.h>
#include <wx/aboutdlg.h>
#include <wx/app.h>
#include <wx/event.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/html/htmlwin.h>
#include <wx/iconbndl.h>
#include <wx/notebook.h>
#include <wx/numdlg.h>
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/log.h>
#include <wx/zipstrm.h>

#include "../external/json.hpp"

using json = nlohmann::json;
#include "addfixturedialog.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "credentialstore.h"
#include "exportfixturedialog.h"
#include "exportobjectdialog.h"
#include "exporttrussdialog.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "gdtfsearchdialog.h"
#include "layerpanel.h"
#include "logindialog.h"
#include "markdown.h"
#include "hoisttablepanel.h"
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
#include "print/PlanPrintSettings.h"
#include "planpdfexporter.h"
#include "print_diagnostics.h"
#include "support.h"
#include "trussloader.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

MainWindow *MainWindow::s_instance = nullptr;
wxDEFINE_EVENT(EVT_PROJECT_LOADED, wxCommandEvent);

MainWindow *MainWindow::Instance() { return s_instance; }

void MainWindow::SetInstance(MainWindow *inst) { s_instance = inst; }

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame) EVT_MENU(
    ID_File_New,
    MainWindow::
        OnNew) EVT_MENU(ID_File_Load,
                        MainWindow::
                            OnLoad) EVT_MENU(ID_File_Save,
                                             MainWindow::
                                                 OnSave) EVT_MENU(ID_File_SaveAs,
                                                                  MainWindow::
                                                                      OnSaveAs)
    EVT_MENU(ID_File_ImportRider, MainWindow::OnImportRider) EVT_MENU(
        ID_File_ImportMVR,
        MainWindow::
            OnImportMVR) EVT_MENU(ID_File_ExportMVR,
                                  MainWindow::
                                      OnExportMVR) EVT_MENU(ID_File_PrintPlan,
                                                            MainWindow::
                                                                OnPrintPlan)
        EVT_MENU(ID_File_PrintTable, MainWindow::OnPrintTable) EVT_MENU(
            ID_File_ExportCSV, MainWindow::OnExportCSV) EVT_MENU(
            ID_File_Close,
            MainWindow::
                OnClose) EVT_CLOSE(MainWindow::
                                       OnCloseWindow) EVT_MENU(ID_Edit_Undo,
                                                               MainWindow::
                                                                   OnUndo)
            EVT_MENU(ID_Edit_Redo, MainWindow::OnRedo) EVT_MENU(
                ID_Edit_AddFixture,
                MainWindow::
                    OnAddFixture) EVT_MENU(ID_Edit_AddTruss,
                                           MainWindow::
                                               OnAddTruss) EVT_MENU(ID_Edit_AddSceneObject,
                                                                    MainWindow::
                                                                        OnAddSceneObject)
                EVT_MENU(ID_Edit_Delete, MainWindow::OnDelete) EVT_MENU(
                    ID_View_ToggleConsole,
                    MainWindow::
                        OnToggleConsole) EVT_MENU(ID_View_ToggleFixtures,
                                                  MainWindow::OnToggleFixtures)
                    EVT_MENU(ID_View_ToggleViewport, MainWindow::OnToggleViewport) EVT_MENU(
                        ID_View_ToggleViewport2D,
                        MainWindow::
                            OnToggleViewport2D) EVT_MENU(ID_View_ToggleRender2D,
                                                         MainWindow::
                                                             OnToggleRender2D)
                        EVT_MENU(ID_View_ToggleLayers, MainWindow::OnToggleLayers) EVT_MENU(
                            ID_View_ToggleSummary,
                            MainWindow::
                                OnToggleSummary) EVT_MENU(ID_View_ToggleRigging,
                                                          MainWindow::OnToggleRigging) EVT_MENU(
                            ID_View_Layout_Default,
                                                      MainWindow::
                                                          OnApplyDefaultLayout)
                            EVT_MENU(
                                ID_View_Layout_2D,
                                MainWindow::
                                    OnApply2DLayout) EVT_MENU(ID_Tools_DownloadGdtf,
                                                              MainWindow::
                                                                  OnDownloadGdtf)
                                EVT_MENU(
                                    ID_Tools_ExportFixture,
                                    MainWindow::
                                        OnExportFixture) EVT_MENU(ID_Tools_ExportTruss,
                                                                  MainWindow::
                                                                      OnExportTruss)
                                    EVT_MENU(
                                        ID_Tools_ExportSceneObject,
                                        MainWindow::
                                      OnExportSceneObject) EVT_MENU(ID_Tools_AutoPatch,
                                                                          MainWindow::
                                                                              OnAutoPatch) EVT_MENU(ID_Tools_AutoColor,
                                                                          MainWindow::
                                                                              OnAutoColor) EVT_MENU(ID_Tools_ConvertToHoist,
                                                                          MainWindow::
                                                                              OnConvertToHoist)
                                        EVT_MENU(ID_Tools_ImportRiderText,
                                                 MainWindow::OnImportRiderText)
                                        EVT_MENU(ID_Help_Help, MainWindow::OnShowHelp) EVT_MENU(
                                            ID_Help_About, MainWindow::
                                                               OnShowAbout)
                                            EVT_MENU(ID_Select_Fixtures,
                                                     MainWindow::
                                                         OnSelectFixtures)
                                                EVT_MENU(ID_Select_Trusses,
                                                         MainWindow::
                                                             OnSelectTrusses)
                                                    EVT_MENU(ID_Select_Supports,
                                                             MainWindow::
                                                                 OnSelectSupports)
                                                        EVT_MENU(
                                                            ID_Select_Objects,
                                                            MainWindow::
                                                                OnSelectObjects)
                                                        EVT_MENU(
                                                            ID_Edit_Preferences,
                                                            MainWindow::
                                                                OnPreferences)
                                                            EVT_COMMAND(
                                                                wxID_ANY,
                                                                EVT_PROJECT_LOADED,
                                                                MainWindow::
                                                                    OnProjectLoaded)
                                                                wxEND_EVENT_TABLE()

                                                                    MainWindow::
                                                                        MainWindow(
                                                                            const wxString
                                                                                &title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 950)) {
  SetInstance(this);
  wxIcon icon;
  const char *iconPaths[] = {"resources/Perastage.ico",
                             "../resources/Perastage.ico",
                             "../../resources/Perastage.ico"};
  for (const char *path : iconPaths) {
    if (icon.LoadFile(path, wxBITMAP_TYPE_ICO))
      break;
  }
  if (icon.IsOk())
    SetIcon(icon);

  Centre();
  SetupLayout();

  ApplySavedLayout();

  // Apply camera settings after layout and config are ready
  if (viewportPanel)
    viewportPanel->LoadCameraFromConfig();

  if (layerPanel)
    layerPanel->ReloadLayers();

  UpdateTitle();
}

MainWindow::~MainWindow() {
  SaveCameraSettings();
  if (auiManager) {
    auiManager->UnInit();
    delete auiManager;
    auiManager = nullptr;
  }
  ConfigManager::Get().SaveUserConfig();
  ProjectUtils::SaveLastProjectPath(currentProjectPath);
}

void MainWindow::SetupLayout() {
  CreateMenuBar();

  // Initialize AUI manager for dynamic pane layout
  auiManager = new wxAuiManager(this);
  Bind(wxEVT_AUI_PANE_CLOSE, &MainWindow::OnPaneClose, this);

  // Create notebook with data panels
  notebook = new wxNotebook(this, wxID_ANY);
  notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,
                 &MainWindow::OnNotebookPageChanged, this);

  fixturePanel = new FixtureTablePanel(notebook);
  FixtureTablePanel::SetInstance(fixturePanel);
  notebook->AddPage(fixturePanel, "Fixtures");

  trussPanel = new TrussTablePanel(notebook);
  TrussTablePanel::SetInstance(trussPanel);
  notebook->AddPage(trussPanel, "Trusses");

  hoistPanel = new HoistTablePanel(notebook);
  HoistTablePanel::SetInstance(hoistPanel);
  notebook->AddPage(hoistPanel, "Hoists");

  sceneObjPanel = new SceneObjectTablePanel(notebook);
  SceneObjectTablePanel::SetInstance(sceneObjPanel);
  notebook->AddPage(sceneObjPanel, "Objects");

  // Add notebook on the left so the viewport can occupy
  // the remaining (and larger) central area
  int halfWidth = GetClientSize().GetWidth() / 2;

  auiManager->AddPane(notebook, wxAuiPaneInfo()
                                    .Name("DataNotebook")
                                    .Caption("Data Views")
                                    .Left()
                                    .BestSize(halfWidth, 600)
                                    .MinSize(wxSize(200, 300))
                                    .PaneBorder(false)
                                    .CaptionVisible(true)
                                    .CloseButton(true)
                                    .MaximizeButton(true));

  // Bottom console panel for messages
  consolePanel = new ConsolePanel(this);
  ConsolePanel::SetInstance(consolePanel);
  auiManager->AddPane(consolePanel, wxAuiPaneInfo()
                                        .Name("Console")
                                        .Caption("Console")
                                        .Bottom()
                                        .BestSize(-1, 150)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  layerPanel = new LayerPanel(this);
  LayerPanel::SetInstance(layerPanel);
  auiManager->AddPane(layerPanel, wxAuiPaneInfo()
                                      .Name("LayerPanel")
                                      .Caption("Layers")
                                      .Right()
                                      .BestSize(200, 300)
                                      .CloseButton(true)
                                      .MaximizeButton(true)
                                      .PaneBorder(true));

  summaryPanel = new SummaryPanel(this);
  SummaryPanel::SetInstance(summaryPanel);
  auiManager->AddPane(summaryPanel, wxAuiPaneInfo()
                                        .Name("SummaryPanel")
                                        .Caption("Summary")
                                        .Right()
                                        .Row(1)
                                        .Position(0)
                                        .BestSize(200, 150)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  riggingPanel = new RiggingPanel(this);
  RiggingPanel::SetInstance(riggingPanel);
  auiManager->AddPane(riggingPanel, wxAuiPaneInfo()
                                        .Name("RiggingPanel")
                                        .Caption("Rigging")
                                        .Right()
                                        .Row(1)
                                        .Position(1)
                                        .BestSize(250, 200)
                                        .CloseButton(true)
                                        .MaximizeButton(true)
                                        .PaneBorder(true));

  // Apply all changes to layout
  auiManager->Update();

  if (summaryPanel)
    summaryPanel->ShowFixtureSummary();
  if (riggingPanel)
    riggingPanel->RefreshData();

  // Keyboard shortcuts to switch notebook pages
  wxAcceleratorEntry entries[4];
  entries[0].Set(wxACCEL_NORMAL, (int)'1', ID_Select_Fixtures);
  entries[1].Set(wxACCEL_NORMAL, (int)'2', ID_Select_Trusses);
  entries[2].Set(wxACCEL_NORMAL, (int)'3', ID_Select_Supports);
  entries[3].Set(wxACCEL_NORMAL, (int)'4', ID_Select_Objects);
  m_accel = wxAcceleratorTable(4, entries);
  SetAcceleratorTable(m_accel);

  // Ensure the View menu reflects the actual pane visibility
  UpdateViewMenuChecks();
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

void MainWindow::CreateMenuBar() {
  wxMenuBar *menuBar = new wxMenuBar();

  // File menu
  wxMenu *fileMenu = new wxMenu();
  fileMenu->Append(ID_File_New, "New\tCtrl+N");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Load, "Load\tCtrl+L");
  fileMenu->Append(ID_File_Save, "Save\tCtrl+S");
  fileMenu->Append(ID_File_SaveAs, "Save As...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_ImportMVR, "Import MVR...");
  fileMenu->Append(ID_File_ExportMVR, "Export MVR...");
  fileMenu->Append(ID_File_PrintPlan, "Print Plan...");
  fileMenu->Append(ID_File_PrintTable, "Print Table...");
  fileMenu->Append(ID_File_ExportCSV, "Export CSV...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Close, "Close\tCtrl+Q");

  menuBar->Append(fileMenu, "&File");

  // Edit menu
  wxMenu *editMenu = new wxMenu();
  editMenu->Append(ID_Edit_Undo, "Undo\tCtrl+Z");
  editMenu->Append(ID_Edit_Redo, "Redo\tCtrl+Y");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_AddFixture, "Add fixture...");
  editMenu->Append(ID_Edit_AddTruss, "Add truss...");
  editMenu->Append(ID_Edit_AddSceneObject, "Add scene object...");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Delete, "Delete\tDel");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Preferences, "Preferences...");

  menuBar->Append(editMenu, "&Edit");

  // View menu for toggling panels
  wxMenu *viewMenu = new wxMenu();
  viewMenu->AppendCheckItem(ID_View_ToggleConsole, "Console");
  viewMenu->AppendCheckItem(ID_View_ToggleFixtures, "Fixtures");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport, "3D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport2D, "2D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleRender2D, "2D Render Options");
  viewMenu->AppendCheckItem(ID_View_ToggleLayers, "Layers");
  viewMenu->AppendCheckItem(ID_View_ToggleSummary, "Summary");
  viewMenu->AppendCheckItem(ID_View_ToggleRigging, "Rigging");

  wxMenu *layoutMenu = new wxMenu();
  layoutMenu->Append(ID_View_Layout_Default, "3D Layout");
  layoutMenu->Append(ID_View_Layout_2D, "2D Layout");
  viewMenu->AppendSubMenu(layoutMenu, "Layout");

  menuBar->Append(viewMenu, "&View");

  // Tools menu
  wxMenu *toolsMenu = new wxMenu();
  toolsMenu->Append(ID_Tools_DownloadGdtf, "Download GDTF fixture...");
  toolsMenu->Append(ID_Tools_ImportRiderText, "Create from text...");
  toolsMenu->Append(ID_Tools_ExportFixture, "Export Fixture...");
  toolsMenu->Append(ID_Tools_ExportTruss, "Export Truss...");
  toolsMenu->Append(ID_Tools_ExportSceneObject, "Export Scene Object...");
  toolsMenu->Append(ID_Tools_AutoPatch, "Auto patch");
  toolsMenu->Append(ID_Tools_AutoColor, "Auto color");
  toolsMenu->Append(ID_Tools_ConvertToHoist, "Convert to Hoist");

  menuBar->Append(toolsMenu, "&Tools");

  // Help menu
  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(ID_Help_Help, "Help\tF1");
  helpMenu->Append(ID_Help_About, "About");

  menuBar->Append(helpMenu, "&Help");

  SetMenuBar(menuBar);
}

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

void MainWindow::OnNew(wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmSaveIfDirty("creating a new project", "New Project"))
    return;

  ResetProject();
}

void MainWindow::OnLoad(wxCommandEvent &event) {
  if (!ConfirmSaveIfDirty("loading a project", "Open Project"))
    return;

  wxString filter = wxString::Format("Perastage files (*%s)|*%s",
                                     ProjectUtils::PROJECT_EXTENSION,
                                     ProjectUtils::PROJECT_EXTENSION);
  wxString projDir;
  if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("projects"));
  wxFileDialog dlg(this, "Open Project", projDir, "", filter,
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  wxString path = dlg.GetPath();
  if (!LoadProjectFromPath(path.ToStdString()))
    wxMessageBox("Failed to load project.", "Error", wxICON_ERROR);
}

void MainWindow::OnSave(wxCommandEvent &event) {
  if (currentProjectPath.empty()) {
    OnSaveAs(event);
    return;
  }
  SyncSceneData();
  SaveCameraSettings();
  ConfigManager::Get().SaveUserConfig();
  if (!ConfigManager::Get().SaveProject(currentProjectPath))
    wxMessageBox("Failed to save project.", "Error", wxICON_ERROR);
  else {
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("Saved " +
                                  wxString::FromUTF8(currentProjectPath));
  }
}

void MainWindow::OnSaveAs(wxCommandEvent &event) {
  wxString filter = wxString::Format("Perastage files (*%s)|*%s",
                                     ProjectUtils::PROJECT_EXTENSION,
                                     ProjectUtils::PROJECT_EXTENSION);
  wxString projDir;
  if (!currentProjectPath.empty())
    projDir = wxString::FromUTF8(
        std::filesystem::path(currentProjectPath).parent_path().string());
  else if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("projects"));
  wxFileDialog dlg(this, "Save Project", projDir, "", filter,
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  currentProjectPath = dlg.GetPath().ToStdString();
  SyncSceneData();
  SaveCameraSettings();
  ConfigManager::Get().SaveUserConfig();
  if (!ConfigManager::Get().SaveProject(currentProjectPath))
    wxMessageBox("Failed to save project.", "Error", wxICON_ERROR);
  else {
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("Saved " + dlg.GetPath());
  }
  UpdateTitle();
}

// Import fixtures and trusses from a rider (.txt/.pdf)
void MainWindow::OnImportRider(wxCommandEvent &event) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog dlg(this, "Import Rider", miscDir, "",
                   "Rider files (*.txt;*.pdf)|*.txt;*.pdf",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  std::string pathUtf8 = dlg.GetPath().ToStdString();
  if (!RiderImporter::Import(pathUtf8)) {
    wxMessageBox("Failed to import rider.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Failed to import " + dlg.GetPath());
  } else {
    wxMessageBox("Rider imported successfully.", "Success", wxICON_INFORMATION);
    if (consolePanel)
      consolePanel->AppendMessage("Imported " + dlg.GetPath());
    if (fixturePanel)
      fixturePanel->ReloadData();
    if (trussPanel)
      trussPanel->ReloadData();
    if (hoistPanel)
      hoistPanel->ReloadData();
    if (viewportPanel) {
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
    RefreshSummary();
  }
}

void MainWindow::OnImportRiderText(wxCommandEvent &WXUNUSED(event)) {
  RiderTextDialog dlg(this);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (consolePanel)
    consolePanel->AppendMessage("Imported rider from text.");
  if (fixturePanel)
    fixturePanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

// Handles MVR file selection, import, and updates fixture/truss panels
// accordingly
void MainWindow::OnImportMVR(wxCommandEvent &event) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog openFileDialog(this, "Import MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (openFileDialog.ShowModal() == wxID_CANCEL)
    return;

  wxString filePath = openFileDialog.GetPath();
  std::string pathUtf8 = filePath.ToUTF8().data();
  if (!MvrImporter::ImportAndRegister(pathUtf8)) {
    wxMessageBox("Failed to import MVR file.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Failed to import " + filePath);
  } else {
    wxMessageBox("MVR file imported successfully.", "Success",
                 wxICON_INFORMATION);
    if (consolePanel)
      consolePanel->AppendMessage("Imported " + filePath);
    if (fixturePanel)
      fixturePanel->ReloadData();
    if (trussPanel)
      trussPanel->ReloadData();
    if (hoistPanel)
      hoistPanel->ReloadData();
    if (sceneObjPanel)
      sceneObjPanel->ReloadData();
    RefreshSummary();
    if (viewportPanel) {
      viewportPanel->UpdateScene();
      viewportPanel->Refresh();
    }
  }
}

void MainWindow::OnExportMVR(wxCommandEvent &event) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog saveFileDialog(this, "Export MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (saveFileDialog.ShowModal() == wxID_CANCEL)
    return;

  SyncSceneData();
  MvrExporter exporter;
  wxString path = saveFileDialog.GetPath();
  if (!exporter.ExportToFile(path.ToStdString())) {
    wxMessageBox("Failed to export MVR file.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Failed to export " + path);
  } else {
    wxMessageBox("MVR file exported successfully.", "Success",
                 wxICON_INFORMATION);
    if (consolePanel)
      consolePanel->AppendMessage("Exported " + path);
  }
}

void MainWindow::OnDownloadGdtf(wxCommandEvent &WXUNUSED(event)) {
  std::string savedUser;
  std::string savedPass;
  if (auto creds = CredentialStore::Load()) {
    savedUser = creds->username;
    savedPass = creds->password;
  } else {
    std::string savedPassEnc =
        ConfigManager::Get().GetValue("gdtf_password").value_or("");
    savedUser = ConfigManager::Get().GetValue("gdtf_username").value_or("");
    savedPass = SimpleCrypt::Decode(savedPassEnc);
  }

  GdtfLoginDialog loginDlg(this, savedUser, savedPass);
  if (loginDlg.ShowModal() != wxID_OK)
    return;
  wxString username =
      wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false);
  wxString password = wxString::FromUTF8(loginDlg.GetPassword());
  ConfigManager::Get().SetValue("gdtf_username",
                                std::string(username.mb_str()));
  ConfigManager::Get().SetValue(
      "gdtf_password", SimpleCrypt::Encode(std::string(password.mb_str())));
  CredentialStore::Save(
      {std::string(username.mb_str()), std::string(password.mb_str())});

  if (!currentProjectPath.empty())
    ConfigManager::Get().SaveProject(currentProjectPath);

  wxString cookieFileWx = wxFileName::GetTempDir() + "/gdtf_session.txt";
  std::string cookieFile = std::string(cookieFileWx.mb_str());
  long httpCode = 0;
  if (consolePanel)
    consolePanel->AppendMessage("Logging into GDTF Share using libcurl");
  if (!GdtfLogin(std::string(username.mb_str()), std::string(password.mb_str()),
                 cookieFile, httpCode)) {
    wxMessageBox("Failed to connect to GDTF Share.", "Login Error",
                 wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Login connection failed");
    return;
  }
  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("Login HTTP code: %ld", httpCode));
  if (httpCode != 200) {
    wxMessageBox("Login failed.", "Login Error", wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Login failed with code " +
                                  wxString::Format("%ld", httpCode));
    return;
  }

  if (consolePanel)
    consolePanel->AppendMessage("Retrieving fixture list via libcurl");
  std::string listData;
  if (!GdtfGetList(cookieFile, listData)) {
    wxMessageBox("Failed to retrieve fixture list.", "Error",
                 wxOK | wxICON_ERROR);
    return;
  }

  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("Retrieved list size: %zu bytes", listData.size()));

  ConfigManager::Get().SetValue("gdtf_fixture_list", listData);

  // open search dialog
  GdtfSearchDialog searchDlg(this, listData);
  if (searchDlg.ShowModal() == wxID_OK) {
    wxString rid = wxString::FromUTF8(searchDlg.GetSelectedId());
    wxString name = wxString::FromUTF8(searchDlg.GetSelectedName());

    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog saveDlg(this, "Save GDTF file", fixDir, name + ".gdtf",
                         "*.gdtf", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() == wxID_OK) {
      wxString dest = saveDlg.GetPath();
      if (!rid.empty()) {
        if (consolePanel)
          consolePanel->AppendMessage("Downloading via libcurl rid=" + rid);
        long dlCode = 0;
        bool ok = GdtfDownload(std::string(rid.mb_str()),
                               std::string(dest.mb_str()), cookieFile, dlCode);
        if (consolePanel)
          consolePanel->AppendMessage(
              wxString::Format("Download HTTP code: %ld", dlCode));
        if (ok && dlCode == 200)
          wxMessageBox("GDTF downloaded.", "Success",
                       wxOK | wxICON_INFORMATION);
        else
          wxMessageBox("Failed to download GDTF.", "Error",
                       wxOK | wxICON_ERROR);
      } else {
        wxMessageBox("Download information missing.", "Error",
                     wxOK | wxICON_ERROR);
      }
    }
  }

  wxRemoveFile(cookieFileWx);
}

void MainWindow::OnExportTruss(wxCommandEvent &WXUNUSED(event)) {
  const auto &trusses = ConfigManager::Get().GetScene().trusses;
  std::set<std::string> names;
  for (const auto &[uuid, t] : trusses)
    names.insert(t.name);
  if (names.empty()) {
    wxMessageBox("No truss data available.", "Export Truss",
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(names.begin(), names.end());
  ExportTrussDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedName();
  const Truss *chosen = nullptr;
  for (const auto &[uuid, t] : trusses) {
    if (t.name == sel) {
      chosen = &t;
      break;
    }
  }
  if (!chosen)
    return;

  wxString trussDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
  wxFileDialog saveDlg(this, "Save Truss", trussDir,
                       wxString::FromUTF8(sel) + ".gtruss", "*.gtruss",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  namespace fs = std::filesystem;
  std::string modelPath = chosen->symbolFile;
  const auto &scene = ConfigManager::Get().GetScene();
  if (fs::path(modelPath).is_relative() && !scene.basePath.empty())
    modelPath = (fs::path(scene.basePath) / modelPath).string();
  if (!fs::exists(modelPath)) {
    wxMessageBox("Model file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxFileOutputStream out(std::string(saveDlg.GetPath().mb_str()));
  if (!out.IsOk()) {
    wxMessageBox("Failed to write file.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxZipOutputStream zip(out);
  json j = {
      {"Name", chosen->name},          {"Manufacturer", chosen->manufacturer},
      {"Model", chosen->model},        {"Length_mm", chosen->lengthMm},
      {"Width_mm", chosen->widthMm},   {"Height_mm", chosen->heightMm},
      {"Weight_kg", chosen->weightKg}, {"CrossSection", chosen->crossSection}};
  std::string meta = j.dump(2);
  auto *metaEntry = new wxZipEntry("Truss.json");
  metaEntry->SetMethod(wxZIP_METHOD_DEFLATE);
  zip.PutNextEntry(metaEntry);
  zip.Write(meta.c_str(), meta.size());

  fs::path mp(modelPath);
  auto *modelEntry = new wxZipEntry(mp.filename().string());
  modelEntry->SetMethod(wxZIP_METHOD_DEFLATE);
  zip.PutNextEntry(modelEntry);
  std::ifstream modelIn(modelPath, std::ios::binary);
  if (!modelIn.is_open()) {
    wxMessageBox("Failed to read model file.", "Error", wxOK | wxICON_ERROR);
    return;
  }
  char buffer[4096];
  while (true) {
    modelIn.read(buffer, sizeof(buffer));
    std::streamsize bytes = modelIn.gcount();
    if (bytes <= 0)
      break;
    zip.Write(buffer, bytes);
  }
  modelIn.close();

  wxMessageBox("Truss exported successfully.", "Export Truss",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportFixture(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  auto createTempDir = []() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string folderName = "GDTF_" + std::to_string(now);
    fs::path base = fs::temp_directory_path();
    fs::path full = base / folderName;
    fs::create_directory(full);
    return full.string();
  };
  auto extractZip = [](const std::string &zipPath, const std::string &destDir) {
    if (!fs::exists(zipPath)) {
      if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("GDTF: cannot open %s",
                                        wxString::FromUTF8(zipPath));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
      return false;
    }
    wxLogNull logNo;
    wxFileInputStream input(zipPath);
    if (!input.IsOk()) {
      if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("GDTF: cannot open %s",
                                        wxString::FromUTF8(zipPath));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
      return false;
    }
    wxZipInputStream zipStream(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipStream.GetNextEntry())), entry) {
      std::string filename = entry->GetName().ToStdString();
      std::string fullPath = destDir + "/" + filename;
      if (entry->IsDir()) {
        wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        continue;
      }
      wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      std::ofstream output(fullPath, std::ios::binary);
      if (!output.is_open())
        return false;
      char buffer[4096];
      while (true) {
        zipStream.Read(buffer, sizeof(buffer));
        size_t bytes = zipStream.LastRead();
        if (bytes == 0)
          break;
        output.write(buffer, bytes);
      }
      output.close();
    }
    return true;
  };
  const auto &fixtures = ConfigManager::Get().GetScene().fixtures;
  std::set<std::string> types;
  for (const auto &[uuid, f] : fixtures)
    if (!f.typeName.empty())
      types.insert(f.typeName);
  if (types.empty()) {
    wxMessageBox("No fixture data available.", "Export Fixture",
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

  fs::path src = chosen->gdtfSpec;
  const std::string &base = ConfigManager::Get().GetScene().basePath;
  if (src.is_relative() && !base.empty())
    src = fs::path(base) / src;
  if (!fs::exists(src)) {
    wxMessageBox("GDTF file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  wxFileDialog saveDlg(this, "Save Fixture", fixDir,
                       wxString::FromUTF8(sel) + ".gdtf", "*.gdtf",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  std::string tempDir = createTempDir();
  if (!extractZip(src.string(), tempDir)) {
    wxMessageBox("Failed to read GDTF.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  fs::path descPath = fs::path(tempDir) / "description.xml";
  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.string().c_str()) != tinyxml2::XML_SUCCESS) {
    fs::remove_all(tempDir);
    wxMessageBox("Failed to parse description.xml.", "Error",
                 wxOK | wxICON_ERROR);
    return;
  }

  tinyxml2::XMLElement *ft = doc.FirstChildElement("GDTF");
  if (ft)
    ft = ft->FirstChildElement("FixtureType");
  else
    ft = doc.FirstChildElement("FixtureType");
  if (!ft) {
    fs::remove_all(tempDir);
    wxMessageBox("Invalid GDTF file.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  tinyxml2::XMLElement *phys = ft->FirstChildElement("PhysicalDescriptions");
  if (!phys)
    phys =
        ft->InsertEndChild(doc.NewElement("PhysicalDescriptions"))->ToElement();
  tinyxml2::XMLElement *props = phys->FirstChildElement("Properties");
  if (!props)
    props = phys->InsertEndChild(doc.NewElement("Properties"))->ToElement();

  if (chosen->weightKg != 0.0f) {
    tinyxml2::XMLElement *w = props->FirstChildElement("Weight");
    if (!w)
      w = props->InsertEndChild(doc.NewElement("Weight"))->ToElement();
    w->SetAttribute("Value", chosen->weightKg);
  }

  if (chosen->powerConsumptionW != 0.0f) {
    tinyxml2::XMLElement *pc = props->FirstChildElement("PowerConsumption");
    if (!pc)
      pc = props->InsertEndChild(doc.NewElement("PowerConsumption"))
               ->ToElement();
    pc->SetAttribute("Value", chosen->powerConsumptionW);
  }

  doc.SaveFile(descPath.string().c_str());

  wxFileOutputStream out(std::string(saveDlg.GetPath().mb_str()));
  if (!out.IsOk()) {
    fs::remove_all(tempDir);
    wxMessageBox("Failed to write file.", "Error", wxOK | wxICON_ERROR);
    return;
  }
  wxZipOutputStream zip(out);
  for (auto &p : fs::recursive_directory_iterator(tempDir)) {
    if (!p.is_regular_file())
      continue;
    std::string rel = fs::relative(p.path(), tempDir).string();
    auto *entry = new wxZipEntry(rel);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    std::ifstream in(p.path(), std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }
  zip.Close();
  fs::remove_all(tempDir);

  wxMessageBox("Fixture exported successfully.", "Export Fixture",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportSceneObject(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  const auto &scene = ConfigManager::Get().GetScene();
  const auto &objs = scene.sceneObjects;
  std::set<std::string> names;
  for (const auto &[uuid, obj] : objs)
    if (!obj.name.empty())
      names.insert(obj.name);
  if (names.empty()) {
    wxMessageBox("No scene objects available.", "Export Scene Object",
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
    wxMessageBox("Model file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxString defName =
      wxString::FromUTF8(sel) + wxString(src.extension().string());
  wxString objDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("scene objects"));
  wxFileDialog saveDlg(this, "Save Object", objDir, defName,
                       wxString("*") + wxString(src.extension().string()),
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  fs::path dest = std::string(saveDlg.GetPath().mb_str());
  std::error_code ec;
  fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    wxMessageBox("Failed to copy file.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxMessageBox("Object exported successfully.", "Export Scene Object",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnAutoPatch(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("auto patch");
  AutoPatcher::AutoPatch(cfg.GetScene());
  if (fixturePanel)
    fixturePanel->ReloadData();
}

void MainWindow::OnAutoColor(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("auto color");
  auto &scene = cfg.GetScene();
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);
  auto randHex = [&]() {
    return wxString::Format("#%02X%02X%02X", dist(rng), dist(rng), dist(rng))
        .ToStdString();
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

  std::map<std::string, std::string> typeColors;
  for (auto &[uuid, f] : scene.fixtures) {
    if (!f.gdtfSpec.empty()) {
      auto it = typeColors.find(f.gdtfSpec);
      if (it == typeColors.end()) {
        std::string c = f.color.empty() ? randHex() : f.color;
        typeColors[f.gdtfSpec] = c;
        f.color = c;
      } else {
        f.color = it->second;
      }
    } else if (f.color.empty()) {
      f.color = randHex();
    }
  }
  for (const auto &[spec, color] : typeColors) {
    std::string gdtfPath = spec;
    std::filesystem::path p = gdtfPath;
    if (p.is_relative() && !scene.basePath.empty())
      gdtfPath = (std::filesystem::path(scene.basePath) / p).string();
    SetGdtfModelColor(gdtfPath, color);
  }

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (layerPanel)
    layerPanel->ReloadLayers();
  if (Viewer3DPanel::Instance()) {
    Viewer3DPanel::Instance()->UpdateScene();
    Viewer3DPanel::Instance()->Refresh();
  }
}

void MainWindow::OnConvertToHoist(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
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
    s.uuid = wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId),
                             idx++)
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
    s.weightKg = fixture.weightKg;
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

  wxMessageBox(wxString::Format("Converted %zu fixture(s) to hoists.",
                                newIds.size()),
               "Convert to Hoist", wxOK | wxICON_INFORMATION);
}

void MainWindow::OnPrintPlan(wxCommandEvent &WXUNUSED(event)) {
  Ensure2DViewport();
  if (!viewport2DPanel) {
    wxMessageBox("2D viewport is not available.", "Print Plan",
                 wxOK | wxICON_ERROR);
    return;
  }

  wxFileDialog dlg(this, "Save plan as", "", "plan.pdf",
                   "PDF files (*.pdf)|*.pdf",
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString outputPathWx = dlg.GetPath();
  outputPathWx.Trim(true).Trim(false);
  if (outputPathWx.empty()) {
    wxMessageBox("Please choose a destination file for the plan.", "Print Plan",
                 wxOK | wxICON_WARNING);
    return;
  }

  print::PlanPrintSettings settings =
      print::PlanPrintSettings::LoadFromConfig(ConfigManager::Get());

  PlanPrintOptions opts; // Defaults to A3 portrait.
  opts.landscape = settings.landscape;
  opts.printIncludeGrid = settings.includeGrid;
  opts.useSimplifiedFootprints = !settings.detailedFootprints;
  opts.pageWidthPt = settings.PageWidthPt();
  opts.pageHeightPt = settings.PageHeightPt();
  std::shared_ptr<const SymbolDefinitionSnapshot> symbolSnapshot = nullptr;
  if (viewport2DPanel) {
    symbolSnapshot = viewport2DPanel->GetBottomSymbolCacheSnapshot();
  }
  std::filesystem::path outputPath(
      std::filesystem::path(outputPathWx.ToStdWstring()));
  wxString outputPathDisplay = outputPathWx;

  viewport2DPanel->CaptureFrameAsync(
      [this, opts, outputPath, outputPathDisplay, symbolSnapshot](
          CommandBuffer buffer, Viewer2DViewState state) {
        if (buffer.commands.empty()) {
          wxMessageBox("Unable to capture the 2D view for printing.",
                       "Print Plan", wxOK | wxICON_ERROR);
          return;
        }

        std::string diagnostics = BuildPrintDiagnostics(buffer);
        wxLogMessage("%s", wxString::FromUTF8(diagnostics));
        bool showDiagnosticsBox =
            ConfigManager::Get().GetFloat("print_plan_diagnostics_popup") !=
            0.0f;
        if (showDiagnosticsBox) {
          wxMessageBox(wxString::FromUTF8(diagnostics), "Print Plan diagnostics",
                       wxOK | wxICON_INFORMATION, this);
        }

        std::string fixtureReport;
        if (viewport2DPanel)
          fixtureReport = viewport2DPanel->GetLastFixtureDebugReport();
        if (!fixtureReport.empty()) {
          wxLogMessage("%s", wxString::FromUTF8(fixtureReport));
          if (showDiagnosticsBox) {
            wxMessageBox(wxString::FromUTF8(fixtureReport),
                         "Print Plan fixture debug", wxOK | wxICON_INFORMATION,
                         this);
          }
        }

        // Run the PDF generation off the UI thread to avoid freezing the
        // window while writing potentially large plans to disk.
        std::thread([this, buffer = std::move(buffer), state, opts, outputPath,
                     outputPathDisplay, symbolSnapshot]() {
          PlanExportResult res =
              ExportPlanToPdf(buffer, state, opts, outputPath, symbolSnapshot);

          wxTheApp->CallAfter([this, res, outputPathDisplay]() {
            if (!res.success) {
              wxString msg = "Failed to generate PDF plan: " +
                             wxString::FromUTF8(res.message);
              wxMessageBox(msg, "Print Plan", wxOK | wxICON_ERROR, this);
            } else {
              wxMessageBox(wxString::Format("Plan saved to %s",
                                            outputPathDisplay),
                           "Print Plan", wxOK | wxICON_INFORMATION, this);
            }
          });
        }).detach();
      },
      opts.useSimplifiedFootprints, opts.printIncludeGrid);
}

void MainWindow::OnPrintTable(wxCommandEvent &WXUNUSED(event)) {
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

  wxSingleChoiceDialog dlg(this, "Select table", "Print Table", options);
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
    TablePrinter::Print(this, ctrl, type);
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

  wxSingleChoiceDialog dlg(this, "Select table", "Export CSV", options);
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
    TablePrinter::ExportCSV(this, ctrl, type);
}

void MainWindow::OnClose(wxCommandEvent &event) {
  // Allow the close event to be vetoed when the user chooses Cancel
  Close(false);
}

void MainWindow::OnCloseWindow(wxCloseEvent &event) {
  SaveCameraSettings();
  ConfigManager::Get().SaveUserConfig();
  if (!ConfirmSaveIfDirty("exiting", "Exit")) {
    event.Veto();
    return;
  }

  if (viewportPanel)
    viewportPanel->StopRefreshThread();

  Destroy();
}

void MainWindow::OnToggleConsole(wxCommandEvent &event) {
  if (!auiManager)
    return;

  auto &pane = auiManager->GetPane("Console");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  // keep menu state in sync
  GetMenuBar()->Check(ID_View_ToggleConsole, pane.IsShown());
}

void MainWindow::OnToggleFixtures(wxCommandEvent &event) {
  if (!auiManager)
    return;

  auto &pane = auiManager->GetPane("DataNotebook");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleFixtures, pane.IsShown());
}

void MainWindow::OnToggleViewport(wxCommandEvent &event) {
  if (!auiManager)
    return;
  Ensure3DViewport();
  auto &pane = auiManager->GetPane("3DViewport");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleViewport, pane.IsShown());
}

void MainWindow::OnToggleViewport2D(wxCommandEvent &event) {
  if (!auiManager)
    return;
  Ensure2DViewport();
  auto &pane = auiManager->GetPane("2DViewport");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleViewport2D, pane.IsShown());
  if (pane.IsShown() && Viewer2DPanel::Instance())
    Viewer2DPanel::Instance()->Refresh();
}

void MainWindow::OnToggleRender2D(wxCommandEvent &event) {
  if (!auiManager)
    return;
  Ensure2DViewport();
  auto &pane = auiManager->GetPane("2DRenderOptions");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleRender2D, pane.IsShown());
}

void MainWindow::OnToggleLayers(wxCommandEvent &event) {
  if (!auiManager)
    return;

  auto &pane = auiManager->GetPane("LayerPanel");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleLayers, pane.IsShown());
}

void MainWindow::OnToggleSummary(wxCommandEvent &event) {
  if (!auiManager)
    return;

  auto &pane = auiManager->GetPane("SummaryPanel");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  GetMenuBar()->Check(ID_View_ToggleSummary, pane.IsShown());
}

void MainWindow::OnToggleRigging(wxCommandEvent &event) {
  if (!auiManager)
    return;

  auto &pane = auiManager->GetPane("RiggingPanel");
  pane.Show(!pane.IsShown());
  auiManager->Update();

  if (pane.IsShown())
    RefreshRigging();

  GetMenuBar()->Check(ID_View_ToggleRigging, pane.IsShown());
}

void MainWindow::OnPaneClose(wxAuiManagerEvent &event) {
  event.Skip();
  CallAfter(&MainWindow::UpdateViewMenuChecks);
}

void MainWindow::OnApplyDefaultLayout(wxCommandEvent &WXUNUSED(event)) {
  if (!auiManager)
    return;
  Ensure3DViewport();
  ConfigManager &cfg = ConfigManager::Get();
  std::string perspective = defaultLayoutPerspective;
  if (auto val = cfg.GetValue("layout_default"))
    perspective = *val;

  auiManager->LoadPerspective(perspective, true);
  auiManager->Update();

  cfg.SetValue("layout_perspective", perspective);
  UpdateViewMenuChecks();
}

void MainWindow::OnApply2DLayout(wxCommandEvent &WXUNUSED(event)) {
  if (!auiManager)
    return;
  Ensure2DViewport();
  auiManager->LoadPerspective(default2DLayoutPerspective, true);
  auiManager->Update();

  ConfigManager &cfg = ConfigManager::Get();
  cfg.SetValue("layout_perspective", default2DLayoutPerspective);
  UpdateViewMenuChecks();
}

bool MainWindow::LoadProjectFromPath(const std::string &path) {
  if (!ConfigManager::Get().LoadProject(path))
    return false;

  Ensure3DViewport();

  currentProjectPath = path;
  ProjectUtils::SaveLastProjectPath(currentProjectPath);

  ApplySavedLayout();

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
    ConfigManager::Get().SetValue("layout_perspective",
                                  auiManager->SavePerspective().ToStdString());
  }
}

void MainWindow::ApplySavedLayout() {
    if (!auiManager)
        return;

    // Load stored layout perspective if it exists
    if (auto val = ConfigManager::Get().GetValue("layout_perspective")) {
        const std::string& perspective = *val;

        // Ensure viewports exist before loading the saved perspective
        if (perspective.find("3DViewport") != std::string::npos)
            Ensure3DViewport();
        if (perspective.find("2DViewport") != std::string::npos ||
            perspective.find("2DRenderOptions") != std::string::npos)
            Ensure2DViewport();

        auiManager->LoadPerspective(perspective, true);
    }

    // Re-apply hard-coded minimum sizes so they are not overridden by the saved perspective
    auto& dataPane = auiManager->GetPane("DataNotebook");
    if (dataPane.IsOk())
        dataPane.MinSize(wxSize(250, 300));

    auto& view3dPane = auiManager->GetPane("3DViewport");
    if (view3dPane.IsOk())
        view3dPane.MinSize(wxSize(250, 600));

    auto& view2dPane = auiManager->GetPane("2DViewport");
    if (view2dPane.IsOk())
        view2dPane.MinSize(wxSize(250, 600));

    // Ensure always-visible panels
    auto& summaryPane = auiManager->GetPane("SummaryPanel");
    if (summaryPane.IsOk() && !summaryPane.IsShown())
        summaryPane.Show();

    auto& riggingPane = auiManager->GetPane("RiggingPanel");
    if (riggingPane.IsOk() && !riggingPane.IsShown())
        riggingPane.Show();

    auiManager->Update();
    UpdateViewMenuChecks();
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

  auto &summaryPane = auiManager->GetPane("SummaryPanel");
  GetMenuBar()->Check(ID_View_ToggleSummary,
                      summaryPane.IsOk() && summaryPane.IsShown());

  auto &riggingPane = auiManager->GetPane("RiggingPanel");
  GetMenuBar()->Check(ID_View_ToggleRigging,
                      riggingPane.IsOk() && riggingPane.IsShown());
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

void MainWindow::OnProjectLoaded(wxCommandEvent &event) {
  bool loaded = event.GetInt() != 0;
  std::string path = event.GetString().ToStdString();
  Ensure3DViewport();
  if (loaded && !path.empty()) {
    currentProjectPath = path;
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    ApplySavedLayout();
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
  } else {
    ResetProject();
  }
  SplashScreen::SetMessage("Ready");
  SplashScreen::Hide();
}

void MainWindow::OnShowHelp(wxCommandEvent &WXUNUSED(event)) {
  // Attempt to load the Markdown help file located alongside the executable.
  wxFileName helpPath(wxStandardPaths::Get().GetExecutablePath());
  helpPath.SetFullName("help.md");
  if (helpPath.Exists()) {
    // Read the file contents.
    std::ifstream in(helpPath.GetFullPath().ToStdString());
    std::string markdown((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    std::string html = MarkdownToHtml(markdown);

    // Create a resizable dialog containing a wxHtmlWindow to render the
    // generated HTML.
    wxDialog dlg(this, wxID_ANY, "Perastage Help", wxDefaultPosition,
                 wxSize(800, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxHtmlWindow *htmlWin = new wxHtmlWindow(
        &dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO);
    htmlWin->SetPage(html);
    sizer->Add(htmlWin, 1, wxEXPAND | wxALL, 5);
    dlg.SetSizer(sizer);
    dlg.ShowModal();
  } else {
    wxMessageBox("help.md file not found", "Perastage Help",
                 wxOK | wxICON_ERROR, this);
  }
}

void MainWindow::OnShowAbout(wxCommandEvent &WXUNUSED(event)) {
  wxAboutDialogInfo info;
  info.SetName("Perastage");
  info.SetVersion("1.0");
  wxString description =
      "High-performance MVR scene viewer with 3D rendering support.\n\n"
      "This application makes use of the following open-source libraries:\n"
      "  - wxWidgets\n"
      "  - tinyxml2\n"
      "  - nlohmann-json\n"
      "  - OpenGL (or Vulkan backend)";
  info.SetDescription(description);
  info.SetWebSite("https://luismaperamato.com");
  info.AddDeveloper("Luisma Peramato");
  info.SetLicence(
      "This software is licensed under the GNU General Public License v3.0.");

  // Load the largest available icon
  wxIconBundle bundle;
  const wxString iconPaths[] = {"resources/Perastage.ico",
                                "../resources/Perastage.ico",
                                "../../resources/Perastage.ico"};
  for (const wxString &path : iconPaths) {
    if (wxFileExists(path))
      bundle.AddIcon(path, wxBITMAP_TYPE_ICO);
  }
  wxIcon icon = bundle.GetIcon(wxSize(256, 256));
  if (icon.IsOk())
    info.SetIcon(icon);

  wxAboutBox(info, this);
}

void MainWindow::OnSelectFixtures(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->ChangeSelection(0);
}

void MainWindow::OnSelectTrusses(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->ChangeSelection(1);
}

void MainWindow::OnSelectSupports(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->ChangeSelection(2);
}

void MainWindow::OnSelectObjects(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->ChangeSelection(3);
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

  RefreshRigging();
}

void MainWindow::RefreshRigging() {
  if (riggingPanel)
    riggingPanel->RefreshData();
}

void MainWindow::OnPreferences(wxCommandEvent &WXUNUSED(event)) {
  PreferencesDialog dlg(this);
  if (dlg.ShowModal() == wxID_OK) {
    ConfigManager::Get().SaveUserConfig();
  }
}

void MainWindow::OnUndo(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  if (!cfg.CanUndo())
    return;
  std::string action = cfg.Undo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Undo" : "Undo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures());
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses());
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports());
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects());
  }
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    if (fixturePanel && fixturePanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedFixtures());
    else if (trussPanel && trussPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedTrusses());
    else if (hoistPanel && hoistPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSupports());
    else if (sceneObjPanel && sceneObjPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSceneObjects());
    else
      viewportPanel->SetSelectedFixtures({});
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnRedo(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  if (!cfg.CanRedo())
    return;
  std::string action = cfg.Redo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Redo" : "Redo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures());
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses());
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports());
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects());
  }
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    if (fixturePanel && fixturePanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedFixtures());
    else if (trussPanel && trussPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedTrusses());
    else if (hoistPanel && hoistPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSupports());
    else if (sceneObjPanel && sceneObjPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSceneObjects());
    else
      viewportPanel->SetSelectedFixtures({});
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddFixture(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string gdtfPath;
  std::string defaultName;

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
          wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
      wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString,
                        "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxString gdtfPathWx = fdlg.GetPath();
      gdtfPath = std::string(gdtfPathWx.mb_str());
      defaultName = GetGdtfFixtureName(gdtfPath);
      if (defaultName.empty())
        defaultName = wxFileName(gdtfPathWx).GetName().ToStdString();
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(types.size()))
        return;
      defaultName = types[sel];
      std::string spec = typeToSpec[defaultName];
      namespace fs = std::filesystem;
      if (fs::path(spec).is_absolute())
        gdtfPath = spec;
      else
        gdtfPath = (fs::path(scene.basePath) / spec).string();
    }
  } else {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString gdtfPathWx = fdlg.GetPath();
    gdtfPath = std::string(gdtfPathWx.mb_str());
    defaultName = GetGdtfFixtureName(gdtfPath);
    if (defaultName.empty())
      defaultName = wxFileName(gdtfPathWx).GetName().ToStdString();
  }

  std::vector<std::string> modes = GetGdtfModes(gdtfPath);
  AddFixtureDialog dlg(this, wxString::FromUTF8(defaultName), modes);
  if (dlg.ShowModal() != wxID_OK)
    return;

  float weight = 0.0f, power = 0.0f;
  GetGdtfProperties(gdtfPath, weight, power);
  std::string defaultColor = GetGdtfModelColor(gdtfPath);

  int count = dlg.GetUnitCount();
  std::string name = dlg.GetFixtureName();
  int startId = dlg.GetFixtureId();
  std::string mode = dlg.GetMode();

  namespace fs = std::filesystem;
  cfg.PushUndoState("add fixture");
  auto &sceneRef = cfg.GetScene();
  std::string base = sceneRef.basePath;
  std::string spec = gdtfPath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(gdtfPath);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      spec = fs::relative(abs, b).string();
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : sceneRef.layers) {
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
    sceneRef.layers[layer.uuid] = layer;
  }

  int maxId = 0;
  for (const auto &[uuid, fix] : sceneRef.fixtures)
    if (fix.fixtureId > maxId)
      maxId = fix.fixtureId;
  if (startId <= 0)
    startId = maxId + 1;

  for (int i = 0; i < count; ++i) {
    Fixture f;
    f.uuid = wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId), i)
                 .ToStdString();
    f.instanceName = name;
    f.typeName = defaultName;
    f.fixtureId = startId + i;
    f.gdtfSpec = spec;
    f.gdtfMode = mode;
    f.layer = layerName;
    f.weightKg = weight;
    f.powerConsumptionW = power;
    f.color = defaultColor;
    sceneRef.fixtures[f.uuid] = f;
  }

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddTruss(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;

  if (!scene.trusses.empty()) {
    std::map<std::string, std::string> nameToFile;
    for (const auto &[uuid, t] : scene.trusses)
      if (!t.name.empty() && !t.symbolFile.empty())
        nameToFile.try_emplace(t.name, t.symbolFile);
    std::vector<std::string> names;
    names.reserve(nameToFile.size());
    for (const auto &[n, _] : nameToFile)
      names.push_back(n);

    SelectNameDialog chooseDlg(this, names, "Select Truss", "Choose a truss:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString trussDir =
          wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
      wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                        "*.gtruss", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = fn.GetName().ToStdString();
      path = std::string(fdlg.GetPath().mb_str());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = names[sel];
      path = nameToFile[defaultName];
    }
  } else {
    wxString trussDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
    wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                      "*.gtruss", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = fn.GetName().ToStdString();
    path = std::string(fdlg.GetPath().mb_str());
  }

  Truss baseTruss;
  namespace fs = std::filesystem;
  if (fs::path(path).extension() == ".gtruss") {
    if (!LoadTrussArchive(path, baseTruss)) {
      wxMessageBox("Failed to read truss file.", "Error", wxOK | wxICON_ERROR);
      return;
    }
    if (!baseTruss.name.empty())
      defaultName = baseTruss.name;
  } else {
    baseTruss.symbolFile = path;
    baseTruss.modelFile = path;
  }

  long qty = wxGetNumberFromUser("Enter truss quantity:", wxEmptyString,
                                 "Add Truss", 1, 1, 1000, this);
  if (qty <= 0)
    return;

  cfg.PushUndoState("add truss");
  std::string base = scene.basePath;
  std::string modelPath = baseTruss.symbolFile;
  if (!base.empty()) {
    fs::path abs = fs::absolute(modelPath);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      modelPath = fs::relative(abs, b).string();
  }
  baseTruss.symbolFile = modelPath;
  if (!baseTruss.modelFile.empty()) {
    std::string archiveRel = baseTruss.modelFile;
    if (!base.empty()) {
      fs::path absM = fs::absolute(archiveRel);
      fs::path b = fs::absolute(base);
      if (absM.string().rfind(b.string(), 0) == 0)
        archiveRel = fs::relative(absM, b).string();
    }
    baseTruss.modelFile = archiveRel;
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

  for (long i = 0; i < qty; ++i) {
    Truss t = baseTruss;
    t.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                 .ToStdString();
    if (qty > 1)
      t.name = defaultName + " " + std::to_string(i + 1);
    else
      t.name = defaultName;
    t.layer = layerName;
    scene.trusses[t.uuid] = t;
  }

  if (trussPanel)
    trussPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddSceneObject(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;

  if (!scene.sceneObjects.empty()) {
    std::map<std::string, std::string> nameToFile;
    for (const auto &[uuid, o] : scene.sceneObjects)
      if (!o.name.empty() && !o.modelFile.empty())
        nameToFile.try_emplace(o.name, o.modelFile);
    std::vector<std::string> names;
    names.reserve(nameToFile.size());
    for (const auto &[n, _] : nameToFile)
      names.push_back(n);

    SelectNameDialog chooseDlg(this, names, "Select Scene Object",
                               "Choose an object:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString objDir = wxString::FromUTF8(
          ProjectUtils::GetDefaultLibraryPath("scene objects"));
      wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString,
                        "*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = fn.GetName().ToStdString();
      path = std::string(fdlg.GetPath().mb_str());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = names[sel];
      path = nameToFile[defaultName];
    }
  } else {
    wxString objDir = wxString::FromUTF8(
        ProjectUtils::GetDefaultLibraryPath("scene objects"));
    wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString, "*.*",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = fn.GetName().ToStdString();
    path = std::string(fdlg.GetPath().mb_str());
  }

  long qty = wxGetNumberFromUser("Enter object quantity:", wxEmptyString,
                                 "Add Scene Object", 1, 1, 1000, this);
  if (qty <= 0)
    return;

  namespace fs = std::filesystem;
  cfg.PushUndoState("add scene object");
  std::string base = scene.basePath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(path);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      path = fs::relative(abs, b).string();
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

  for (long i = 0; i < qty; ++i) {
    SceneObject obj;
    obj.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                   .ToStdString();
    if (qty > 1)
      obj.name = defaultName + " " + std::to_string(i + 1);
    else
      obj.name = defaultName;
    obj.modelFile = path;
    obj.layer = layerName;
    scene.sceneObjects[obj.uuid] = obj;
  }

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnDelete(wxCommandEvent &WXUNUSED(event)) {
  if (fixturePanel && fixturePanel->IsActivePage())
    fixturePanel->DeleteSelected();
  else if (trussPanel && trussPanel->IsActivePage())
    trussPanel->DeleteSelected();
  else if (hoistPanel && hoistPanel->IsActivePage())
    hoistPanel->DeleteSelected();
  else if (sceneObjPanel && sceneObjPanel->IsActivePage())
    sceneObjPanel->DeleteSelected();
}

void MainWindow::EnableShortcuts(bool enable) {
  if (enable)
    SetAcceleratorTable(m_accel);
  else
    SetAcceleratorTable(wxAcceleratorTable());
}
