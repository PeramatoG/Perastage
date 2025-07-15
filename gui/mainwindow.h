#pragma once

#include <wx/wx.h>

// Forward declarations for GUI components
class wxNotebook;
class FixtureTablePanel;
class TrussTablePanel;

// Main application window for GUI components
class MainWindow : public wxFrame
{
public:
    explicit MainWindow(const wxString& title);

private:
    void SetupLayout();         // Set up main window layout
    void CreateMenuBar();       // Create the File menu

    wxNotebook* notebook = nullptr;
    FixtureTablePanel* fixturePanel = nullptr;
    TrussTablePanel* trussPanel = nullptr;

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
