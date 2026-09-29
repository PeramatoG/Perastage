#!/usr/bin/env python3
"""Enforce the quarterly dependency-review workflow safety contract offline."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/dependency-review.yml"


def main() -> int:
    """Reject unsafe triggers, mutations, permissions, and PR behavior."""
    text = WORKFLOW.read_text(encoding="utf-8")
    assert re.search(r"(?m)^\s*schedule:\s*$", text)
    assert re.search(r"cron:\s*[\"']?[^\n]*1,4,7,10", text)
    assert re.search(r"(?m)^\s*workflow_dispatch:\s*$", text)
    assert not re.search(r"(?m)^\s*pull_request(?:_target)?:\s*$", text)
    assert re.search(r"(?m)^concurrency:\s*$", text)
    assert "contents: write" in text and "pull-requests: write" in text
    assert text.index("permissions:") > text.index("review:")
    assert "auto-merge" not in text.casefold()
    assert "gh pr create --draft" in text
    assert "get_vcpkg_baseline.py vcpkg.json" in text
    assert "scripts/dependencies/review.py" in text
    assert "bootstrap-vcpkg" not in text and not re.search(r"\bvcpkg(?:\.exe)?\s+install\b", text)

    add_lines = re.findall(r"(?m)^\s*git add (.+)$", text)
    assert add_lines == ["vcpkg.json dependencies/resolved-vcpkg.json .github/badges/wxwidgets.json"]
    for forbidden in ("third_party/", "resources/", "library/fixtures/", "library/trusses/"):
        assert forbidden not in add_lines[0]
    assert "unexpected=" in text and "dependencies/resolved-vcpkg.json" in text
    assert "--body-file \"$RUNNER_TEMP/dependency-review.md\"" in text

    report_code = (ROOT / "scripts/dependencies/model.py").read_text(encoding="utf-8")
    assert "must not be auto-merged" in report_code
    assert "Full CI must pass" in report_code and "human must review" in report_code
    print("Dependency review workflow safety checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
