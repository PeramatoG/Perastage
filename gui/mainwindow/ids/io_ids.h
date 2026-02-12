#pragma once

#include <wx/defs.h>

inline constexpr int ID_File_New = wxID_HIGHEST + 1;
inline constexpr int ID_File_Load = ID_File_New + 1;
inline constexpr int ID_File_Save = ID_File_Load + 1;
inline constexpr int ID_File_SaveAs = ID_File_Save + 1;
inline constexpr int ID_File_ImportRider = ID_File_SaveAs + 1;
inline constexpr int ID_File_ImportMVR = ID_File_ImportRider + 1;
inline constexpr int ID_File_ExportMVR = ID_File_ImportMVR + 1;
inline constexpr int ID_File_PrintViewer2D = ID_File_ExportMVR + 1;
inline constexpr int ID_File_PrintLayout = ID_File_PrintViewer2D + 1;
inline constexpr int ID_File_PrintTable = ID_File_PrintLayout + 1;
inline constexpr int ID_File_PrintMenu = ID_File_PrintTable + 1;
inline constexpr int ID_File_ExportCSV = ID_File_PrintMenu + 1;
inline constexpr int ID_File_Close = ID_File_ExportCSV + 1;
