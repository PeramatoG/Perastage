#pragma once

#include "projectutils.h"
#include <wx/aui/aui.h>
#include <wx/wx.h>

// Forward declarations for GUI components
class wxNotebook;
class FixtureTablePanel;
class TrussTablePanel;
class SceneObjectTablePanel;
class Viewer3DPanel;
class ConsolePanel;
class LayerPanel;
class SummaryPanel;

// Main application window for GUI components
class MainWindow : public wxFrame {
public:
  explicit MainWindow(const wxString &title);
  ~MainWindow();

  bool LoadProjectFromPath(const std::string &path); // Load given project
  void ResetProject();                               // Clear current project

private:
  void SetupLayout();   // Set up main window layout
  void CreateMenuBar(); // Create menus
  void UpdateTitle();   // Refresh window title

  std::string currentProjectPath;
  wxNotebook *notebook = nullptr;
  FixtureTablePanel *fixturePanel = nullptr;
  TrussTablePanel *trussPanel = nullptr;
  SceneObjectTablePanel *sceneObjPanel = nullptr;
  wxAuiManager *auiManager = nullptr;
  Viewer3DPanel *viewportPanel = nullptr;
  ConsolePanel *consolePanel = nullptr;
  LayerPanel *layerPanel = nullptr;
  SummaryPanel *summaryPanel = nullptr;

  void OnNew(wxCommandEvent &event);    // Start new project
  void OnLoad(wxCommandEvent &event);   // Load project
  void OnSave(wxCommandEvent &event);   // Save project
  void OnSaveAs(wxCommandEvent &event); // Save project as
  void
  OnImportRider(wxCommandEvent &event);    // Import fixtures/trusses from rider
  void OnImportMVR(wxCommandEvent &event); // Handle the Import MVR action
  void OnExportMVR(wxCommandEvent &event); // Handle the Export MVR action
  void OnExportTruss(wxCommandEvent &event);       // Export truss metadata
  void OnExportFixture(wxCommandEvent &event);     // Export fixture GDTF
  void OnExportSceneObject(wxCommandEvent &event); // Export scene object model
  void OnAutoPatch(wxCommandEvent &event);         // Auto patch fixtures
  void OnPrintTable(wxCommandEvent &event);        // Print selected table
  void OnExportCSV(wxCommandEvent &event);         // Export table to CSV
  void
  OnDownloadGdtf(wxCommandEvent &event);   // Download fixture from GDTF Share
  void OnClose(wxCommandEvent &event);     // Handle the Close action
  void OnCloseWindow(wxCloseEvent &event); // Handle window close
  void OnToggleConsole(wxCommandEvent &event);        // Toggle console panel
  void OnToggleFixtures(wxCommandEvent &event);       // Toggle fixture panel
  void OnToggleViewport(wxCommandEvent &event);       // Toggle 3D viewport
  void OnToggleLayers(wxCommandEvent &event);         // Toggle layer panel
  void OnToggleSummary(wxCommandEvent &event);        // Toggle summary panel
  void OnShowHelp(wxCommandEvent &event);             // Show help dialog
  void OnShowAbout(wxCommandEvent &event);            // Show about dialog
  void OnSelectFixtures(wxCommandEvent &event);       // Switch to fixtures tab
  void OnSelectTrusses(wxCommandEvent &event);        // Switch to trusses tab
  void OnSelectObjects(wxCommandEvent &event);        // Switch to objects tab
  void OnNotebookPageChanged(wxBookCtrlEvent &event); // Update summary panel

  void OnPreferences(wxCommandEvent &event);        // Show preferences dialog
  void OnApplyDefaultLayout(wxCommandEvent &event); // Reset to default layout

  void OnUndo(wxCommandEvent &event);           // Undo action placeholder
  void OnRedo(wxCommandEvent &event);           // Redo action placeholder
  void OnAddFixture(wxCommandEvent &event);     // Add fixture from GDTF
  void OnAddTruss(wxCommandEvent &event);       // Add truss from library
  void OnAddSceneObject(wxCommandEvent &event); // Add generic scene object
  void OnDelete(wxCommandEvent &event);         // Delete selected items

  void SaveCameraSettings();
  void ApplySavedLayout();
  void UpdateViewMenuChecks();
  std::string defaultLayoutPerspective;

  wxDECLARE_EVENT_TABLE();
};

// Menu item identifiers
enum {
  ID_File_New = wxID_HIGHEST + 1,
  ID_File_Load,
  ID_File_Save,
  ID_File_SaveAs,
  ID_File_ImportRider,
  ID_File_ImportMVR,
  ID_File_ExportMVR,
  ID_File_PrintTable,
  ID_File_ExportCSV,
  ID_File_Close,
  ID_View_ToggleConsole,
  ID_View_ToggleFixtures,
  ID_View_ToggleViewport,
  ID_View_ToggleLayers,
  ID_View_ToggleSummary,
  ID_View_Layout_Default,
  ID_Tools_DownloadGdtf,
  ID_Tools_ExportFixture,
  ID_Tools_ExportTruss,
  ID_Tools_ExportSceneObject,
  ID_Tools_AutoPatch,
  ID_Help_Help,
  ID_Help_About,
  ID_Select_Fixtures,
  ID_Select_Trusses,
  ID_Select_Objects,
  ID_Edit_Undo,
  ID_Edit_Redo,
  ID_Edit_AddFixture,
  ID_Edit_AddTruss,
  ID_Edit_AddSceneObject,
  ID_Edit_Delete,
  ID_Edit_Preferences
};
