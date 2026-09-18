#!/usr/bin/env python3
"""Protect read-only Legend refreshes from mutating Layout Viewer selection."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LEGEND_SOURCE = ROOT / "gui" / "layoutviewerpanel_legend.cpp"
GETTER_SOURCES = {
    "layouts::Layout2DViewDefinition *LayoutViewerPanel::GetEditableView()":
        ROOT / "gui" / "layoutviewerpanel_view.cpp",
    "layouts::LayoutLegendDefinition *LayoutViewerPanel::GetSelectedLegend()":
        LEGEND_SOURCE,
    "LayoutViewerPanel::GetSelectedEventTable()":
        ROOT / "gui" / "layoutviewerpanel_eventtable.cpp",
    "layouts::LayoutTextDefinition *LayoutViewerPanel::GetSelectedText()":
        ROOT / "gui" / "layoutviewerpanel_text.cpp",
    "layouts::LayoutImageDefinition *LayoutViewerPanel::GetSelectedImage()":
        ROOT / "gui" / "layoutviewerpanel_image.cpp",
}


def function_body(source: str, signature: str) -> str:
    """Return a C++ function body by balancing braces from its signature."""
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing function: {signature}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"Missing function body: {signature}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"Unterminated function body: {signature}")


def main() -> None:
    """Verify read/query paths cannot mutate Layout Viewer selection."""
    for signature, path in GETTER_SOURCES.items():
        getter_body = function_body(path.read_text(encoding="utf-8"), signature)
        assert "selectionState_.Select(" not in getter_body, signature
        assert "selectionState_.Clear(" not in getter_body, signature

    source = LEGEND_SOURCE.read_text(encoding="utf-8")
    body = function_body(source, "void LayoutViewerPanel::RefreshLegendData()")

    const_lookup = (
        "static_cast<const LayoutViewerPanel *>(this)->GetSelectedLegend()"
    )
    assert const_lookup in body, (
        "RefreshLegendData must resolve GetSelectedLegend through the const "
        "panel overload"
    )
    assert body.count("GetSelectedLegend()") == 1
    assert "BuildLegendItems(selectedLegend)" in body
    assert "HashLegendItems(items, selectedLegend)" in body
    assert "selectionState_.Select(" not in body
    assert "selectionState_.Clear(" not in body
    assert "interactionSession_" not in body


if __name__ == "__main__":
    main()
