#include "mainwindow_menu_builders.h"

#include "ui_feature_flags.h"
#include "mainwindow.h"

namespace {

// Builds the File menu with project and import/export actions.
wxMenu *BuildFileMenu() {
  wxMenu *fileMenu = new wxMenu();
  fileMenu->Append(ID_File_New, "New\tCtrl+N");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Load, "Load\tCtrl+L");
  fileMenu->Append(ID_File_Save, "Save\tCtrl+S");
  fileMenu->Append(ID_File_SaveAs, "Save As...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_ImportMVR, "Import MVR...");
  fileMenu->Append(ID_File_ExportMVR, "Export MVR...");
  fileMenu->Append(ID_File_PrintViewer2D, "Print Viewer 2D...");
  fileMenu->Append(ID_File_PrintLayout, "Print Layout...");
  fileMenu->Append(ID_File_PrintTable, "Print Table...");
  fileMenu->Append(ID_File_ExportCSV, "Export CSV...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Close, "Close\tCtrl+Q");
  return fileMenu;
}

// Builds the primitive submenu used by edit actions.
wxMenu *BuildPrimitiveMenu() {
  wxMenu *primitiveMenu = new wxMenu();
  primitiveMenu->Append(ID_Edit_AddPrimitiveSphere, "Sphere...");
  primitiveMenu->Append(ID_Edit_AddPrimitiveCube, "Cube...");
  primitiveMenu->Append(ID_Edit_AddPrimitiveCylinder, "Cylinder...");
  return primitiveMenu;
}

// Builds the Edit menu including add and preference actions.
wxMenu *BuildEditMenu() {
  wxMenu *editMenu = new wxMenu();
  editMenu->Append(ID_Edit_Undo, "Undo\tCtrl+Z");
  editMenu->Append(ID_Edit_Redo, "Redo\tCtrl+Y");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_AddFixture, "Add fixture...");
  editMenu->Append(ID_Edit_AddTruss, "Add truss...");
  editMenu->Append(ID_Edit_AddSceneObject, "Add scene object...");
  editMenu->AppendSubMenu(BuildPrimitiveMenu(), "Add basic geometry");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Delete, "Delete\tDel");
  editMenu->Append(ID_Edit_ReplaceFixtures, "Replace fixtures...");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Preferences, "Preferences...");
  return editMenu;
}

// Builds the layout view submenu for switching layout visualizations.
wxMenu *BuildLayoutViewsMenu() {
  wxMenu *layoutMenu = new wxMenu();
  layoutMenu->Append(ID_View_Layout_Default, "3D Layout View");
  layoutMenu->Append(ID_View_Layout_2D, "2D Layout View");
  layoutMenu->Append(ID_View_Layout_Mode, "Layout Mode View");
  return layoutMenu;
}

// Builds the View menu with panel visibility and layout view entries.
wxMenu *BuildViewMenu() {
  wxMenu *viewMenu = new wxMenu();
  viewMenu->AppendCheckItem(ID_View_ToggleConsole, "Console");
  viewMenu->AppendCheckItem(ID_View_ToggleFixtures, "Fixtures");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport, "3D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport2D, "2D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleRender2D, "2D Render Options");
  viewMenu->AppendCheckItem(ID_View_ToggleLayers, "Layers");
  viewMenu->AppendCheckItem(ID_View_ToggleLayouts, "Layouts");
  viewMenu->AppendCheckItem(ID_View_ToggleSummary, "Summary");
  viewMenu->AppendCheckItem(ID_View_ToggleRigging, "Rigging");
  viewMenu->AppendSubMenu(BuildLayoutViewsMenu(), "Layout Views");
  return viewMenu;
}

// Builds the Tools menu and adds feature-flagged utility entries.
wxMenu *BuildToolsMenu() {
  wxMenu *toolsMenu = new wxMenu();
  toolsMenu->Append(ID_Tools_DownloadGdtf, "Download GDTF fixture...");
  toolsMenu->Append(ID_Tools_EditDictionaries, "Edit dictionaries...");
  toolsMenu->Append(ID_Tools_OpenUserLibraryFolder, "Open user library folder");
  toolsMenu->Append(ID_Tools_ImportRiderText, "Create from text...");
  toolsMenu->Append(ID_Tools_DistributeHoistWeights, "Distribute hoist weights...");
  toolsMenu->Append(ID_Tools_ExportFixture, "Export Fixture...");
  toolsMenu->Append(ID_Tools_ExportTruss, "Export Truss...");
  toolsMenu->Append(ID_Tools_ExportSceneObject, "Export Scene Object...");
  toolsMenu->Append(ID_Tools_AutoPatch, "Auto patch");
  toolsMenu->Append(ID_Tools_AutoColor, "Auto color");
  toolsMenu->Append(ID_Tools_ConvertToHoist, "Convert to Hoist");
  if (ui::IsFeatureEnabled(ui::FeatureFlag::GenerateFixtureSymbols)) {
    toolsMenu->Append(ID_Tools_GenerateFixtureSymbols, "Generate Fixture Symbols...");
  }
  if (ui::IsFeatureEnabled(ui::FeatureFlag::AssignSelectedFixtureCategory)) {
    toolsMenu->Append(ID_Tools_AssignSelectedFixtureCategory,
                      "Auto-assign categories to selected fixtures...");
  }
  return toolsMenu;
}

// Builds the Help menu with user guidance and about entries.
wxMenu *BuildHelpMenu() {
  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(ID_Help_Help, "Help\tF1");
  helpMenu->Append(ID_Help_OnlineDocumentation, "Online Documentation");
  helpMenu->Append(ID_Help_About, "About Perastage");
  return helpMenu;
}

} // namespace

// Builds the complete main window menu bar structure with all top-level menus.
wxMenuBar *BuildMainWindowMenuBar() {
  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(BuildFileMenu(), "&File");
  menuBar->Append(BuildEditMenu(), "&Edit");
  menuBar->Append(BuildViewMenu(), "&View");
  menuBar->Append(BuildToolsMenu(), "&Tools");
  menuBar->Append(BuildHelpMenu(), "&Help");
  return menuBar;
}
