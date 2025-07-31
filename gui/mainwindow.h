#pragma once

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include "projectutils.h"

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

    bool LoadProjectFromPath(const std::string& path); // Load given project
    void ResetProject();                               // Clear current project

private:
    void SetupLayout();         // Set up main window layout
    void CreateMenuBar();       // Create menus
    void UpdateTitle();         // Refresh window title

    std::string currentProjectPath;
    wxNotebook* notebook = nullptr;
    FixtureTablePanel* fixturePanel = nullptr;
    TrussTablePanel* trussPanel = nullptr;
    SceneObjectTablePanel* sceneObjPanel = nullptr;
    wxAuiManager* auiManager = nullptr;
    Viewer3DPanel* viewportPanel = nullptr;
    ConsolePanel* consolePanel = nullptr;

    void OnNew(wxCommandEvent& event);             // Start new project
    void OnLoad(wxCommandEvent& event);            // Load project
    void OnSave(wxCommandEvent& event);            // Save project
    void OnSaveAs(wxCommandEvent& event);         // Save project as
    void OnImportMVR(wxCommandEvent& event);       // Handle the Import MVR action
    void OnExportMVR(wxCommandEvent& event);       // Handle the Export MVR action
    void OnDownloadGdtf(wxCommandEvent& event);    // Download fixture from GDTF Share
    void OnClose(wxCommandEvent& event);           // Handle the Close action
    void OnCloseWindow(wxCloseEvent& event);       // Handle window close
    void OnToggleConsole(wxCommandEvent& event);   // Toggle console panel
    void OnToggleFixtures(wxCommandEvent& event);  // Toggle fixture panel
    void OnToggleViewport(wxCommandEvent& event);  // Toggle 3D viewport
    void OnShowHelp(wxCommandEvent& event);        // Show help dialog
    void OnShowAbout(wxCommandEvent& event);       // Show about dialog
    void OnSelectFixtures(wxCommandEvent& event);  // Switch to fixtures tab
    void OnSelectTrusses(wxCommandEvent& event);   // Switch to trusses tab
    void OnSelectObjects(wxCommandEvent& event);   // Switch to objects tab

    void SaveCameraSettings();

    wxDECLARE_EVENT_TABLE();
};

// Menu item identifiers
enum
{
    ID_File_New = wxID_HIGHEST + 1,
    ID_File_Load,
    ID_File_Save,
    ID_File_SaveAs,
    ID_File_ImportMVR,
    ID_File_ExportMVR,
    ID_File_Close,
    ID_View_ToggleConsole,
    ID_View_ToggleFixtures,
    ID_View_ToggleViewport,
    ID_Tools_DownloadGdtf,
    ID_Help_Help,
    ID_Help_About,
    ID_Select_Fixtures,
    ID_Select_Trusses,
    ID_Select_Objects
};
