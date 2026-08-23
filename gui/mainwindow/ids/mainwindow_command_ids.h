#pragma once

#include <array>
#include <cstddef>

#include <wx/defs.h>

// Defines every distinct command routed by MainWindow in one contiguous sequence.
enum MainWindowCommandId {
  ID_File_New = wxID_HIGHEST + 1,
  ID_File_Load,
  ID_File_Save,
  ID_File_SaveAs,
  ID_File_ImportRider,
  ID_File_ImportMVR,
  ID_File_ExportMVR,
  ID_File_MvrXchange,
  ID_File_PrintViewer2D,
  ID_File_PrintLayout,
  ID_File_PrintTable,
  ID_File_PrintMenu,
  ID_File_ExportCSV,
  ID_File_Close,
  ID_Edit_Undo,
  ID_Edit_Redo,
  ID_Edit_Cut,
  ID_Edit_Copy,
  ID_Edit_Paste,
  ID_Edit_AddFixture,
  ID_Edit_AddTruss,
  ID_Edit_AddSceneObject,
  ID_Edit_AddPrimitiveSphere,
  ID_Edit_AddPrimitiveCube,
  ID_Edit_AddPrimitiveCylinder,
  ID_Edit_Delete,
  ID_Edit_Group,
  ID_Edit_Ungroup,
  ID_Edit_ReplaceFixtures,
  ID_Edit_ReplaceTrusses,
  ID_Edit_Preferences,
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
  ID_View_Layout_Legend,
  ID_View_Layout_EventTable,
  ID_View_Layout_Text,
  ID_View_Layout_Image,
  ID_View_Viewport_Top,
  ID_View_Viewport_Front,
  ID_View_Viewport_Side,
  ID_View_Viewport_SelectTool,
  ID_View_Viewport_MeasureTool,
  ID_View_Viewport_GapMeasureTool,
  ID_View_Viewport_AxisConstraint,
  ID_View_Viewport_LeftDragMove,
  ID_View_Viewport_LocalAxes,
  ID_View_Viewport_Magnet,
  ID_View_Viewport_CrossTableActions,
  ID_Tools_DownloadGdtf,
  ID_Tools_EditDictionaries,
  ID_Tools_ImportRiderText,
  ID_Tools_DistributeHoistWeights,
  ID_Tools_ExportFixture,
  ID_Tools_ExportTruss,
  ID_Tools_ExportSceneObject,
  ID_Tools_AutoPatch,
  ID_Tools_AutoColor,
  ID_Tools_ConvertToHoist,
  ID_Tools_ConvertSceneObjectsToTruss,
  ID_Tools_GenerateFixtureSymbols,
  ID_Tools_AssignSelectedFixtureCategory,
  ID_Tools_OpenUserLibraryFolder,
  ID_Tools_DistributeFixturesOnTruss,
  ID_Tools_DistributeFixturesBetweenPoints,
  ID_Tools_DistributeFixtures,
  ID_Help_Help,
  ID_Help_OnlineDocumentation,
  ID_Help_OpenLogsFolder,
  ID_Help_ExportDiagnosticReport,
  ID_Help_About,
  ID_Help_CheckForUpdates,
  ID_Select_Fixtures,
  ID_Select_Trusses,
  ID_Select_Supports,
  ID_Select_Objects,
  ID_MainWindowCommand_End
};

namespace mainwindow::commands {

inline constexpr std::array kAllDistinctCommandIds = {
    ID_File_New,
    ID_File_Load, ID_File_Save, ID_File_SaveAs, ID_File_ImportRider,
    ID_File_ImportMVR, ID_File_ExportMVR, ID_File_MvrXchange,
    ID_File_PrintViewer2D, ID_File_PrintLayout, ID_File_PrintTable,
    ID_File_PrintMenu, ID_File_ExportCSV, ID_File_Close, ID_Edit_Undo,
    ID_Edit_Redo, ID_Edit_Cut, ID_Edit_Copy, ID_Edit_Paste,
    ID_Edit_AddFixture, ID_Edit_AddTruss, ID_Edit_AddSceneObject,
    ID_Edit_AddPrimitiveSphere, ID_Edit_AddPrimitiveCube,
    ID_Edit_AddPrimitiveCylinder, ID_Edit_Delete, ID_Edit_Group,
    ID_Edit_Ungroup, ID_Edit_ReplaceFixtures, ID_Edit_ReplaceTrusses,
    ID_Edit_Preferences, ID_View_ToggleConsole, ID_View_ToggleFixtures,
    ID_View_ToggleViewport, ID_View_ToggleViewport2D,
    ID_View_ToggleRender2D, ID_View_ToggleLayers, ID_View_ToggleLayouts,
    ID_View_ToggleSummary, ID_View_ToggleRigging, ID_View_Layout_Default,
    ID_View_Layout_2D, ID_View_Layout_Mode, ID_View_Layout_2DView,
    ID_View_Layout_Legend, ID_View_Layout_EventTable, ID_View_Layout_Text,
    ID_View_Layout_Image, ID_View_Viewport_Top, ID_View_Viewport_Front,
    ID_View_Viewport_Side, ID_View_Viewport_SelectTool,
    ID_View_Viewport_MeasureTool, ID_View_Viewport_GapMeasureTool,
    ID_View_Viewport_AxisConstraint, ID_View_Viewport_LeftDragMove,
    ID_View_Viewport_LocalAxes, ID_View_Viewport_Magnet,
    ID_View_Viewport_CrossTableActions, ID_Tools_DownloadGdtf,
    ID_Tools_EditDictionaries, ID_Tools_ImportRiderText,
    ID_Tools_DistributeHoistWeights, ID_Tools_ExportFixture,
    ID_Tools_ExportTruss, ID_Tools_ExportSceneObject, ID_Tools_AutoPatch,
    ID_Tools_AutoColor, ID_Tools_ConvertToHoist,
    ID_Tools_ConvertSceneObjectsToTruss, ID_Tools_GenerateFixtureSymbols,
    ID_Tools_AssignSelectedFixtureCategory, ID_Tools_OpenUserLibraryFolder,
    ID_Tools_DistributeFixturesOnTruss,
    ID_Tools_DistributeFixturesBetweenPoints, ID_Tools_DistributeFixtures,
    ID_Help_Help, ID_Help_OnlineDocumentation, ID_Help_OpenLogsFolder,
    ID_Help_ExportDiagnosticReport, ID_Help_About, ID_Help_CheckForUpdates,
    ID_Select_Fixtures, ID_Select_Trusses, ID_Select_Supports,
    ID_Select_Objects};

// Reports whether every distinct MainWindow action has a unique numeric ID.
constexpr bool AllCommandIdsAreUnique() {
  for (std::size_t left = 0; left < kAllDistinctCommandIds.size(); ++left) {
    for (std::size_t right = left + 1; right < kAllDistinctCommandIds.size();
         ++right) {
      if (kAllDistinctCommandIds[left] == kAllDistinctCommandIds[right]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(AllCommandIdsAreUnique(),
              "MainWindow command IDs must be unique for distinct actions");
static_assert(kAllDistinctCommandIds.size() ==
                  static_cast<std::size_t>(ID_MainWindowCommand_End -
                                           ID_File_New),
              "The MainWindow command registry must contain every command ID");

} // namespace mainwindow::commands
