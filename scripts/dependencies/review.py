#!/usr/bin/env python3
"""Compare fixed vcpkg checkouts and optionally prepare candidate artifacts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_state import write_json
from model import compare, load_json, markdown_report, resolve, version_label


def validate_features(manifest: dict, policy: dict, registry: Path) -> bool:
    """Check policy features against manifest requests and candidate port metadata."""
    manifest_items = {item["name"]: item for item in __import__('model').direct_dependencies(manifest)}
    for rule in policy["vcpkg"]:
        requested = rule["required-features"]
        if requested != manifest_items[rule["name"]]["features"]:
            return False
        available = load_json(registry / "ports" / rule["name"] / "vcpkg.json").get("features", {})
        if any(feature not in available for feature in requested):
            return False
    return True


def main() -> int:
    """Print an offline-safe review and mutate only explicit generated files with --apply."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--current-vcpkg", type=Path, required=True)
    parser.add_argument("--candidate-vcpkg", type=Path, required=True)
    parser.add_argument("--candidate-baseline", required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--bundled-notice", default="Notify-only checks were not requested.")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    manifest = load_json(args.repo / "vcpkg.json")
    policy = load_json(args.repo / "dependencies/dependency-policy.json")
    old = resolve(manifest, policy, args.current_vcpkg)
    candidate_manifest = dict(manifest)
    candidate_manifest["builtin-baseline"] = args.candidate_baseline
    candidate = resolve(candidate_manifest, policy, args.candidate_vcpkg)
    feature_ok = validate_features(manifest, policy, args.candidate_vcpkg)
    report = markdown_report(old, candidate, policy, feature_ok, args.bundled_notice)
    print(report, end="")
    if args.report:
        args.report.write_text(report, encoding="utf-8")
    changes = compare(old, candidate, policy)
    if args.apply and any(row["reason"] == "trigger" for row in changes):
        if not feature_ok:
            raise SystemExit("candidate port metadata lacks a required manifest feature")
        (args.repo / "vcpkg.json").write_text(json.dumps(candidate_manifest, indent=2) + "\n", encoding="utf-8")
        write_json(args.repo / "dependencies/resolved-vcpkg.json", candidate)
        wx = next(item for item in candidate["packages"] if item["name"] == "wxwidgets")
        write_json(args.repo / ".github/badges/wxwidgets.json",
                   {"schemaVersion": 1, "label": "wxWidgets", "message": version_label(wx), "color": "green"})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
