#include "mainwindow.h"
#include "mvrimporter.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "viewer3dpanel.h"
#include "consolepanel.h"
#include "configmanager.h"
#include "mvrexporter.h"
#include "projectutils.h"
#include "logindialog.h"
#include "gdtfsearchdialog.h"
#include "addfixturedialog.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "simplecrypt.h"
#include "credentialstore.h"
#include "fixture.h"
#include <wx/aboutdlg.h>
#include <wx/notebook.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <chrono>
#ifdef _WIN32
#  define popen _popen
#  define pclose _pclose
#endif


wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_MENU(ID_File_New, MainWindow::OnNew)
EVT_MENU(ID_File_Load, MainWindow::OnLoad)
EVT_MENU(ID_File_Save, MainWindow::OnSave)
EVT_MENU(ID_File_SaveAs, MainWindow::OnSaveAs)
EVT_MENU(ID_File_ImportMVR, MainWindow::OnImportMVR)
EVT_MENU(ID_File_ExportMVR, MainWindow::OnExportMVR)
EVT_MENU(ID_File_Close, MainWindow::OnClose)
EVT_CLOSE(MainWindow::OnCloseWindow)
EVT_MENU(ID_Edit_Undo, MainWindow::OnUndo)
EVT_MENU(ID_Edit_Redo, MainWindow::OnRedo)
EVT_MENU(ID_Edit_AddFixture, MainWindow::OnAddFixture)
EVT_MENU(ID_Edit_Delete, MainWindow::OnDelete)
EVT_MENU(ID_View_ToggleConsole, MainWindow::OnToggleConsole)
EVT_MENU(ID_View_ToggleFixtures, MainWindow::OnToggleFixtures)
EVT_MENU(ID_View_ToggleViewport, MainWindow::OnToggleViewport)
EVT_MENU(ID_Tools_DownloadGdtf, MainWindow::OnDownloadGdtf)
EVT_MENU(ID_Help_Help, MainWindow::OnShowHelp)
EVT_MENU(ID_Help_About, MainWindow::OnShowAbout)
EVT_MENU(ID_Select_Fixtures, MainWindow::OnSelectFixtures)
EVT_MENU(ID_Select_Trusses, MainWindow::OnSelectTrusses)
EVT_MENU(ID_Select_Objects, MainWindow::OnSelectObjects)
wxEND_EVENT_TABLE()

MainWindow::MainWindow(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 950))
{
    wxIcon icon;
    const char* iconPaths[] = {
        "resources/Perastage.ico",
        "../resources/Perastage.ico",
        "../../resources/Perastage.ico"
    };
    for (const char* path : iconPaths)
    {
        if (icon.LoadFile(path, wxBITMAP_TYPE_ICO))
            break;
    }
    if (icon.IsOk())
        SetIcon(icon);

    Centre();
    SetupLayout();

    // Apply camera settings after layout and config are ready
    if (viewportPanel)
        viewportPanel->LoadCameraFromConfig();

    UpdateTitle();
}

MainWindow::~MainWindow()
{
    if (auiManager) {
        auiManager->UnInit();
        delete auiManager;
    }
    SaveCameraSettings();
    ConfigManager::Get().SaveUserConfig();
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
}

void MainWindow::SetupLayout()
{
    CreateMenuBar();

    // Initialize AUI manager for dynamic pane layout
    auiManager = new wxAuiManager(this);

    // Create notebook with data panels
    notebook = new wxNotebook(this, wxID_ANY);

    fixturePanel = new FixtureTablePanel(notebook);
    FixtureTablePanel::SetInstance(fixturePanel);
    notebook->AddPage(fixturePanel, "Fixtures");

    trussPanel = new TrussTablePanel(notebook);
    TrussTablePanel::SetInstance(trussPanel);
    notebook->AddPage(trussPanel, "Trusses");

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
        .MinSize(wxSize(halfWidth, 600))
        .PaneBorder(false)
        .CaptionVisible(true)
        .CloseButton(true)
        .MaximizeButton(true));

    // Add 3D viewport as the main center pane
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
        .MinSize(wxSize(halfWidth, 600))
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

    // Apply all changes to layout
    auiManager->Update();

    // Keyboard shortcuts to switch notebook pages
    wxAcceleratorEntry entries[3];
    entries[0].Set(wxACCEL_NORMAL, (int)'1', ID_Select_Fixtures);
    entries[1].Set(wxACCEL_NORMAL, (int)'2', ID_Select_Trusses);
    entries[2].Set(wxACCEL_NORMAL, (int)'3', ID_Select_Objects);
    wxAcceleratorTable accel(3, entries);
    SetAcceleratorTable(accel);
}

void MainWindow::CreateMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar();

    // File menu
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_File_New, "New\tCtrl+N");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_File_Load, "Load\tCtrl+L");
    fileMenu->Append(ID_File_Save, "Save\tCtrl+S");
    fileMenu->Append(ID_File_SaveAs, "Save As...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_File_ImportMVR, "Import MVR...");
    fileMenu->Append(ID_File_ExportMVR, "Export MVR...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_File_Close, "Close\tCtrl+Q");

    menuBar->Append(fileMenu, "&File");

    // Edit menu
    wxMenu* editMenu = new wxMenu();
    editMenu->Append(ID_Edit_Undo, "Undo\tCtrl+Z");
    editMenu->Append(ID_Edit_Redo, "Redo\tCtrl+Y");
    editMenu->AppendSeparator();
    editMenu->Append(ID_Edit_AddFixture, "Add fixture...");
    editMenu->AppendSeparator();
    editMenu->Append(ID_Edit_Delete, "Delete\tDel");

    menuBar->Append(editMenu, "&Edit");

    // View menu for toggling panels
    wxMenu* viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(ID_View_ToggleConsole, "Console");
    viewMenu->AppendCheckItem(ID_View_ToggleFixtures, "Fixtures");
    viewMenu->AppendCheckItem(ID_View_ToggleViewport, "3D Viewport");

    // Initial check states
    viewMenu->Check(ID_View_ToggleConsole, true);
    viewMenu->Check(ID_View_ToggleFixtures, true);
    viewMenu->Check(ID_View_ToggleViewport, true);

    menuBar->Append(viewMenu, "&View");

    // Tools menu
    wxMenu* toolsMenu = new wxMenu();
    toolsMenu->Append(ID_Tools_DownloadGdtf, "Download GDTF fixture...");

    menuBar->Append(toolsMenu, "&Tools");

    // Help menu
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_Help_Help, "Help\tF1");
    helpMenu->Append(ID_Help_About, "About");

    menuBar->Append(helpMenu, "&Help");

    SetMenuBar(menuBar);
}

void MainWindow::OnNew(wxCommandEvent& WXUNUSED(event))
{
    wxMessageDialog dlg(this,
        "Do you want to save changes before creating a new project?",
        "New Project",
        wxYES_NO | wxCANCEL | wxICON_QUESTION);

    int res = dlg.ShowModal();
    if (res == wxID_YES)
    {
        wxCommandEvent saveEvt;
        OnSave(saveEvt);
    }
    else if (res == wxID_CANCEL)
    {
        return;
    }

    ResetProject();
}

void MainWindow::OnLoad(wxCommandEvent& event)
{
    wxString filter = wxString::Format("Perastage files (*%s)|*%s", ProjectUtils::PROJECT_EXTENSION, ProjectUtils::PROJECT_EXTENSION);
    wxFileDialog dlg(this, "Open Project", "", "", filter, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL)
        return;

    wxString path = dlg.GetPath();
    if (!LoadProjectFromPath(path.ToStdString()))
        wxMessageBox("Failed to load project.", "Error", wxICON_ERROR);
}

void MainWindow::OnSave(wxCommandEvent& event)
{
    if (currentProjectPath.empty()) {
        OnSaveAs(event);
        return;
    }
    SaveCameraSettings();
    ConfigManager::Get().SaveUserConfig();
    if (!ConfigManager::Get().SaveProject(currentProjectPath))
        wxMessageBox("Failed to save project.", "Error", wxICON_ERROR);
    else {
        ProjectUtils::SaveLastProjectPath(currentProjectPath);
        if (consolePanel)
            consolePanel->AppendMessage("Saved " + wxString::FromUTF8(currentProjectPath));
    }
}

void MainWindow::OnSaveAs(wxCommandEvent& event)
{
    wxString filter = wxString::Format("Perastage files (*%s)|*%s", ProjectUtils::PROJECT_EXTENSION, ProjectUtils::PROJECT_EXTENSION);
    wxFileDialog dlg(this, "Save Project", "", "", filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL)
        return;

    currentProjectPath = dlg.GetPath().ToStdString();
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

// Handles MVR file selection, import, and updates fixture/truss panels accordingly
void MainWindow::OnImportMVR(wxCommandEvent& event)
{
    wxFileDialog openFileDialog(
        this,
        "Import MVR file",
        "",
        "",
        "MVR files (*.mvr)|*.mvr",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST
    );

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filePath = openFileDialog.GetPath();
    if (!MvrImporter::ImportAndRegister(filePath.ToStdString())) {
        wxMessageBox("Failed to import MVR file.", "Error", wxICON_ERROR);
        if (consolePanel)
            consolePanel->AppendMessage("Failed to import " + filePath);
    }
    else {
        wxMessageBox("MVR file imported successfully.", "Success", wxICON_INFORMATION);
        if (consolePanel)
            consolePanel->AppendMessage("Imported " + filePath);
        if (fixturePanel)
            fixturePanel->ReloadData();
        if (trussPanel)
            trussPanel->ReloadData();
        if (sceneObjPanel)
            sceneObjPanel->ReloadData();
        if (viewportPanel) {
            viewportPanel->UpdateScene();
            viewportPanel->Refresh();
        }
    }
}


void MainWindow::OnExportMVR(wxCommandEvent& event)
{
    wxFileDialog saveFileDialog(
        this,
        "Export MVR file",
        "",
        "",
        "MVR files (*.mvr)|*.mvr",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return;

    MvrExporter exporter;
    wxString path = saveFileDialog.GetPath();
    if (!exporter.ExportToFile(path.ToStdString())) {
        wxMessageBox("Failed to export MVR file.", "Error", wxICON_ERROR);
        if (consolePanel)
            consolePanel->AppendMessage("Failed to export " + path);
    } else {
        wxMessageBox("MVR file exported successfully.", "Success", wxICON_INFORMATION);
        if (consolePanel)
            consolePanel->AppendMessage("Exported " + path);
    }
}

void MainWindow::OnDownloadGdtf(wxCommandEvent& WXUNUSED(event))
{
    std::string savedUser;
    std::string savedPass;
    if (auto creds = CredentialStore::Load()) {
        savedUser = creds->username;
        savedPass = creds->password;
    } else {
        std::string savedPassEnc = ConfigManager::Get().GetValue("gdtf_password").value_or("");
        savedUser = ConfigManager::Get().GetValue("gdtf_username").value_or("");
        savedPass = SimpleCrypt::Decode(savedPassEnc);
    }

    GdtfLoginDialog loginDlg(this, savedUser, savedPass);
    if (loginDlg.ShowModal() != wxID_OK)
        return;
    wxString username = wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false);
    wxString password = wxString::FromUTF8(loginDlg.GetPassword());
    ConfigManager::Get().SetValue("gdtf_username", std::string(username.mb_str()));
    ConfigManager::Get().SetValue("gdtf_password", SimpleCrypt::Encode(std::string(password.mb_str())));
    CredentialStore::Save({std::string(username.mb_str()), std::string(password.mb_str())});

    if (!currentProjectPath.empty())
        ConfigManager::Get().SaveProject(currentProjectPath);

    wxString cookieFileWx = wxFileName::GetTempDir() + "/gdtf_session.txt";
    std::string cookieFile = std::string(cookieFileWx.mb_str());
    long httpCode = 0;
    if (consolePanel)
        consolePanel->AppendMessage("Logging into GDTF Share using libcurl");
    if (!GdtfLogin(std::string(username.mb_str()), std::string(password.mb_str()), cookieFile, httpCode)) {
        wxMessageBox("Failed to connect to GDTF Share.", "Login Error", wxOK | wxICON_ERROR);
        if (consolePanel)
            consolePanel->AppendMessage("Login connection failed");
        return;
    }
    if (consolePanel)
        consolePanel->AppendMessage(wxString::Format("Login HTTP code: %ld", httpCode));
    if (httpCode != 200) {
        wxMessageBox("Login failed.", "Login Error", wxOK | wxICON_ERROR);
        if (consolePanel)
            consolePanel->AppendMessage("Login failed with code " + wxString::Format("%ld", httpCode));
        return;
    }

    if (consolePanel)
        consolePanel->AppendMessage("Retrieving fixture list via libcurl");
    std::string listData;
    if (!GdtfGetList(cookieFile, listData)) {
        wxMessageBox("Failed to retrieve fixture list.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    if (consolePanel)
        consolePanel->AppendMessage(wxString::Format("Retrieved list size: %zu bytes", listData.size()));

    if (consolePanel)
        consolePanel->AppendMessage(wxString::Format("Retrieved list size: %zu bytes", listData.size()));

    ConfigManager::Get().SetValue("gdtf_fixture_list", listData);

    // open search dialog
    GdtfSearchDialog searchDlg(this, listData);
    if (searchDlg.ShowModal() == wxID_OK) {
        wxString rid = wxString::FromUTF8(searchDlg.GetSelectedId());
        wxString name = wxString::FromUTF8(searchDlg.GetSelectedName());

        wxFileDialog saveDlg(this, "Save GDTF file", "library/fixtures", name + ".gdtf",
                             "*.gdtf", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDlg.ShowModal() == wxID_OK) {
            wxString dest = saveDlg.GetPath();
            if (!rid.empty()) {
                if (consolePanel)
                    consolePanel->AppendMessage("Downloading via libcurl rid=" + rid);
                long dlCode = 0;
                bool ok = GdtfDownload(std::string(rid.mb_str()), std::string(dest.mb_str()), cookieFile, dlCode);
                if (consolePanel)
                    consolePanel->AppendMessage(wxString::Format("Download HTTP code: %ld", dlCode));
                if (ok && dlCode == 200)
                    wxMessageBox("GDTF downloaded.", "Success", wxOK | wxICON_INFORMATION);
                else
                    wxMessageBox("Failed to download GDTF.", "Error", wxOK | wxICON_ERROR);
            } else {
                wxMessageBox("Download information missing.", "Error", wxOK | wxICON_ERROR);
            }
        }
    }

    wxRemoveFile(cookieFileWx);
}


void MainWindow::OnClose(wxCommandEvent& event)
{
    // Allow the close event to be vetoed when the user chooses Cancel
    Close(false);
}

void MainWindow::OnCloseWindow(wxCloseEvent& event)
{
    SaveCameraSettings();
    ConfigManager::Get().SaveUserConfig();
    wxMessageDialog dlg(this,
        "Do you want to save changes before exiting?",
        "Exit",
        wxYES_NO | wxCANCEL | wxICON_QUESTION);

    int res = dlg.ShowModal();
    if (res == wxID_YES)
    {
        wxCommandEvent saveEvt;
        OnSave(saveEvt);
    }
    else if (res == wxID_CANCEL)
    {
        event.Veto();
        return;
    }

    Destroy();
}

void MainWindow::OnToggleConsole(wxCommandEvent& event)
{
    if (!auiManager)
        return;

    auto& pane = auiManager->GetPane("Console");
    pane.Show(!pane.IsShown());
    auiManager->Update();

    // keep menu state in sync
    GetMenuBar()->Check(ID_View_ToggleConsole, pane.IsShown());
}

void MainWindow::OnToggleFixtures(wxCommandEvent& event)
{
    if (!auiManager)
        return;

    auto& pane = auiManager->GetPane("DataNotebook");
    pane.Show(!pane.IsShown());
    auiManager->Update();

    GetMenuBar()->Check(ID_View_ToggleFixtures, pane.IsShown());
}

void MainWindow::OnToggleViewport(wxCommandEvent& event)
{
    if (!auiManager)
        return;

    auto& pane = auiManager->GetPane("3DViewport");
    pane.Show(!pane.IsShown());
    auiManager->Update();

    GetMenuBar()->Check(ID_View_ToggleViewport, pane.IsShown());
}

bool MainWindow::LoadProjectFromPath(const std::string& path)
{
    if (!ConfigManager::Get().LoadProject(path))
        return false;

    currentProjectPath = path;
    ProjectUtils::SaveLastProjectPath(currentProjectPath);

    if (consolePanel)
        consolePanel->AppendMessage("Loaded " + wxString::FromUTF8(path));
    if (fixturePanel)
        fixturePanel->ReloadData();
    if (trussPanel)
        trussPanel->ReloadData();
    if (sceneObjPanel)
        sceneObjPanel->ReloadData();
    if (viewportPanel) {
        ConfigManager& cfg = ConfigManager::Get();
        Viewer3DCamera& cam = viewportPanel->GetCamera();
        cam.SetOrientation(cfg.GetFloat("camera_yaw"), cfg.GetFloat("camera_pitch"));
        cam.SetDistance(cfg.GetFloat("camera_distance"));
        cam.SetTarget(cfg.GetFloat("camera_target_x"),
                      cfg.GetFloat("camera_target_y"),
                      cfg.GetFloat("camera_target_z"));
        viewportPanel->UpdateScene();
        viewportPanel->Refresh();
    }
    UpdateTitle();
    return true;
}

void MainWindow::ResetProject()
{
    ConfigManager::Get().Reset();
    currentProjectPath.clear();
    if (fixturePanel)
        fixturePanel->ReloadData();
    if (trussPanel)
        trussPanel->ReloadData();
    if (sceneObjPanel)
        sceneObjPanel->ReloadData();
    if (viewportPanel) {
        viewportPanel->UpdateScene();
        viewportPanel->Refresh();
    }
    UpdateTitle();
}

void MainWindow::UpdateTitle()
{
    wxString title = "Perastage";
    if (!currentProjectPath.empty()) {
        wxFileName fn(wxString::FromUTF8(currentProjectPath));
        title += " - " + fn.GetName();
    } else {
        title += " - Untitled";
    }
    SetTitle(title);
}

void MainWindow::SaveCameraSettings()
{
    if (!viewportPanel)
        return;
    Viewer3DCamera& cam = viewportPanel->GetCamera();
    ConfigManager::Get().SetFloat("camera_yaw", cam.GetYaw());
    ConfigManager::Get().SetFloat("camera_pitch", cam.GetPitch());
    ConfigManager::Get().SetFloat("camera_distance", cam.GetDistance());
    ConfigManager::Get().SetFloat("camera_target_x", cam.GetTargetX());
    ConfigManager::Get().SetFloat("camera_target_y", cam.GetTargetY());
    ConfigManager::Get().SetFloat("camera_target_z", cam.GetTargetZ());
}

void MainWindow::OnShowHelp(wxCommandEvent& WXUNUSED(event))
{
    const wxString helpText =
        "Use File → Import MVR to load an .mvr file.\n"
        "Tables will list fixtures and trusses while the scene is shown in the 3D viewport.\n"
        "Toggle panels from the View menu.\n\n"
        "Keyboard controls:\n"
        "- Arrow keys: orbit the view\n"
        "- Shift + Arrow keys: pan\n"
        "- Alt + Up/Down (or Alt + Left/Right): zoom\n"
        "- Numpad 1/3/7: front, right and top views\n"
        "- Numpad 5: reset orientation\n"
        "- 1/2/3: show Fixtures, Trusses or Objects tables";

    wxMessageBox(helpText, "Perastage Help", wxOK | wxICON_INFORMATION, this);
}

void MainWindow::OnShowAbout(wxCommandEvent& WXUNUSED(event))
{
    wxAboutDialogInfo info;
    info.SetName("Perastage");
    info.SetVersion("1.0");
    info.SetDescription("MVR scene viewer");
    info.SetWebSite("https://luismaperamato.com", "luismaperamato.com");
    info.AddDeveloper("Luisma Peramato");

    wxString licence =
        "This application uses:\n"
        "- wxWidgets\n"
        "- tinyxml2\n"
        "- nlohmann-json\n"
        "- OpenGL\n\n"
        "Licensed under the MIT License.";
    info.SetLicence(licence);

    wxIcon icon;
    const char* iconPaths[] = {
        "resources/Perastage.ico",
        "../resources/Perastage.ico",
        "../../resources/Perastage.ico"
    };
    for (const char* path : iconPaths)
    {
        if (icon.LoadFile(path, wxBITMAP_TYPE_ICO))
            break;
    }
    if (icon.IsOk())
        info.SetIcon(icon);

    wxAboutBox(info, this);
}

void MainWindow::OnSelectFixtures(wxCommandEvent& WXUNUSED(event))
{
    if (notebook)
        notebook->ChangeSelection(0);
}

void MainWindow::OnSelectTrusses(wxCommandEvent& WXUNUSED(event))
{
    if (notebook)
        notebook->ChangeSelection(1);
}

void MainWindow::OnSelectObjects(wxCommandEvent& WXUNUSED(event))
{
    if (notebook)
        notebook->ChangeSelection(2);
}

void MainWindow::OnUndo(wxCommandEvent& WXUNUSED(event))
{
    ConfigManager& cfg = ConfigManager::Get();
    if (!cfg.CanUndo())
        return;
    cfg.Undo();
    if (fixturePanel)
        fixturePanel->ReloadData();
    if (trussPanel)
        trussPanel->ReloadData();
    if (sceneObjPanel)
        sceneObjPanel->ReloadData();
    if (viewportPanel) {
        viewportPanel->UpdateScene();
        viewportPanel->Refresh();
    }
}

void MainWindow::OnRedo(wxCommandEvent& WXUNUSED(event))
{
    ConfigManager& cfg = ConfigManager::Get();
    if (!cfg.CanRedo())
        return;
    cfg.Redo();
    if (fixturePanel)
        fixturePanel->ReloadData();
    if (trussPanel)
        trussPanel->ReloadData();
    if (sceneObjPanel)
        sceneObjPanel->ReloadData();
    if (viewportPanel) {
        viewportPanel->UpdateScene();
        viewportPanel->Refresh();
    }
}

void MainWindow::OnAddFixture(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog fdlg(this, "Select GDTF file", "library/fixtures", wxEmptyString,
                      "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
        return;

    wxString gdtfPathWx = fdlg.GetPath();
    std::string gdtfPath = std::string(gdtfPathWx.mb_str());

    std::string defaultName = GetGdtfFixtureName(gdtfPath);
    if (defaultName.empty())
        defaultName = wxFileName(gdtfPathWx).GetName().ToStdString();

    std::vector<std::string> modes = GetGdtfModes(gdtfPath);
    AddFixtureDialog dlg(this, wxString::FromUTF8(defaultName), modes);
    if (dlg.ShowModal() != wxID_OK)
        return;

    int count = dlg.GetUnitCount();
    std::string name = dlg.GetFixtureName();
    int startId = dlg.GetFixtureId();
    std::string mode = dlg.GetMode();

    namespace fs = std::filesystem;
    ConfigManager& cfg = ConfigManager::Get();
    cfg.PushUndoState();
    auto& scene = cfg.GetScene();
    std::string base = scene.basePath;
    std::string spec = gdtfPath;
    if (!base.empty()) {
        fs::path abs = fs::absolute(gdtfPath);
        fs::path b = fs::absolute(base);
        if (abs.string().rfind(b.string(), 0) == 0)
            spec = fs::relative(abs, b).string();
    }

    int maxId = 0;
    for (const auto& [uuid, fix] : scene.fixtures)
        if (fix.fixtureId > maxId)
            maxId = fix.fixtureId;
    if (startId <= 0)
        startId = maxId + 1;

    for (int i = 0; i < count; ++i) {
        Fixture f;
        f.uuid = wxString::Format("uuid_%lld_%d",
                                static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()), i).ToStdString();
        f.name = name;
        f.fixtureId = startId + i;
        f.gdtfSpec = spec;
        f.gdtfMode = mode;
        scene.fixtures[f.uuid] = f;
    }

    if (fixturePanel)
        fixturePanel->ReloadData();
    if (viewportPanel) {
        viewportPanel->UpdateScene();
        viewportPanel->Refresh();
    }
}

void MainWindow::OnDelete(wxCommandEvent& WXUNUSED(event))
{
    if (fixturePanel && fixturePanel->IsActivePage())
        fixturePanel->DeleteSelected();
    else if (trussPanel && trussPanel->IsActivePage())
        trussPanel->DeleteSelected();
    else if (sceneObjPanel && sceneObjPanel->IsActivePage())
        sceneObjPanel->DeleteSelected();
}
