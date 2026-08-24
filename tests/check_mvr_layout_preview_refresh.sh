#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
controller_cpp="$repo_root/gui/mainwindow/controllers/mainwindow_io_controller.cpp"

run_test_python - "$controller_cpp" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()

for function_name, end_marker in (
    ("ImportMvrFromPath", "// Refreshes all scene-dependent UI panels"),
    ("MergeMvrFromPath", "// Applies the official MVR-open policy"),
):
    match = re.search(
        rf"bool MainWindowIoController::{function_name}\([\s\S]*?{re.escape(end_marker)}",
        text,
    )
    if not match:
        raise SystemExit(f"Unable to inspect {function_name}")

    body = match.group(0)
    panels_index = body.rfind("RefreshPanelsAfterMvrSceneChange")
    resume_index = body.find("mvrImportPipelineActive = false", panels_index)
    layout_index = body.find("RefreshAfterSceneContentUpdate", panels_index)
    if panels_index < 0 or resume_index < 0 or layout_index < 0:
        raise SystemExit(
            f"{function_name} must resume rendering and refresh Layout previews after panel updates"
        )
    if not panels_index < resume_index < layout_index:
        raise SystemExit(
            f"{function_name} must resume MVR rendering before queuing the Layout preview refresh"
        )

refresh_match = re.search(
    r"void MainWindowIoController::RefreshPanelsAfterMvrSceneChange\([\s\S]*?"
    r"\n}\n\n// Opens a file picker",
    text,
)
if not refresh_match:
    raise SystemExit("Unable to inspect RefreshPanelsAfterMvrSceneChange")
if "RefreshAfterSceneContentUpdate" in refresh_match.group(0):
    raise SystemExit(
        "Layout preview refresh must not be queued while the MVR pipeline is active"
    )
PY
