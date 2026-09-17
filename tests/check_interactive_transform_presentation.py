#!/usr/bin/env python3
"""Guard lightweight, coalesced live-transform presentation in both viewers."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def function_body(path: str, signature: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    start = text.find(signature)
    assert start >= 0, f"missing {signature} in {path}"
    opening = text.find("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening : index + 1]
    raise AssertionError(f"unterminated {signature} in {path}")


incremental = function_body(
    "viewer3d/viewer3dcontroller.cpp",
    "void Viewer3DController::MarkInteractiveTransformsDirty(",
)
assert "InvalidateTransformedBounds" in incremental
assert "interactiveTransformActive = true" in incremental
for forbidden in ("sceneChangedDirty", "sortedListsDirty", "MarkResourceSyncPending", "RebuildIfDirty"):
    assert forbidden not in incremental

for path, panel, repaint in (
    ("viewer2d/viewer2dpanel_presentation.cpp", "Viewer2DPanel", "RequestRepaint()"),
    ("viewer3d/viewer3dpanel_presentation.cpp", "Viewer3DPanel", "Refresh(false)"),
):
    present = function_body(path, f"void {panel}::PresentInteractiveTransformFrame()")
    assert repaint in present
    for forbidden in ("UpdateScene", "UpdateResourcesIfDirty", "MarkSceneTransformsDirty"):
        assert forbidden not in present
    if panel == "Viewer3DPanel":
        assert "Update()" not in present
    finish = function_body(path, f"void {panel}::FinishInteractiveTransformPresentation()")
    assert "SetInteractiveTransformActive(false)" in finish
    assert "MarkSceneTransformsDirty" in finish and "Update()" in finish

for path, panel in (
    ("viewer2d/viewer2dpanel.cpp", "Viewer2DPanel"),
    ("viewer3d/viewer3dpanel.cpp", "Viewer3DPanel"),
):
    motion = function_body(path, f"void {panel}::OnMouseMove")
    assert "if (changed)" in motion
    assert "PresentInteractiveTransformFrame()" in motion
    assert "UpdateResourcesIfDirty" not in motion

controller = (ROOT / "viewer3d/viewer3dcontroller.cpp").read_text(encoding="utf-8")
assert "interactiveTransformActive" in controller
assert "m_impl->cameraMoving ||" in controller
viewer2d = (ROOT / "viewer2d/viewer2dpanel.cpp").read_text(encoding="utf-8")
viewer3d = (ROOT / "viewer3d/viewer3dpanel.cpp").read_text(encoding="utf-8")
assert "drawFixtureLabels && !suppressInteractiveLabels" in viewer2d
assert "m_controller.IsInteractiveTransformActive() ||" in viewer3d

print("Interactive transform presentation architecture is intact.")
