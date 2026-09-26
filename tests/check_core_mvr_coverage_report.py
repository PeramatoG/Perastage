#!/usr/bin/env python3
"""Verify deterministic Core/MVR coverage-scope aggregation and rendering."""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / ".github" / "scripts" / "generate_core_mvr_coverage.py"
SPEC = importlib.util.spec_from_file_location("core_mvr_coverage", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def file_entry(name: str, counts: list[int]) -> dict:
    return {
        "file": name,
        "lines": [{"count": count} for count in counts],
    }


def main() -> None:
    report = {
        "files": [
            file_entry("core/inspection/gdtf_inspection.cpp", [1, 0, 2]),
            file_entry("core/project_service.cpp", [0, 3]),
            file_entry("mvr/mvr_read.cpp", [4, 0, 0, 1]),
        ]
    }
    scopes = MODULE.coverage_scopes(report)
    assert scopes["inspection"] == (2, 3)
    assert scopes["core"] == (3, 5)
    assert scopes["mvr"] == (2, 4)
    assert scopes["combined"] == (5, 9)

    summary = MODULE.render_summary(report, "synthetic")
    assert "| Inspection (`core/inspection/`) | 2 / 3 | 66.67% |" in summary
    assert "| Combined | 5 / 9 | 55.56% |" in summary
    assert "no percentage threshold is enforced" in summary

    empty_report = {"files": [file_entry("mvr/mvr_read.cpp", [1])]}
    empty_summary = MODULE.render_summary(empty_report, "empty-inspection")
    assert "| Inspection (`core/inspection/`) | 0 / 0 | n/a |" in empty_summary


if __name__ == "__main__":
    main()
