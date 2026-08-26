#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$repo_root/tests/test_tool_requirements.sh"

run_test_python - "$repo_root" <<'PY'
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
if "onlineCatalogLoadAttempted" not in dialog or "BeginOnlineCatalogAcquisition" not in dialog:
    raise SystemExit("Resolver must attempt shared online catalog acquisition after a cache miss")
if "DetermineCatalogAccessAction" not in workflow:
    raise SystemExit("Resolver catalog acquisition must use the shared GDTF access policy")
if "gdtf_download_filename::ChooseDestination" not in workflow:
    raise SystemExit("Resolver downloads must use the shared readable filename policy")
online_loader_start = workflow.find("auto loadOnlineCatalog")
credential_request_start = workflow.find("auto requestCatalogCredentials", online_loader_start)
if online_loader_start < 0 or credential_request_start < 0:
    raise SystemExit("Resolver must expose focused online catalog and credential callbacks")
automatic_online_path = workflow[online_loader_start:credential_request_start]
if "WaitForNetworkTask" in automatic_online_path or "wxProgressDialog" in automatic_online_path:
    raise SystemExit("Automatic resolver catalog acquisition must not open nested progress dialogs")
if "std::stop_token" not in automatic_online_path:
    raise SystemExit("Automatic resolver catalog acquisition must support owned cancellation")
if "Acquiring the shared GDTF catalog" in dialog:
    raise SystemExit("Resolver must not retain the obsolete non-terminal acquisition label")
if "RefreshCatalogCompletionStatus" not in dialog:
    raise SystemExit("Resolver acquisition must publish a stable final status")
if "BuildCatalogMatchTargets" not in dialog:
    raise SystemExit("Resolver matching must skip dictionary and explicit rows")
for source in ("rider_fixture_resolution_dialog.cpp",
               "rider_fixture_resolution_model.cpp",
               "rider_fixture_resolution_workflow.cpp"):
    if source not in gui_cmake:
        raise SystemExit(f"GUI CMake does not compile {source}")

print("OK: Create from text is gated by the compiled fixture-resolution workflow.")
PY
