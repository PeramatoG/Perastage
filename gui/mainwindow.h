#pragma once

#include <wx/wx.h>
#include <wx/aui/aui.h>

// Forward declarations for GUI components
class wxNotebook;
class FixtureTablePanel;
class TrussTablePanel;
class SceneObjectTablePanel;
class Viewer3DPanel;
class ConsolePanel;

// Main application window for GUI components
class MainWindow : public wxFrame
{
public:
    explicit MainWindow(const wxString& title);
    ~MainWindow();

private:
    void SetupLayout();         // Set up main window layout
    void CreateMenuBar();       // Create the File menu

    wxNotebook* notebook = nullptr;
    FixtureTablePanel* fixturePanel = nullptr;
    TrussTablePanel* trussPanel = nullptr;
    SceneObjectTablePanel* sceneObjPanel = nullptr;
    wxAuiManager* auiManager = nullptr;
    Viewer3DPanel* viewportPanel = nullptr;
    ConsolePanel* consolePanel = nullptr;

    void OnImportMVR(wxCommandEvent& event); // Handle the Import MVR action
    void OnClose(wxCommandEvent& event);     // Handle the Close action

    wxDECLARE_EVENT_TABLE();
};

// Menu item identifiers
enum
{
    ID_File_Load = wxID_HIGHEST + 1,
    ID_File_Save,
    ID_File_SaveAs,
    ID_File_ImportMVR,
    ID_File_ExportMVR,
    ID_File_Close
};
