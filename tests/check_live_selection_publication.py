#!/usr/bin/env python3
"""Protect the active-project selection publication boundary."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def function_body(path: str, signature: str) -> str:
    """Return a C++ function body by balancing its braces."""
    source = (ROOT / path).read_text(encoding="utf-8")
    start = source.find(signature)
    assert start >= 0, f"Missing function: {signature}"
    opening = source.find("{", start)
    assert opening >= 0, f"Missing function body: {signature}"
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"Unterminated function body: {signature}")


def main() -> None:
    """Keep selection publication synchronized without scene side effects."""
    active = function_body(
        "app/active_project_command_context.cpp",
        "void ActiveProjectCommandContext::Publish(",
    )
    assert re.search(
        r"if\s*\(mutation\.sceneChanged\)\s*"
        r"window\.RefreshAfterToolSceneUpdate\(selection_\);\s*"
        r"else if\s*\(mutation\.selectionChanged\)\s*"
        r"window\.RefreshAfterToolSelectionUpdate\(selection_\);",
        active,
    ), "Selection-only Commands must use the selection-only GUI refresh path"

    selection = function_body(
        "gui/mainwindow_selection_refresh.cpp",
        "void MainWindow::RefreshAfterToolSelectionUpdate(",
    )
    for category, setter, panel in [
        ("fixtures", "SetSelectedFixtures", "fixturePanel"),
        ("trusses", "SetSelectedTrusses", "trussPanel"),
        ("supports", "SetSelectedSupports", "hoistPanel"),
        ("sceneObjects", "SetSelectedSceneObjects", "sceneObjPanel"),
    ]:
        assert f"config_.{setter}(selection_.{category});" in active, category
        assert f"config.{setter}(selection.{category});" in selection, category
        assert f"{panel}->SelectByUuid(selection.{category}, false);" in selection, (
            f"{category} must update table rows without recursive selection events"
        )
        assert f"appendSelection(selection.{category});" in selection, category

    for publication in [
        "viewportPanel->SetSelectedFixtures(mergedSelection);",
        "viewport2DPanel->SetSelectedUuids(mergedSelection);",
        "layoutViewerPanel->RefreshAfterSelectionOnlyUpdate();",
    ]:
        assert publication in selection, publication

    for forbidden in [
        "RefreshAfterSceneChange", "RefreshAfterToolSceneUpdate",
        "PersistFixtureTypeAutoColors", "ReloadData", "UpdateScene",
        "NotifySceneVisualContentChanged", "PushUndo", "CommitMutation",
        "MarkDirty", "SetDirty", "GetScene",
    ]:
        assert forbidden not in selection, (
            f"Selection-only publication must not perform {forbidden}"
        )
        if forbidden not in {"RefreshAfterToolSceneUpdate"}:
            assert forbidden not in active, (
                f"The publication adapter must not perform {forbidden}"
            )

    scene = function_body(
        "gui/mainwindow_selection_refresh.cpp",
        "void MainWindow::RefreshAfterToolSceneUpdate(\n",
    )
    assert re.search(
        r"RefreshAfterSceneChange\(\);\s*"
        r"RefreshAfterToolSelectionUpdate\(selection\);",
        scene,
    ), "Scene mutations must retain scene refresh before selection publication"

    live = function_body(
        "app/local_live_controller.cpp",
        "LocalLiveController::HandleOnMainThread(",
    )
    assert "ActiveProjectCommandContext activeProject(config);" in live
    assert "activeProject.Publish(window_, execution.mutation);" in live


if __name__ == "__main__":
    main()
