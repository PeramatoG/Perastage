#!/usr/bin/env python3
"""Guard the lightweight live-transform boundary in both viewers."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def function_body(path: str, signature: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    start = text.find(signature)
    assert start >= 0, f"missing {signature} in {path}"
    opening = text.find("{", start)
    depth = 0
    for index in range(opening, len(text)):
        depth += text[index] == "{"
        depth -= text[index] == "}"
        if depth == 0:
            return text[opening : index + 1]
    raise AssertionError(f"unterminated {signature}")


for path, panel, apply_name in (
    ("viewer2d/viewer2dpanel.cpp", "Viewer2DPanel", "ApplySelectionDelta"),
    ("viewer3d/viewer3dpanel.cpp", "Viewer3DPanel", "ApplySelectionDragDelta"),
):
    apply_body = function_body(path, f"bool {panel}::{apply_name}")
    assert "TranslateTargets" in apply_body
    for forbidden in (
        "MarkSceneTransformsDirty",
        "RefreshTransformCachesIfDirty",
        "RebuildIfDirty",
        "InvalidateTransformedBounds",
        "UpdateResourcesIfDirty",
    ):
        assert forbidden not in apply_body
    motion = function_body(path, f"void {panel}::OnMouseMove")
    assert "ResolveLatestPointer" in motion
    assert "PresentInteractiveTransformFrame" in motion

controller = (ROOT / "viewer3d/viewer3dcontroller.cpp").read_text(encoding="utf-8")
assert "m_impl->cameraMoving && context.fastInteractionMode" in controller
assert "interactiveTransformActive" not in controller
print("Interactive transform presentation architecture is intact.")
