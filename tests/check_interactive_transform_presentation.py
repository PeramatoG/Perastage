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


for dimensions, apply_name, mark_activity in (
    ("2d", "ApplySelectionDelta", "MarkInteractionActivity"),
    ("3d", "ApplySelectionDragDelta", "RecordTransformActivity"),
):
    panel = f"Viewer{dimensions.upper()}Panel"
    path = f"viewer{dimensions}/viewer{dimensions}panel.cpp"
    presentation = path.replace(".cpp", "_presentation.cpp")
    apply_body = function_body(path, f"bool {panel}::{apply_name}")
    assert apply_body.index("!hasTranslation && !realign") < apply_body.index("TranslateTargets")
    assert mark_activity in apply_body
    assert "MarkTransformApplied" in apply_body
    for forbidden in (
        "MarkSceneTransformsDirty",
        "RefreshTransformCachesIfDirty",
        "RebuildIfDirty",
        "InvalidateTransformedBounds",
        "UpdateResourcesIfDirty",
        "Change::Scene",
    ):
        assert forbidden not in apply_body

    motion = function_body(path, f"void {panel}::OnMouseMove")
    assert "ResolveLatestPointer" in motion
    assert "UpdateContinuousPlacementPointer(pos)" in motion
    release = function_body(path, f"void {panel}::OnMouseUp")
    assert release.index("UpdateContinuousPlacementPointer(event.GetPosition())") < release.index(
        "ConfirmContinuousPlacement()"
    )
    assert "UpdateSelectionDragPointer(event.GetPosition())" in release
    for method in ("PresentInteractiveTransformFrame", "FinishInteractiveTransformPresentation"):
        body = function_body(presentation, f"void {panel}::{method}")
        assert "Update();" not in body
        assert "NotifyChange" not in body
    paint = (ROOT / path).read_text(encoding="utf-8")
    assert "TakePresentedFeedback()" in paint
    assert paint.count("NotifyChange(ConfigManager::Change::TransformPreview)") == 1

controller = (ROOT / "viewer3d/viewer3dcontroller.cpp").read_text(encoding="utf-8")
assert "m_impl->cameraMoving && context.fastInteractionMode" in controller
assert "culling.enabled && !previewingTransforms" in controller
patch = (ROOT / "gui/patch/dmx_patch_panel.cpp").read_text(encoding="utf-8")
assert "Change::Scene" in patch and "Change::TransformPreview" not in patch
position_tool = (ROOT / "gui/positiontoolpanel.cpp").read_text(encoding="utf-8")
assert "Change::TransformPreview" in position_tool
print("Interactive transform presentation architecture is intact.")
