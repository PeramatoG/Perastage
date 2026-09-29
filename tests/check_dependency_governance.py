#!/usr/bin/env python3
"""Validate dependency policy coverage and deterministic generated artifacts."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts/dependencies"))
from model import compare, direct_dependencies, load_json, markdown_report, resolve, version_label  # noqa: E402
from report_installed import status_versions  # noqa: E402


def main() -> int:
    """Enforce the complete offline governance contract."""
    manifest = load_json(ROOT / "vcpkg.json")
    policy = load_json(ROOT / "dependencies/dependency-policy.json")
    state = load_json(ROOT / "dependencies/resolved-vcpkg.json")
    badge = load_json(ROOT / ".github/badges/wxwidgets.json")
    direct = {item["name"]: item for item in direct_dependencies(manifest)}
    rules = policy["vcpkg"]
    assert len({item["name"] for item in rules}) == len(rules)
    assert set(direct) == {item["name"] for item in rules}
    assert re.fullmatch(r"[0-9a-f]{40}", manifest["builtin-baseline"])
    assert state["baseline"] == manifest["builtin-baseline"]
    assert {item["name"] for item in state["packages"]} == set(direct)
    for rule in rules:
        assert rule["required-features"] == direct[rule["name"]]["features"]
    assert direct["wxwidgets"]["features"] == ["secretstore"]
    wx = next(item for item in state["packages"] if item["name"] == "wxwidgets")
    assert badge["message"] == version_label(wx)
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "img.shields.io/endpoint" in readme and "%2F.github%2Fbadges%2Fwxwidgets.json" in readme
    assert len(policy["bundled"]) == 4
    docs = (ROOT / "docs/developer/build.md").read_text(encoding="utf-8")
    assert not re.search(r"wxWidgets\s+\d+\.\d+\.\d+", docs)

    fixture = ROOT / "tests/fixtures/dependencies"
    fixture_manifest = load_json(fixture / "vcpkg.json")
    fixture_policy = load_json(fixture / "dependencies/dependency-policy.json")
    old = resolve(fixture_manifest, fixture_policy, fixture / "old")
    candidate_manifest = dict(fixture_manifest, **{"builtin-baseline": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"})
    candidate = resolve(candidate_manifest, fixture_policy, fixture / "candidate")
    assert old == resolve(fixture_manifest, fixture_policy, fixture / "old")
    expected = compare(old, candidate, fixture_policy)
    assert expected == compare(old, candidate, fixture_policy)
    assert [item["reason"] for item in expected] == ["trigger", "collateral"]
    assert expected[0]["license-changed"] is True
    assert markdown_report(old, candidate, fixture_policy, True) == markdown_report(old, candidate, fixture_policy, True)
    with tempfile.TemporaryDirectory() as temporary:
        status = Path(temporary) / "status"
        status.write_text("Package: alpha\nVersion: 1.0\nPort-Version: 2\nArchitecture: x64-test\n\n", encoding="utf-8")
        assert status_versions(status) == {"alpha": "1.0#2"}

    before = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT)
    with tempfile.TemporaryDirectory() as temporary:
        report = Path(temporary) / "report.md"
        subprocess.run([sys.executable, str(ROOT / "scripts/dependencies/review.py"), "--repo", str(fixture),
                        "--current-vcpkg", str(fixture / "old"), "--candidate-vcpkg", str(fixture / "candidate"),
                        "--candidate-baseline", candidate_manifest["builtin-baseline"], "--report", str(report)], check=True,
                       capture_output=True, text=True)
        assert "| alpha | feature-performance | 1.0 | 2.0 | trigger | yes |" in report.read_text(encoding="utf-8")
    after = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT)
    assert hashlib.sha256(before).digest() == hashlib.sha256(after).digest()
    print("Dependency governance checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
