#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${PERASTAGE_TEST_PYTHON:-python3}" - \
    "$repo_root/gui/mainwindow/ids/mainwindow_command_ids.h" \
    "$repo_root/gui/mainwindow_menu_builders.cpp" \
    "$repo_root/gui/mainwindow.cpp" <<'PY'
import re
import sys

ids_text, menu_text, window_text = (
    open(path, encoding="utf-8").read() for path in sys.argv[1:]
)

enum_body = re.search(
    r"enum MainWindowCommandId\s*\{(.*?)ID_MainWindowCommand_End", ids_text, re.S
)
if not enum_body:
    raise SystemExit("MainWindow command IDs must use the central command enum")

entries = re.findall(r"\b(ID_(?:File|Edit|View|Tools|Help|Select)_[A-Za-z0-9_]+)\b", enum_body.group(1))
if len(entries) != len(set(entries)):
    raise SystemExit("Duplicate symbolic command in MainWindow command enum")
assignments = re.findall(r"\bID_[A-Za-z0-9_]+\s*=", enum_body.group(1))
if assignments != ["ID_File_New ="]:
    raise SystemExit("Only the first MainWindow command may set an explicit value")
if "static_assert(AllCommandIdsAreUnique()" not in ids_text:
    raise SystemExit("Missing compile-time MainWindow command-ID uniqueness guard")

expected_routes = {
    "ID_File_New": "OnNew", "ID_File_Load": "OnLoad",
    "ID_File_Save": "OnSave", "ID_File_SaveAs": "OnSaveAs",
    "ID_File_ImportMVR": "OnImportMVR", "ID_File_ExportMVR": "OnExportMVR",
    "ID_File_MvrXchange": "OnMvrXchange",
    "ID_File_PrintViewer2D": "OnPrintViewer2D",
    "ID_File_PrintLayout": "OnPrintLayout", "ID_File_PrintTable": "OnPrintTable",
    "ID_File_ExportCSV": "OnExportCSV", "ID_File_Close": "OnClose",
    "ID_Edit_Undo": "OnUndo", "ID_Edit_Redo": "OnRedo",
    "ID_Edit_Cut": "OnCut", "ID_Edit_Copy": "OnCopy", "ID_Edit_Paste": "OnPaste",
    "ID_Edit_AddFixture": "OnAddFixture", "ID_Edit_AddTruss": "OnAddTruss",
    "ID_Edit_AddSceneObject": "OnAddSceneObject",
    "ID_Edit_AddPrimitiveSphere": "OnAddPrimitiveSphere",
    "ID_Edit_AddPrimitiveCube": "OnAddPrimitiveCube",
    "ID_Edit_AddPrimitiveCylinder": "OnAddPrimitiveCylinder",
    "ID_Edit_Delete": "OnDelete", "ID_Edit_Group": "OnGroupSelection",
    "ID_Edit_Ungroup": "OnUngroupSelection",
    "ID_Edit_ReplaceFixtures": "OnReplaceSelectedFixtures",
    "ID_Edit_ReplaceTrusses": "OnReplaceSelectedTrusses",
    "ID_Edit_Preferences": "OnPreferences",
    "ID_View_ToggleConsole": "OnToggleConsole",
    "ID_View_ToggleFixtures": "OnToggleFixtures",
    "ID_View_ToggleViewport": "OnToggleViewport",
    "ID_View_ToggleViewport2D": "OnToggleViewport2D",
    "ID_View_ToggleRender2D": "OnToggleRender2D",
    "ID_View_ToggleLayers": "OnToggleLayers",
    "ID_View_ToggleLayouts": "OnToggleLayouts",
    "ID_View_ToggleSummary": "OnToggleSummary",
    "ID_View_ToggleRigging": "OnToggleRigging",
    "ID_View_Layout_Default": "OnApplyDefaultLayout",
    "ID_View_Layout_2D": "OnApply2DLayout",
    "ID_View_Layout_Mode": "OnApplyLayoutModeLayout",
    "ID_Tools_DownloadGdtf": "OnDownloadGdtf",
    "ID_Tools_EditDictionaries": "OnEditDictionaries",
    "ID_Tools_OpenUserLibraryFolder": "OnOpenUserLibraryFolder",
    "ID_Tools_ImportRiderText": "OnImportRiderText",
    "ID_Tools_DistributeHoistWeights": "OnDistributeHoistWeights",
    "ID_Tools_DistributeFixtures": "OnDistributeFixtures",
    "ID_Tools_ExportFixture": "OnExportFixture",
    "ID_Tools_ExportTruss": "OnExportTruss",
    "ID_Tools_ExportSceneObject": "OnExportSceneObject",
    "ID_Tools_AutoPatch": "OnAutoPatch", "ID_Tools_AutoColor": "OnAutoColor",
    "ID_Tools_ConvertToHoist": "OnConvertToHoist",
    "ID_Tools_ConvertSceneObjectsToTruss": "OnConvertSceneObjectsToTruss",
    "ID_Tools_GenerateFixtureSymbols": "OnGenerateFixtureSymbols",
    "ID_Tools_AssignSelectedFixtureCategory": "OnAssignSelectedFixtureCategory",
    "ID_Help_Help": "OnShowHelp",
    "ID_Help_OnlineDocumentation": "OnOpenOnlineDocumentation",
    "ID_Help_CheckForUpdates": "OnCheckForUpdates",
    "ID_Help_OpenLogsFolder": "OnOpenLogsFolder",
    "ID_Help_ExportDiagnosticReport": "OnExportDiagnosticReport",
    "ID_Help_About": "OnShowAbout",
}

menu_ids = re.findall(
    r"->Append(?:CheckItem)?\s*\(\s*(ID_[A-Za-z0-9_]+)", menu_text, re.S
)
if set(menu_ids) != set(expected_routes):
    missing = sorted(set(expected_routes) - set(menu_ids))
    unexpected = sorted(set(menu_ids) - set(expected_routes))
    raise SystemExit(f"Main menu contract changed; missing={missing}, unexpected={unexpected}")
if len(menu_ids) != len(set(menu_ids)):
    raise SystemExit("A visible MainWindow menu command is registered more than once")

event_pairs = re.findall(
    r"EVT_MENU\s*\(\s*(ID_[A-Za-z0-9_]+)\s*,\s*MainWindow::(On[A-Za-z0-9_]+)\s*\)",
    window_text,
    re.S,
)
event_routes = {}
for command_id, handler in event_pairs:
    if command_id in event_routes:
        raise SystemExit(f"Duplicate EVT_MENU registration for {command_id}")
    event_routes[command_id] = handler

for command_id, expected_handler in expected_routes.items():
    actual_handler = event_routes.get(command_id)
    if actual_handler != expected_handler:
        raise SystemExit(
            f"Wrong route for {command_id}: expected {expected_handler}, found {actual_handler}"
        )

distribution_ids = {
    "ID_Tools_DistributeFixturesOnTruss": "OnDistributeFixturesOnTruss",
    "ID_Tools_DistributeFixturesBetweenPoints": "OnDistributeFixturesBetweenPoints",
    "ID_Tools_DistributeFixtures": "OnDistributeFixtures",
}
for command_id, expected_handler in distribution_ids.items():
    if event_routes.get(command_id) != expected_handler:
        raise SystemExit(f"Wrong specialized distribution route for {command_id}")

help_ids = {"ID_Help_Help", "ID_Help_OnlineDocumentation", "ID_Help_OpenLogsFolder"}
numeric_ids = {command_id: offset for offset, command_id in enumerate(entries)}
if ({numeric_ids[command_id] for command_id in help_ids} &
        {numeric_ids[command_id] for command_id in distribution_ids}):
    raise SystemExit("Help and fixture-distribution commands must be distinct actions")
if not set(event_routes).issubset(set(entries)):
    raise SystemExit("EVT_MENU uses a command outside the central MainWindow command enum")

# Menu and toolbar controls may intentionally reuse one ID for the same action;
# this contract requires uniqueness only among the distinct IDs in the registry.
print("OK: MainWindow menu IDs and routes match the complete command contract.")
PY
