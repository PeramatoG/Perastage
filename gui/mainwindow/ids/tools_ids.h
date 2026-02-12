#pragma once

#include "view_ids.h"

inline constexpr int ID_Tools_DownloadGdtf = ID_View_Layout_Image + 1;
inline constexpr int ID_Tools_EditDictionaries = ID_Tools_DownloadGdtf + 1;
inline constexpr int ID_Tools_ImportRiderText = ID_Tools_EditDictionaries + 1;
inline constexpr int ID_Tools_ExportFixture = ID_Tools_ImportRiderText + 1;
inline constexpr int ID_Tools_ExportTruss = ID_Tools_ExportFixture + 1;
inline constexpr int ID_Tools_ExportSceneObject = ID_Tools_ExportTruss + 1;
inline constexpr int ID_Tools_AutoPatch = ID_Tools_ExportSceneObject + 1;
inline constexpr int ID_Tools_AutoColor = ID_Tools_AutoPatch + 1;
inline constexpr int ID_Tools_ConvertToHoist = ID_Tools_AutoColor + 1;
