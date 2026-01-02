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
#pragma once

#include "projectutils.h"
#include "viewer2dstate.h"
#include <wx/aui/aui.h>
#include <wx/wx.h>

#include <memory>
#include <optional>

wxDECLARE_EVENT(EVT_PROJECT_LOADED, wxCommandEvent);

// Forward declarations for GUI components
class wxNotebook;
class FixtureTablePanel;
class TrussTablePanel;
class HoistTablePanel;
class SceneObjectTablePanel;
class Viewer3DPanel;
class Viewer2DPanel;
class Viewer2DRenderPanel;
class ConsolePanel;
class LayerPanel;
class LayoutPanel;
class LayoutViewerPanel;
class SummaryPanel;
class RiggingPanel;

// Main application window for GUI components
class MainWindow : public wxFrame {
public:
  explicit MainWindow(const wxString &title);
  ~MainWindow();

  bool LoadProjectFromPath(const std::string &path); // Load given project
  void ResetProject();                               // Clear current project

  static MainWindow *Instance();
  static void SetInstance(MainWindow *inst);

  void EnableShortcuts(bool enable);
  void Ensure2DViewportAvailable();
  Viewer2DPanel *GetLayoutCapturePanel() const;
  bool IsLayout2DViewEditing() const;
  void PersistLayout2DViewState();
  void RestoreLayout2DViewState(int viewIndex);

private:
  void SetupLayout();   // Set up main window layout
  void CreateMenuBar(); // Create menus
  void CreateToolBars(); // Create toolbars
  void UpdateTitle();   // Refresh window title
  void Ensure3DViewport();
  void Ensure2DViewport();
  void OnProjectLoaded(wxCommandEvent &event);

  std::string currentProjectPath;
  wxNotebook *notebook = nullptr;
  FixtureTablePanel *fixturePanel = nullptr;
  TrussTablePanel *trussPanel = nullptr;
  HoistTablePanel *hoistPanel = nullptr;
  SceneObjectTablePanel *sceneObjPanel = nullptr;
  wxAuiManager *auiManager = nullptr;
  Viewer3DPanel *viewportPanel = nullptr;
  Viewer2DPanel *viewport2DPanel = nullptr;
  Viewer2DRenderPanel *viewport2DRenderPanel = nullptr;
  ConsolePanel *consolePanel = nullptr;
  LayerPanel *layerPanel = nullptr;
  LayoutPanel *layoutPanel = nullptr;
  LayoutViewerPanel *layoutViewerPanel = nullptr;
  SummaryPanel *summaryPanel = nullptr;
  RiggingPanel *riggingPanel = nullptr;
  wxAuiToolBar *fileToolBar = nullptr;
  wxAuiToolBar *layoutToolBar = nullptr;

  wxAcceleratorTable m_accel;

  void OnNew(wxCommandEvent &event);    // Start new project
  void OnLoad(wxCommandEvent &event);   // Load project
  void OnSave(wxCommandEvent &event);   // Save project
  void OnSaveAs(wxCommandEvent &event); // Save project as
  void
  OnImportRider(wxCommandEvent &event);    // Import fixtures/trusses from rider
  void OnImportRiderText(wxCommandEvent &event); // Import rider from text editor
  void OnImportMVR(wxCommandEvent &event); // Handle the Import MVR action
  void OnExportMVR(wxCommandEvent &event); // Handle the Export MVR action
  void OnExportTruss(wxCommandEvent &event);       // Export truss metadata
  void OnExportFixture(wxCommandEvent &event);     // Export fixture GDTF
  void OnExportSceneObject(wxCommandEvent &event); // Export scene object model
  void OnAutoPatch(wxCommandEvent &event);         // Auto patch fixtures
  void OnAutoColor(wxCommandEvent &event);         // Auto assign colors
  void OnConvertToHoist(wxCommandEvent &event);    // Convert fixtures to hoists
  void OnPrintViewer2D(wxCommandEvent &event); // Print 2D view to PDF
  void OnPrintTable(wxCommandEvent &event);        // Print selected table
  void OnExportCSV(wxCommandEvent &event);         // Export table to CSV
  void
  OnDownloadGdtf(wxCommandEvent &event);   // Download fixture from GDTF Share
  void OnEditDictionaries(wxCommandEvent &event); // Edit fixture/truss dictionaries
  void OnClose(wxCommandEvent &event);     // Handle the Close action
  void OnCloseWindow(wxCloseEvent &event); // Handle window close
  void OnToggleConsole(wxCommandEvent &event);        // Toggle console panel
  void OnToggleFixtures(wxCommandEvent &event);       // Toggle fixture panel
  void OnToggleViewport(wxCommandEvent &event);       // Toggle 3D viewport
  void OnToggleViewport2D(wxCommandEvent &event);     // Toggle 2D viewport
  void OnToggleRender2D(wxCommandEvent &event);       // Toggle 2D render panel
  void OnToggleLayers(wxCommandEvent &event);         // Toggle layer panel
  void OnToggleLayouts(wxCommandEvent &event);        // Toggle layout panel
  void OnToggleSummary(wxCommandEvent &event);        // Toggle summary panel
  void OnToggleRigging(wxCommandEvent &event);        // Toggle rigging panel
  void OnShowHelp(wxCommandEvent &event);             // Show help dialog
  void OnShowAbout(wxCommandEvent &event);            // Show about dialog
  void OnSelectFixtures(wxCommandEvent &event);       // Switch to fixtures tab
  void OnSelectTrusses(wxCommandEvent &event);        // Switch to trusses tab
  void OnSelectSupports(wxCommandEvent &event);       // Switch to supports tab
  void OnSelectObjects(wxCommandEvent &event);        // Switch to objects tab
  void OnNotebookPageChanged(wxBookCtrlEvent &event); // Update summary panel
  void RefreshSummary();                              // Refresh summary counts
  void RefreshRigging();                              // Refresh rigging summary

  void OnPreferences(wxCommandEvent &event);        // Show preferences dialog
  void OnApplyDefaultLayout(wxCommandEvent &event); // Reset to default layout
  void OnApply2DLayout(wxCommandEvent &event);      // Apply 2D layout
  void OnApplyLayoutModeLayout(wxCommandEvent &event); // Apply layout mode

  void OnUndo(wxCommandEvent &event);           // Undo action placeholder
  void OnRedo(wxCommandEvent &event);           // Redo action placeholder
  void OnAddFixture(wxCommandEvent &event);     // Add fixture from GDTF
  void OnAddTruss(wxCommandEvent &event);       // Add truss from library
  void OnAddSceneObject(wxCommandEvent &event); // Add generic scene object
  void OnDelete(wxCommandEvent &event);         // Delete selected items
  void OnLayoutAdd2DView(wxCommandEvent &event); // Layout 2D view creation
  void OnLayout2DViewOk(wxCommandEvent &event); // Confirm layout 2D view edit
  void OnLayout2DViewCancel(wxCommandEvent &event); // Cancel layout 2D edit
  void OnLayoutViewEdit(wxCommandEvent &event);

  void OnPaneClose(wxAuiManagerEvent &event); // Keep View menu in sync

  void SaveCameraSettings();
  void ApplySavedLayout();
  void ApplyLayoutModePerspective();
  void BeginLayout2DViewEdit();
  void UpdateViewMenuChecks();
  void OnLayoutSelected(wxCommandEvent &event);
  void ActivateLayoutView(const std::string &layoutName);
  void SyncSceneData();
  bool ConfirmSaveIfDirty(const wxString &actionLabel,
                          const wxString &dialogTitle);
  std::string defaultLayoutPerspective;
  std::string default2DLayoutPerspective;
  std::string layoutModePerspective;
  bool layoutModeActive = false;
  std::string activeLayoutName;
  bool layout2DViewEditing = false;
  std::unique_ptr<viewer2d::ScopedViewer2DState> layout2DViewStateGuard;
  Viewer2DPanel *layout2DViewEditPanel = nullptr;
  Viewer2DRenderPanel *layout2DViewEditRenderPanel = nullptr;

  inline static MainWindow *s_instance = nullptr;
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
  ID_File_PrintViewer2D,
  ID_File_PrintTable,
  ID_File_ExportCSV,
  ID_File_Close,
  ID_View_ToggleConsole,
  ID_View_ToggleFixtures,
  ID_View_ToggleViewport,
  ID_View_ToggleViewport2D,
  ID_View_ToggleRender2D,
  ID_View_ToggleLayers,
  ID_View_ToggleLayouts,
  ID_View_ToggleSummary,
  ID_View_ToggleRigging,
  ID_View_Layout_Default,
  ID_View_Layout_2D,
  ID_View_Layout_Mode,
  ID_View_Layout_2DView,
  ID_Tools_DownloadGdtf,
  ID_Tools_EditDictionaries,
  ID_Tools_ImportRiderText,
  ID_Tools_ExportFixture,
  ID_Tools_ExportTruss,
  ID_Tools_ExportSceneObject,
  ID_Tools_AutoPatch,
  ID_Tools_AutoColor,
  ID_Tools_ConvertToHoist,
  ID_Help_Help,
  ID_Help_About,
  ID_Select_Fixtures,
  ID_Select_Trusses,
  ID_Select_Supports,
  ID_Select_Objects,
  ID_Edit_Undo,
  ID_Edit_Redo,
  ID_Edit_AddFixture,
  ID_Edit_AddTruss,
  ID_Edit_AddSceneObject,
  ID_Edit_Delete,
  ID_Edit_Preferences
};
