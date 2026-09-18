#!/usr/bin/env python3
"""Protect read-only Legend refreshes from mutating Layout Viewer selection."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LEGEND_SOURCE = ROOT / "gui" / "layoutviewerpanel_legend.cpp"


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
    """Verify Legend semantic refresh uses the non-mutating const lookup path."""
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
    assert "selectedElementType =" not in body
    assert "selectedElementId =" not in body
    assert "interactionSession_" not in body


if __name__ == "__main__":
    main()
