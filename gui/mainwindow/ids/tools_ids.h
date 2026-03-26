#pragma once

#include "view_ids.h"

constexpr int ID_Tools_DownloadGdtf = ID_View_Viewport_Side + 1;
constexpr int ID_Tools_EditDictionaries = ID_Tools_DownloadGdtf + 1;
constexpr int ID_Tools_ImportRiderText = ID_Tools_EditDictionaries + 1;
constexpr int ID_Tools_DistributeHoistWeights =
    ID_Tools_ImportRiderText + 1;
constexpr int ID_Tools_ExportFixture =
    ID_Tools_DistributeHoistWeights + 1;
constexpr int ID_Tools_ExportTruss = ID_Tools_ExportFixture + 1;
constexpr int ID_Tools_ExportSceneObject = ID_Tools_ExportTruss + 1;
constexpr int ID_Tools_AutoPatch = ID_Tools_ExportSceneObject + 1;
constexpr int ID_Tools_AutoColor = ID_Tools_AutoPatch + 1;
constexpr int ID_Tools_ConvertToHoist = ID_Tools_AutoColor + 1;
constexpr int ID_Tools_GenerateFixtureSymbols = ID_Tools_ConvertToHoist + 1;
constexpr int ID_Tools_AssignSelectedFixtureCategory =
    ID_Tools_GenerateFixtureSymbols + 1;
