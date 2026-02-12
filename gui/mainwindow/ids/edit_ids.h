#pragma once

#include "tools_ids.h"

inline constexpr int ID_Help_Help = ID_Tools_ConvertToHoist + 1;
inline constexpr int ID_Help_About = ID_Help_Help + 1;
inline constexpr int ID_Select_Fixtures = ID_Help_About + 1;
inline constexpr int ID_Select_Trusses = ID_Select_Fixtures + 1;
inline constexpr int ID_Select_Supports = ID_Select_Trusses + 1;
inline constexpr int ID_Select_Objects = ID_Select_Supports + 1;
inline constexpr int ID_Edit_Undo = ID_Select_Objects + 1;
inline constexpr int ID_Edit_Redo = ID_Edit_Undo + 1;
inline constexpr int ID_Edit_AddFixture = ID_Edit_Redo + 1;
inline constexpr int ID_Edit_AddTruss = ID_Edit_AddFixture + 1;
inline constexpr int ID_Edit_AddSceneObject = ID_Edit_AddTruss + 1;
inline constexpr int ID_Edit_Delete = ID_Edit_AddSceneObject + 1;
inline constexpr int ID_Edit_Preferences = ID_Edit_Delete + 1;
