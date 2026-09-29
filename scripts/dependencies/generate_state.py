#!/usr/bin/env python3
"""Generate canonical vcpkg state and display metadata from a fixed checkout."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from model import load_json, resolve, version_label


def write_json(path: Path, value: dict) -> None:
    """Write stable, reviewable JSON."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    """Validate checkout identity and regenerate repository-owned artifacts."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--vcpkg", type=Path, required=True)
    args = parser.parse_args()
    manifest = load_json(args.repo / "vcpkg.json")
    head = subprocess.run(["git", "-C", str(args.vcpkg), "rev-parse", "HEAD"], check=True,
                          text=True, capture_output=True).stdout.strip()
    if head != manifest["builtin-baseline"]:
        raise SystemExit(f"vcpkg checkout is {head}; expected {manifest['builtin-baseline']}")
    state = resolve(manifest, load_json(args.repo / "dependencies/dependency-policy.json"), args.vcpkg)
    write_json(args.repo / "dependencies/resolved-vcpkg.json", state)
    wx = next(item for item in state["packages"] if item["name"] == "wxwidgets")
    write_json(args.repo / ".github/badges/wxwidgets.json",
               {"schemaVersion": 1, "label": "wxWidgets", "message": version_label(wx), "color": "green"})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
