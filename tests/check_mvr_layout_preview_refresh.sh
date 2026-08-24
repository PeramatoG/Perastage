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
    layout_index = body.find("NotifySceneVisualContentChanged", panels_index)
    if panels_index < 0 or resume_index < 0 or layout_index < 0:
        raise SystemExit(
            f"{function_name} must resume rendering and notify Layout previews after panel updates"
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
if "NotifySceneVisualContentChanged" in refresh_match.group(0):
    raise SystemExit(
        "Layout preview notification must not be queued while the MVR pipeline is active"
    )

required_hooks = {
    "gui/scene_view_refresh.cpp": "mainWindow->NotifySceneVisualContentChanged()",
    "gui/mainwindow.cpp": "void MainWindow::NotifySceneVisualContentChanged()",
    "gui/hoisttablepanel.cpp": "RefreshLayout2DViewsAfterSceneChange(this)",
}
for relative_path, token in required_hooks.items():
    source = open(f"{sys.argv[1].rsplit('/gui/', 1)[0]}/{relative_path}", encoding="utf-8").read()
    if token not in source:
        raise SystemExit(f"Missing general Layout scene-change hook in {relative_path}")

for relative_path in (
    "viewer2d/viewer2dpanel.cpp",
    "viewer3d/viewer3dpanel.cpp",
):
    source = open(f"{sys.argv[1].rsplit('/gui/', 1)[0]}/{relative_path}", encoding="utf-8").read()
    if "NotifySceneVisualContentChanged" in source:
        raise SystemExit(
            f"Viewer scene reloads must not invalidate Layout previews recursively: {relative_path}"
        )

offscreen_source = open(
    f"{sys.argv[1].rsplit('/gui/', 1)[0]}/viewer2d/viewer2doffscreenrenderer.cpp",
    encoding="utf-8",
).read()
prepare_match = re.search(
    r"void Viewer2DOffscreenRenderer::PrepareForCapture\(\) \{[\s\S]*?\n\}",
    offscreen_source,
)
if not prepare_match or "panel_->UpdateScene()" not in prepare_match.group(0):
    raise SystemExit(
        "The offscreen Layout capture panel must synchronize current scene data before capture"
    )

invalidation_source = open(
    f"{sys.argv[1].rsplit('/gui/', 1)[0]}/gui/layoutviewerpanel_render_invalidation.cpp",
    encoding="utf-8",
).read()
invalidation_match = re.search(
    r"void LayoutViewerPanel::InvalidateViewCacheForSceneContent\(ViewCache &cache\) "
    r"\{[\s\S]*?\n\}",
    invalidation_source,
)
if not invalidation_match:
    raise SystemExit("Unable to inspect scene-dependent Layout cache invalidation")
invalidation_body = invalidation_match.group(0)
for token in (
    "cache.captureVersion = -1",
    "cache.persistentRgba.clear()",
    "cache.persistentRgbaContentHash = 0",
    "cache.renderDirty = true",
):
    if token not in invalidation_body:
        raise SystemExit(f"Scene-dependent Layout cache invalidation is missing: {token}")
if invalidation_source.count("InvalidateViewCacheForSceneContent(") < 3:
    raise SystemExit(
        "Explicit notifications and scene-hash detection must both discard stale Layout rasters"
    )

layout_panel_source = open(
    f"{sys.argv[1].rsplit('/gui/', 1)[0]}/gui/layoutviewerpanel.cpp",
    encoding="utf-8",
).read()
show_match = re.search(
    r"void LayoutViewerPanel::OnShow\(wxShowEvent &event\) \{[\s\S]*?\n\}",
    layout_panel_source,
)
if not show_match or "InvalidateRenderIfFrameChanged(true)" not in show_match.group(0):
    raise SystemExit(
        "Showing the Layout pane must detect scene changes missed while it was hidden"
    )
PY
