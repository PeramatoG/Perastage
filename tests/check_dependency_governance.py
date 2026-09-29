#!/usr/bin/env python3
"""Validate dependency policy coverage and deterministic generated artifacts."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts/dependencies"))
from bundled import nlohmann_notice, nlohmann_version  # noqa: E402
from checkout import require_checkout_head  # noqa: E402
from model import compare, direct_dependencies, load_json, markdown_report, resolve, version_label  # noqa: E402
from report_installed import status_versions  # noqa: E402
from review import validate_features  # noqa: E402

REQUIRED_POLICY_FIELDS = {
    "name", "provider", "maintenance-group", "update-mode", "expose-version", "rationale"
}
EXPECTED_GROUPS = {
    "wxwidgets": "feature-performance", "meshoptimizer": "feature-performance",
    "curl": "security-interop", "libxml2": "security-interop", "tinyxml2": "security-interop",
    "podofo": "security-interop", "zlib": "security-interop", "mdns": "security-interop",
    "glew": "stable-runtime", "nanovg": "stable-runtime", "backward-cpp": "stable-runtime",
    "gettext": "build-tooling",
}
EXPECTED_BUNDLED = {"nlohmann/json", "stb_easy_font", "Lucide selected SVG assets", "Noto Sans"}


def git_registry(source: Path, destination: Path) -> str:
    """Create a deterministic local Git checkout around registry fixture data."""
    shutil.copytree(source, destination)
    subprocess.run(["git", "init", "-q", str(destination)], check=True)
    subprocess.run(["git", "-C", str(destination), "config", "user.name", "Dependency Test"], check=True)
    subprocess.run(["git", "-C", str(destination), "config", "user.email", "dependency-test@example.invalid"], check=True)
    subprocess.run(["git", "-C", str(destination), "add", "."], check=True)
    commit_environment = dict(os.environ, GIT_AUTHOR_DATE="2000-01-01T00:00:00Z",
                              GIT_COMMITTER_DATE="2000-01-01T00:00:00Z")
    subprocess.run(["git", "-C", str(destination), "commit", "-q", "-m", "fixture"],
                   check=True, env=commit_environment)
    return subprocess.check_output(["git", "-C", str(destination), "rev-parse", "HEAD"], text=True).strip()


def tree_digest(root: Path) -> str:
    """Hash regular non-Git files to detect dry-run mutations."""
    digest = hashlib.sha256()
    for path in sorted(item for item in root.rglob("*") if item.is_file() and ".git" not in item.parts):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def validate_policy(manifest: dict, policy: dict, state: dict) -> None:
    """Enforce the complete dependency classification contract."""
    direct_items = direct_dependencies(manifest)
    direct_names = [item["name"] for item in direct_items]
    rules = policy["vcpkg"]
    rule_names = [item["name"] for item in rules]
    assert len(rule_names) == len(set(rule_names)), "duplicate vcpkg policy entry"
    assert set(rule_names) == set(direct_names), "vcpkg policy and manifest must have exact coverage"

    allowed_providers = {"vcpkg", "bundled", "system", "standards"}
    allowed_groups = {"feature-performance", "security-interop", "stable-runtime", "build-tooling",
                      "visual", "compatibility", "standards"}
    allowed_modes = {"automatic-candidate", "notify-only", "manual", "platform", "excluded"}
    for section in ("vcpkg", "bundled", "capabilities", "excluded"):
        for entry in policy[section]:
            assert REQUIRED_POLICY_FIELDS <= set(entry), f"incomplete {section} policy entry: {entry.get('name')}"
            assert entry["provider"] in allowed_providers
            assert entry["maintenance-group"] in allowed_groups
            assert entry["update-mode"] in allowed_modes
            assert isinstance(entry["rationale"], str) and entry["rationale"].strip()
    for entry in rules:
        assert {"quarterly-candidate", "required-features"} <= set(entry)
        assert entry["provider"] == "vcpkg"
        assert isinstance(entry["quarterly-candidate"], bool)
        assert isinstance(entry["required-features"], list)
    for entry in policy["bundled"]:
        assert {"quarterly-candidate", "manual-replacement"} <= set(entry)
        assert entry["provider"] == "bundled"

    by_name = {item["name"]: item for item in rules}
    assert {name: by_name[name]["maintenance-group"] for name in EXPECTED_GROUPS} == EXPECTED_GROUPS
    assert by_name["wxwidgets"]["update-mode"] == "automatic-candidate"
    assert by_name["wxwidgets"]["required-features"] == ["secretstore"]
    assert by_name["meshoptimizer"]["update-mode"] == "automatic-candidate"
    assert {item["name"] for item in policy["bundled"]} == EXPECTED_BUNDLED
    assert all(item.get("manual-replacement") is True for item in policy["bundled"])
    nlohmann = next(item for item in policy["bundled"] if item["name"] == "nlohmann/json")
    assert nlohmann["update-mode"] == "notify-only" and nlohmann["quarterly-candidate"] is True
    standards = policy["excluded"]
    assert any("MVR/GDTF" in item["name"] and item["update-mode"] == "excluded" for item in standards)

    state_names = [item["name"] for item in state["packages"]]
    assert len(state_names) == len(set(state_names)) and set(state_names) == set(direct_names)


def main() -> int:
    """Enforce policy, generated-state, reporting, and dry-run contracts offline."""
    manifest = load_json(ROOT / "vcpkg.json")
    policy = load_json(ROOT / "dependencies/dependency-policy.json")
    state = load_json(ROOT / "dependencies/resolved-vcpkg.json")
    badge = load_json(ROOT / ".github/badges/wxwidgets.json")
    validate_policy(manifest, policy, state)
    assert re.fullmatch(r"[0-9a-f]{40}", manifest["builtin-baseline"])
    assert state["baseline"] == manifest["builtin-baseline"]
    manifest_by_name = {item["name"]: item for item in direct_dependencies(manifest)}
    for rule in policy["vcpkg"]:
        assert rule["required-features"] == manifest_by_name[rule["name"]]["features"]
    wx = next(item for item in state["packages"] if item["name"] == "wxwidgets")
    assert wx["port-version"] == 1 and badge["message"] == wx["version"]
    assert "#" not in badge["message"]

    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "img.shields.io/endpoint" in readme and "%2F.github%2Fbadges%2Fwxwidgets.json" in readme
    exact_wx = re.compile(r"(?i)wxwidgets(?:\s+|[-_/])v?\d+\.\d+(?:\.\d+)?")
    maintained_docs = [ROOT / "README.md", *sorted((ROOT / "docs").rglob("*.md"))]
    maintained_docs = [path for path in maintained_docs if path.name not in {"release-notes-draft.md", "CHANGELOG.md"}]
    assert not [(path, exact_wx.findall(path.read_text(encoding="utf-8"))) for path in maintained_docs
                if exact_wx.search(path.read_text(encoding="utf-8"))]

    vendored = nlohmann_version(ROOT / "third_party/json.hpp")
    assert re.fullmatch(r"\d+\.\d+\.\d+", vendored)
    assert "newer upstream release available" in nlohmann_notice("3.11.2", "v3.12.0")
    assert "up to date" in nlohmann_notice(vendored, vendored)
    assert "lookup unavailable" in nlohmann_notice(vendored, None)

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
    assert expected[0]["license-changed"] is True and expected[0]["large-version-jump"] is True
    report = markdown_report(old, candidate, fixture_policy, True)
    assert report == markdown_report(old, candidate, fixture_policy, True)
    assert "Large version jumps: alpha." in report and "| alpha | feature-performance | 1.0 | 2.0 | trigger | yes |" in report
    assert validate_features(fixture_manifest, fixture_policy, fixture / "candidate") is True

    with tempfile.TemporaryDirectory() as temporary:
        status = Path(temporary) / "status"
        status.write_text(
            "Package: alpha\nVersion: 9.0\nArchitecture: arm64-test\n\n"
            "Package: alpha\nVersion: 1.0\nPort-Version: 2\nArchitecture: x64-test\n\n"
            "Package: alpha\nFeature: tool\nVersion: 99.0\nArchitecture: x64-test\n\n", encoding="utf-8")
        assert status_versions(status, "x64-test") == {"alpha": "1.0#2"}
        assert status_versions(status, "arm64-test") == {"alpha": "9.0"}
        assert status_versions(status, "missing-test") == {}
        state_path = Path(temporary) / "state.json"
        state_path.write_text(json.dumps(old), encoding="utf-8")
        strict = subprocess.run(
            [sys.executable, str(ROOT / "scripts/dependencies/report_installed.py"),
             "--state", str(state_path), "--status", str(status), "--triplet", "missing-test", "--strict"],
            capture_output=True, text=True)
        assert strict.returncode != 0 and "missing direct packages for triplet missing-test" in strict.stderr

    with tempfile.TemporaryDirectory() as temporary:
        incomplete_registry = Path(temporary) / "candidate"
        shutil.copytree(fixture / "candidate", incomplete_registry)
        beta_port = load_json(incomplete_registry / "ports/beta/vcpkg.json")
        beta_port["features"] = {}
        (incomplete_registry / "ports/beta/vcpkg.json").write_text(json.dumps(beta_port), encoding="utf-8")
        assert validate_features(fixture_manifest, fixture_policy, incomplete_registry) is False

    try:
        resolve(fixture_manifest, {"vcpkg": []}, fixture / "old")
        raise AssertionError("missing policy coverage was accepted")
    except ValueError as error:
        assert "policy coverage mismatch" in str(error)
    with tempfile.TemporaryDirectory() as temporary:
        malformed = Path(temporary) / "invalid.json"
        malformed.write_text("[", encoding="utf-8")
        try:
            load_json(malformed)
            raise AssertionError("malformed state was accepted")
        except ValueError as error:
            assert str(malformed) in str(error)

    with tempfile.TemporaryDirectory() as temporary:
        temporary_root = Path(temporary)
        current_registry = temporary_root / "current"
        candidate_registry = temporary_root / "candidate"
        current_sha = git_registry(fixture / "old", current_registry)
        candidate_sha = git_registry(fixture / "candidate", candidate_registry)
        project = temporary_root / "project"
        (project / "dependencies").mkdir(parents=True)
        project_manifest = dict(fixture_manifest, **{"builtin-baseline": current_sha})
        (project / "vcpkg.json").write_text(json.dumps(project_manifest), encoding="utf-8")
        shutil.copy2(fixture / "dependencies/dependency-policy.json", project / "dependencies/dependency-policy.json")
        before = tree_digest(project)
        review = subprocess.run(
            [sys.executable, str(ROOT / "scripts/dependencies/review.py"), "--repo", str(project),
             "--current-vcpkg", str(current_registry), "--candidate-vcpkg", str(candidate_registry),
             "--candidate-baseline", candidate_sha, "--report", str(temporary_root / "report.md")],
            check=True, capture_output=True, text=True)
        assert "trigger" in review.stdout and tree_digest(project) == before
        mismatch = subprocess.run(
            [sys.executable, str(ROOT / "scripts/dependencies/review.py"), "--repo", str(project),
             "--current-vcpkg", str(current_registry), "--candidate-vcpkg", str(candidate_registry),
             "--candidate-baseline", "0" * 40], capture_output=True, text=True)
        assert mismatch.returncode != 0 and "candidate vcpkg checkout HEAD" in mismatch.stderr
        try:
            require_checkout_head(current_registry, "0" * 40, "current")
            raise AssertionError("checkout identity mismatch was accepted")
        except ValueError as error:
            assert "Check out the expected commit and retry" in str(error)

    print("Dependency governance checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
