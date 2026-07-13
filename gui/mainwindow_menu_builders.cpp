#include "mainwindow_menu_builders.h"

#include "mainwindow.h"
#include "ui_feature_flags.h"

#include <wx/intl.h>

namespace {

// Builds the File menu with project and import/export actions.
wxMenu *BuildFileMenu() {
  wxMenu *fileMenu = new wxMenu();
  fileMenu->Append(ID_File_New, _("New\tCtrl+N"));
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Load, _("Load\tCtrl+L"));
  fileMenu->Append(ID_File_Save, _("Save\tCtrl+S"));
  fileMenu->Append(ID_File_SaveAs, _("Save As..."));
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_ImportMVR, _("Import MVR..."));
  fileMenu->Append(ID_File_ExportMVR, _("Export MVR..."));
  fileMenu->Append(ID_File_MvrXchange, _("MVR-xchange..."));
  fileMenu->Append(ID_File_PrintViewer2D, _("Print Viewer 2D..."));
  fileMenu->Append(ID_File_PrintLayout, _("Print Layout..."));
  fileMenu->Append(ID_File_PrintTable, _("Print Table..."));
  fileMenu->Append(ID_File_ExportCSV, _("Export CSV..."));
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Close, _("Close\tCtrl+Q"));
  return fileMenu;
}

// Builds the primitive submenu used by edit actions.
wxMenu *BuildPrimitiveMenu() {
  wxMenu *primitiveMenu = new wxMenu();
  primitiveMenu->Append(ID_Edit_AddPrimitiveSphere, _("Sphere..."));
  primitiveMenu->Append(ID_Edit_AddPrimitiveCube, _("Cube..."));
  primitiveMenu->Append(ID_Edit_AddPrimitiveCylinder, _("Cylinder..."));
  return primitiveMenu;
}

// Builds the Edit menu including add and preference actions.
wxMenu *BuildEditMenu() {
  wxMenu *editMenu = new wxMenu();
  editMenu->Append(ID_Edit_Undo, _("Undo\tCtrl+Z"));
  editMenu->Append(ID_Edit_Redo, _("Redo\tCtrl+Y"));
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_AddFixture, _("Add fixture..."));
  editMenu->Append(ID_Edit_AddTruss, _("Add truss..."));
  editMenu->Append(ID_Edit_AddSceneObject, _("Add scene object..."));
  editMenu->AppendSubMenu(BuildPrimitiveMenu(), _("Add basic geometry"));
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Delete, _("Delete\tDel"));
  editMenu->Append(ID_Edit_Group, _("Group\tCtrl+G"));
  editMenu->Append(ID_Edit_Ungroup, _("Ungroup\tCtrl+U"));
  editMenu->Append(ID_Edit_ReplaceFixtures, _("Replace fixtures..."));
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Preferences, _("Preferences..."));
  return editMenu;
}

// Builds the layout view submenu for switching layout visualizations.
wxMenu *BuildLayoutViewsMenu() {
  wxMenu *layoutMenu = new wxMenu();
  layoutMenu->Append(ID_View_Layout_Default, _("3D Layout View"));
  layoutMenu->Append(ID_View_Layout_2D, _("2D Layout View"));
  layoutMenu->Append(ID_View_Layout_Mode, _("Layout Mode View"));
  return layoutMenu;
}

// Builds the View menu with panel visibility and layout view entries.
wxMenu *BuildViewMenu() {
  wxMenu *viewMenu = new wxMenu();
  viewMenu->AppendCheckItem(ID_View_ToggleConsole, _("Console"));
  viewMenu->AppendCheckItem(ID_View_ToggleFixtures, _("Fixtures"));
  viewMenu->AppendCheckItem(ID_View_ToggleViewport, _("3D Viewport"));
  viewMenu->AppendCheckItem(ID_View_ToggleViewport2D, _("2D Viewport"));
  viewMenu->AppendCheckItem(ID_View_ToggleRender2D, _("2D Render Options"));
  viewMenu->AppendCheckItem(ID_View_ToggleLayers, _("Layers"));
  viewMenu->AppendCheckItem(ID_View_ToggleLayouts, _("Layouts"));
  viewMenu->AppendCheckItem(ID_View_ToggleSummary, _("Summary"));
  viewMenu->AppendCheckItem(ID_View_ToggleRigging, _("Rigging"));
  viewMenu->AppendSubMenu(BuildLayoutViewsMenu(), _("Layout Views"));
  return viewMenu;
}

// Builds the Tools menu and adds feature-flagged utility entries.
wxMenu *BuildToolsMenu() {
  wxMenu *toolsMenu = new wxMenu();
  toolsMenu->Append(ID_Tools_DownloadGdtf, _("Download GDTF fixture..."));
  toolsMenu->Append(ID_Tools_EditDictionaries, _("Edit dictionaries..."));
  toolsMenu->Append(ID_Tools_OpenUserLibraryFolder,
                    _("Open user library folder"));
  toolsMenu->Append(ID_Tools_ImportRiderText, _("Create from text..."));
  toolsMenu->Append(ID_Tools_DistributeHoistWeights,
                    _("Distribute hoist weights..."));
  toolsMenu->Append(ID_Tools_ExportFixture, _("Export Fixture..."));
  toolsMenu->Append(ID_Tools_ExportTruss, _("Export Truss..."));
  toolsMenu->Append(ID_Tools_ExportSceneObject, _("Export Scene Object..."));
  toolsMenu->Append(ID_Tools_AutoPatch, _("Auto patch"));
  toolsMenu->Append(ID_Tools_AutoColor, _("Auto color"));
  toolsMenu->Append(ID_Tools_ConvertToHoist, _("Convert to Hoist"));
  if (ui::IsFeatureEnabled(ui::FeatureFlag::GenerateFixtureSymbols)) {
    toolsMenu->Append(ID_Tools_GenerateFixtureSymbols,
                      _("Generate Fixture Symbols..."));
  }
  if (ui::IsFeatureEnabled(ui::FeatureFlag::AssignSelectedFixtureCategory)) {
    toolsMenu->Append(ID_Tools_AssignSelectedFixtureCategory,
                      _("Auto-assign categories to selected fixtures..."));
  }
  return toolsMenu;
}

// Builds the Help menu with user guidance and about entries.
wxMenu *BuildHelpMenu() {
  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(ID_Help_Help, _("Help\tF1"));
  helpMenu->Append(ID_Help_OnlineDocumentation, _("Online Documentation"));
  helpMenu->Append(ID_Help_CheckForUpdates, _("Check for Updates..."));
  helpMenu->AppendSeparator();
  helpMenu->Append(ID_Help_OpenLogsFolder, _("Open Logs Folder"));
  helpMenu->Append(ID_Help_ExportDiagnosticReport,
                   _("Export Diagnostic Report..."));
  helpMenu->AppendSeparator();
  helpMenu->Append(ID_Help_About, _("About Perastage"));
  return helpMenu;
}

} // namespace

// Builds the complete main window menu bar structure with all top-level menus.
wxMenuBar *BuildMainWindowMenuBar() {
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(BuildFileMenu(), _("&File"));
  menuBar->Append(BuildEditMenu(), _("&Edit"));
  menuBar->Append(BuildViewMenu(), _("&View"));
  menuBar->Append(BuildToolsMenu(), _("&Tools"));
  menuBar->Append(BuildHelpMenu(), _("&Help"));
  return menuBar;
}
