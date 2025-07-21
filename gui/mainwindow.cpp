#include "mainwindow.h"
#include "mvrimporter.h"
#include "fixturetablepanel.h"
#include "trusstablepanel.h"
#include "sceneobjecttablepanel.h"
#include "viewer3dpanel.h"
#include <wx/notebook.h>


wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
EVT_MENU(ID_File_ImportMVR, MainWindow::OnImportMVR)
EVT_MENU(ID_File_Close, MainWindow::OnClose)
wxEND_EVENT_TABLE()

MainWindow::MainWindow(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1600, 600))
{
    Centre();
    SetupLayout();
}

MainWindow::~MainWindow()
{
    if (auiManager) {
        auiManager->UnInit();
        delete auiManager;
    }
}

void MainWindow::SetupLayout()
{
    CreateMenuBar();

    // Initialize AUI manager for dynamic pane layout
    auiManager = new wxAuiManager(this);

    // Create notebook with data panels
    notebook = new wxNotebook(this, wxID_ANY);

    fixturePanel = new FixtureTablePanel(notebook);
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
        .PaneBorder(false));

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

    SetMenuBar(menuBar);
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
    }
    else {
        wxMessageBox("MVR file imported successfully.", "Success", wxICON_INFORMATION);
        if (fixturePanel)
            fixturePanel->ReloadData();
        if (trussPanel)
            trussPanel->ReloadData();
        if (sceneObjPanel)
            sceneObjPanel->ReloadData();
        if (viewportPanel)
            viewportPanel->UpdateScene();
    }
}



void MainWindow::OnClose(wxCommandEvent& event)
{
    Close(true);
}
