#pragma once

#include "tools_ids.h"

constexpr int ID_Help_Help = ID_Tools_OpenUserLibraryFolder + 1;
constexpr int ID_Help_OnlineDocumentation = ID_Help_Help + 1;
constexpr int ID_Help_OpenLogsFolder = ID_Help_OnlineDocumentation + 1;
constexpr int ID_Help_ExportDiagnosticReport = ID_Help_OpenLogsFolder + 1;
constexpr int ID_Help_About = ID_Help_ExportDiagnosticReport + 1;
constexpr int ID_Help_CheckForUpdates = ID_Help_About + 1;
constexpr int ID_Select_Fixtures = ID_Help_CheckForUpdates + 1;
constexpr int ID_Select_Trusses = ID_Select_Fixtures + 1;
constexpr int ID_Select_Supports = ID_Select_Trusses + 1;
constexpr int ID_Select_Objects = ID_Select_Supports + 1;
constexpr int ID_Edit_Undo = ID_Select_Objects + 1;
constexpr int ID_Edit_Redo = ID_Edit_Undo + 1;
constexpr int ID_Edit_AddFixture = ID_Edit_Redo + 1;
constexpr int ID_Edit_AddTruss = ID_Edit_AddFixture + 1;
constexpr int ID_Edit_AddSceneObject = ID_Edit_AddTruss + 1;
constexpr int ID_Edit_AddPrimitiveSphere = ID_Edit_AddSceneObject + 1;
constexpr int ID_Edit_AddPrimitiveCube = ID_Edit_AddPrimitiveSphere + 1;
constexpr int ID_Edit_AddPrimitiveCylinder = ID_Edit_AddPrimitiveCube + 1;
constexpr int ID_Edit_Delete = ID_Edit_AddPrimitiveCylinder + 1;
constexpr int ID_Edit_Group = ID_Edit_Delete + 1;
constexpr int ID_Edit_Ungroup = ID_Edit_Group + 1;
constexpr int ID_Edit_ReplaceFixtures = ID_Edit_Ungroup + 1;
constexpr int ID_Edit_ReplaceTrusses = ID_Edit_ReplaceFixtures + 1;
constexpr int ID_Edit_Preferences = ID_Edit_ReplaceTrusses + 1;
