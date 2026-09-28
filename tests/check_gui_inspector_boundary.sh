#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dialog="$repo_root/gui/inspection/inspector_workspace_panel.cpp"
cmake="$repo_root/gui/CMakeLists.txt"
layout="$repo_root/gui/mainwindow_layout.cpp"
window="$repo_root/gui/mainwindow.cpp"
presets="$repo_root/core/layoutviewpresets.cpp"

rg -q 'InspectGdtf' "$dialog"
rg -q 'InspectMvr' "$dialog"
rg -q 'DescribePackageResources' "$dialog"
rg -q 'perastage_inspection_gdtf' "$cmake"
rg -q 'perastage_inspection_mvr' "$cmake"
rg -q 'perastage_inspection_resource' "$cmake"
rg -q 'class InspectorWorkspacePanel final : public wxPanel' \
  "$repo_root/gui/inspection/inspector_workspace_panel.h"
rg -Fq '.Name("InspectorWorkspace")' "$layout"
rg -q '"inspector_view"' "$presets"
rg -U -q 'preset\.name == "inspector_view"[^;]*ActivateInspectorIfVisible\(\)' "$layout"
rg -U -q 'ActivateInspectorIfVisible\(\)[^}]*!startupProjectLoadPending[^}]*IsPaneShown\(auiManager, "InspectorWorkspace"\)[^}]*inspectorWorkspacePanel->Activate\(\)' "$layout"
rg -U -q '!pending[^;]*ActivateInspectorIfVisible\(\)' "$window"

forbidden='wxDialog|cli/|inspect_command|inspection_report_json|ConfigManager|'
forbidden+='mvrimporter|mvrexporter|MvrScene|gdtf_(document_)?mutation|gdtf.*writer|'
forbidden+='GdtfShare|gdtf_share|network|credential|Viewer[23]D|TablePanel|'
forbidden+='MainWindow::Instance|tinyxml|wxZip|ReadGdtfArchive'
if rg -ni "$forbidden" "$repo_root/gui/inspection"; then
  echo "Inspector GUI must consume only structured Core inspection APIs" >&2
  exit 1
fi

echo "GUI Inspector boundary checks passed."
