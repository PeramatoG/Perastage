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
#include <wx/notebook.h>
#include <wx/filename.h>


wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_MENU(ID_File_Load, MainWindow::OnLoad)
EVT_MENU(ID_File_Save, MainWindow::OnSave)
EVT_MENU(ID_File_SaveAs, MainWindow::OnSaveAs)
EVT_MENU(ID_File_ImportMVR, MainWindow::OnImportMVR)
EVT_MENU(ID_File_ExportMVR, MainWindow::OnExportMVR)
EVT_MENU(ID_File_Close, MainWindow::OnClose)
EVT_MENU(ID_View_ToggleConsole, MainWindow::OnToggleConsole)
EVT_MENU(ID_View_ToggleFixtures, MainWindow::OnToggleFixtures)
EVT_MENU(ID_View_ToggleViewport, MainWindow::OnToggleViewport)
EVT_MENU(ID_Help_Help, MainWindow::OnShowHelp)
EVT_MENU(ID_Help_About, MainWindow::OnShowAbout)
wxEND_EVENT_TABLE()

MainWindow::MainWindow(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 600))
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
    UpdateTitle();
}

MainWindow::~MainWindow()
{
    if (auiManager) {
        auiManager->UnInit();
        delete auiManager;
    }
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
    notebook->AddPage(trussPanel, "Trusses");

    sceneObjPanel = new SceneObjectTablePanel(notebook);
    notebook->AddPage(sceneObjPanel, "Objects");

    // Add notebook as center pane
    auiManager->AddPane(notebook, wxAuiPaneInfo()
        .Name("DataNotebook")
        .Caption("Data Views")
        .CenterPane()
        .PaneBorder(false)
        .CaptionVisible(true)
        .CloseButton(true)
        .MaximizeButton(true));

    // Add 3D viewport as a dockable pane on the right
    viewportPanel = new Viewer3DPanel(this);
    Viewer3DPanel::SetInstance(viewportPanel);
    auiManager->AddPane(viewportPanel, wxAuiPaneInfo()
        .Name("3DViewport")
        .Caption("3D Viewport")
        .Right()
        .BestSize(800, 600)
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
}

void MainWindow::CreateMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar();

    // File menu
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_File_Load, "Load\tCtrl+L");
    fileMenu->Append(ID_File_Save, "Save\tCtrl+S");
    fileMenu->Append(ID_File_SaveAs, "Save As...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_File_ImportMVR, "Import MVR...");
    fileMenu->Append(ID_File_ExportMVR, "Export MVR...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_File_Close, "Close\tCtrl+Q");

    menuBar->Append(fileMenu, "&File");

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

    // Help menu
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_Help_Help, "Help\tF1");
    helpMenu->Append(ID_Help_About, "About");

    menuBar->Append(helpMenu, "&Help");

    SetMenuBar(menuBar);
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


void MainWindow::OnClose(wxCommandEvent& event)
{
    Close(true);
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
        "- Numpad 5: reset orientation";

    wxMessageBox(helpText, "Perastage Help", wxOK | wxICON_INFORMATION, this);
}

void MainWindow::OnShowAbout(wxCommandEvent& WXUNUSED(event))
{
    const wxString aboutText =
        "Perastage v1.0 - MVR scene viewer\n\n"
        "This application uses:\n"
        "- wxWidgets\n"
        "- tinyxml2\n"
        "- nlohmann-json\n"
        "- OpenGL\n\n"
        "Author: Luisma Peramato\n"
        "https://luismaperamato.com\n\n"
        "Licensed under the MIT License.";

    wxMessageBox(aboutText, "About Perastage", wxOK | wxICON_INFORMATION, this);
}
