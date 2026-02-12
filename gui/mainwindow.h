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

#include "mainwindow/ids/edit_ids.h"
#include "viewer2dstate.h"
#include <wx/aui/aui.h>
#include <wx/frame.h>

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
class Viewer2DOffscreenRenderer;
class ConsolePanel;
class LayerPanel;
class LayoutPanel;
class LayoutViewerPanel;
class SummaryPanel;
class RiggingPanel;
struct LayoutViewPreset;
class MainWindowIoController;
class MainWindowLayoutController;
class MainWindowPrintController;
class MainWindowViewController;


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
  Viewer2DOffscreenRenderer *GetOffscreenRenderer();
  bool IsLayout2DViewEditing() const;
  void PersistLayout2DViewState();
  void RestoreLayout2DViewState(int viewId);

private:
  void SetupLayout();   // Set up main window layout
  void CreateMenuBar(); // Create menus
  void CreateToolBars(); // Create toolbars
  void UpdateToolBarAvailability();
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
  std::unique_ptr<Viewer2DOffscreenRenderer> offscreenViewer2DRenderer;
  ConsolePanel *consolePanel = nullptr;
  LayerPanel *layerPanel = nullptr;
  LayoutPanel *layoutPanel = nullptr;
  LayoutViewerPanel *layoutViewerPanel = nullptr;
  SummaryPanel *summaryPanel = nullptr;
  RiggingPanel *riggingPanel = nullptr;
  wxAuiToolBar *fileToolBar = nullptr;
  wxAuiToolBar *editToolBar = nullptr;
  wxAuiToolBar *layoutToolBar = nullptr;
  wxAuiToolBar *layoutViewsToolBar = nullptr;
  wxAuiToolBar *toolsToolBar = nullptr;

  wxAcceleratorTable m_accel;
  std::unique_ptr<MainWindowIoController> ioController;
  std::unique_ptr<MainWindowLayoutController> layoutController;
  std::unique_ptr<MainWindowPrintController> printController;
  std::unique_ptr<MainWindowViewController> viewController;

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
  void OnPrintLayout(wxCommandEvent &event);   // Print layout to PDF
  void OnPrintTable(wxCommandEvent &event);        // Print selected table
  void OnPrintMenu(wxCommandEvent &event);         // Show print options popup
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
  void RefreshAfterSceneChange(bool refreshViewport = true);
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
  void OnLayoutAddLegend(wxCommandEvent &event); // Layout legend creation
  void OnLayoutAddEventTable(wxCommandEvent &event); // Layout event table
  void OnLayoutAddText(wxCommandEvent &event); // Layout text creation
  void OnLayoutAddImage(wxCommandEvent &event); // Layout image creation
  void OnLayout2DViewOk(wxCommandEvent &event); // Confirm layout 2D view edit
  void OnLayout2DViewCancel(wxCommandEvent &event); // Cancel layout 2D edit
  void OnLayoutViewEdit(wxCommandEvent &event);
  void OnLayoutRenderReady(wxCommandEvent &event);

  void OnPaneClose(wxAuiManagerEvent &event); // Keep View menu in sync

  void SaveCameraSettings();
  void SaveUserConfigWithViewport2DState();
  void ApplySavedLayout();
  void ApplyLayoutPreset(const LayoutViewPreset &preset,
                         const std::optional<std::string> &perspective,
                         bool layoutMode, bool persistPerspective);
  void ApplyLayoutModePerspective();
  void BeginLayout2DViewEdit();
  void UpdateViewMenuChecks();
  void OnLayoutSelected(wxCommandEvent &event);
  void ActivateLayoutView(const std::string &layoutName);
  void ShowLayoutLoadingIndicator(const wxString &message);
  void ClearLayoutLoadingIndicator();
  bool HasActiveLayout2DView() const;
  void SyncSceneData();
  bool ConfirmSaveIfDirty(const wxString &actionLabel,
                          const wxString &dialogTitle);
  std::string defaultLayoutPerspective;
  std::string default2DLayoutPerspective;
  std::string defaultLayoutModePerspective;
  bool layoutModeActive = false;
  std::string activeLayoutName;
  bool layout2DViewEditing = false;
  int layout2DViewEditingId = 0;
  std::unique_ptr<viewer2d::ScopedViewer2DState> layout2DViewStateGuard;
  Viewer2DPanel *layout2DViewEditPanel = nullptr;
  Viewer2DRenderPanel *layout2DViewEditRenderPanel = nullptr;
  std::optional<viewer2d::Viewer2DState> standalone2DState;
  std::unique_ptr<wxBusyCursor> layoutRenderCursor;

  friend class MainWindowIoController;
  friend class MainWindowLayoutController;
  friend class MainWindowPrintController;
  friend class MainWindowViewController;

  inline static MainWindow *s_instance = nullptr;
  wxDECLARE_EVENT_TABLE();
};
