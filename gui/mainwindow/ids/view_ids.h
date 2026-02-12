#pragma once

#include "io_ids.h"

inline constexpr int ID_View_ToggleConsole = ID_File_Close + 1;
inline constexpr int ID_View_ToggleFixtures = ID_View_ToggleConsole + 1;
inline constexpr int ID_View_ToggleViewport = ID_View_ToggleFixtures + 1;
inline constexpr int ID_View_ToggleViewport2D = ID_View_ToggleViewport + 1;
inline constexpr int ID_View_ToggleRender2D = ID_View_ToggleViewport2D + 1;
inline constexpr int ID_View_ToggleLayers = ID_View_ToggleRender2D + 1;
inline constexpr int ID_View_ToggleLayouts = ID_View_ToggleLayers + 1;
inline constexpr int ID_View_ToggleSummary = ID_View_ToggleLayouts + 1;
inline constexpr int ID_View_ToggleRigging = ID_View_ToggleSummary + 1;
inline constexpr int ID_View_Layout_Default = ID_View_ToggleRigging + 1;
inline constexpr int ID_View_Layout_2D = ID_View_Layout_Default + 1;
inline constexpr int ID_View_Layout_Mode = ID_View_Layout_2D + 1;
inline constexpr int ID_View_Layout_2DView = ID_View_Layout_Mode + 1;
inline constexpr int ID_View_Layout_Legend = ID_View_Layout_2DView + 1;
inline constexpr int ID_View_Layout_EventTable = ID_View_Layout_Legend + 1;
inline constexpr int ID_View_Layout_Text = ID_View_Layout_EventTable + 1;
inline constexpr int ID_View_Layout_Image = ID_View_Layout_Text + 1;
