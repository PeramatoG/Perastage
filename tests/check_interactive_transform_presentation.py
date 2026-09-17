#!/usr/bin/env python3
"""Guard lightweight live-transform presentation in both scene viewers."""

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


mark_dirty = function_body(
    "viewer3d/viewer3dcontroller.cpp",
    "void Viewer3DController::MarkSceneTransformsDirty()",
)
assert "sceneChangedDirty = true" in mark_dirty
assert "sortedListsDirty = true" in mark_dirty
assert "MarkResourceSyncPending" not in mark_dirty

lightweight = function_body(
    "viewer3d/viewer3dcontroller.cpp",
    "void Viewer3DController::UpdateFrameStateLightweight()",
)
assert "RefreshTransformCachesIfDirty" in lightweight

for path, signature, repaint in (
    (
        "viewer2d/viewer2dpanel_presentation.cpp",
        "void Viewer2DPanel::PresentInteractiveTransformFrame()",
        "RequestRepaint()",
    ),
    (
        "viewer3d/viewer3dpanel_presentation.cpp",
        "void Viewer3DPanel::PresentInteractiveTransformFrame()",
        "Refresh(false)",
    ),
):
    body = function_body(path, signature)
    assert repaint in body and "Update()" in body

for path in ("viewer2d/viewer2dpanel.cpp", "viewer3d/viewer3dpanel.cpp"):
    motion = function_body(path, "void Viewer" + path.split("viewer")[1][:2].upper() + "Panel::OnMouseMove")
    assert "if (changed)" in motion
    assert "PresentInteractiveTransformFrame()" in motion

print("Interactive transform presentation architecture is intact.")
