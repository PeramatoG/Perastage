#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 - "$repo_root" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
io_source = (root / "gui/mainwindow_io.cpp").read_text(encoding="utf-8")
workflow = (root / "gui/rider_fixture_resolution_workflow.cpp").read_text(encoding="utf-8")
dialog = (root / "gui/rider_fixture_resolution_dialog.cpp").read_text(encoding="utf-8")
gui_cmake = (root / "gui/CMakeLists.txt").read_text(encoding="utf-8")

preflight = io_source.find("RunCreateFromTextPreflight")
scene_import = io_source.find("RiderImporter::ImportText", preflight)
if preflight < 0 or scene_import < 0 or preflight >= scene_import:
    raise SystemExit("Create from text must run fixture preflight before ImportText")

required = {
    "runtime service analysis": "rider_fixture_resolution::Service::Analyze",
    "modal resolver": "RiderFixtureResolutionDialog dialog",
    "external dictionary persistence": "CreateOrUpdateExternalLibraryMapping",
}
for label, token in required.items():
    if token not in workflow:
        raise SystemExit(f"Missing {label}: {token}")

if "GdtfSearchDialog dialog" not in dialog or "item->effectiveFixtureType" not in dialog:
    raise SystemExit("Resolver Search must call GdtfSearchDialog with the rider alias")
for source in ("rider_fixture_resolution_dialog.cpp",
               "rider_fixture_resolution_model.cpp",
               "rider_fixture_resolution_workflow.cpp"):
    if source not in gui_cmake:
        raise SystemExit(f"GUI CMake does not compile {source}")

print("OK: Create from text is gated by the compiled fixture-resolution workflow.")
PY
