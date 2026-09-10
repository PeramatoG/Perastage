#!/usr/bin/env python3
"""Enforce the tracked C/C++ source-size baseline and hotspot ratchet."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any


DEFAULT_POLICY = Path(__file__).with_name("source_file_size_policy.json")


def load_policy(path: Path) -> dict[str, Any]:
    """Load and validate the source-size policy document."""
    try:
        policy = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read valid JSON from {path}: {error}") from error
    required = {"schema_version", "default_max_lines", "source_suffixes", "excluded_prefixes", "hotspots"}
    if not isinstance(policy, dict) or set(policy) != required:
        raise ValueError(f"{path} must contain exactly these keys: {sorted(required)}")
    if policy["schema_version"] != 1:
        raise ValueError(f"{path} has unsupported schema_version {policy['schema_version']!r}")
    if not isinstance(policy["default_max_lines"], int) or isinstance(policy["default_max_lines"], bool) or policy["default_max_lines"] < 1:
        raise ValueError(f"{path}: default_max_lines must be a positive integer")
    suffixes = policy["source_suffixes"]
    if not isinstance(suffixes, list) or not suffixes or any(not isinstance(item, str) or not item.startswith(".") for item in suffixes):
        raise ValueError(f"{path}: source_suffixes must be a non-empty list of extensions")
    prefixes = policy["excluded_prefixes"]
    if not isinstance(prefixes, list) or any(not isinstance(item, str) or not item.endswith("/") for item in prefixes):
        raise ValueError(f"{path}: excluded_prefixes must contain directory prefixes ending in '/'")
    hotspots = policy["hotspots"]
    if not isinstance(hotspots, dict):
        raise ValueError(f"{path}: hotspots must be an object mapping paths to line limits")
    for relative, maximum in hotspots.items():
        normalized = PurePosixPath(relative)
        if normalized.is_absolute() or ".." in normalized.parts or "\\" in relative or relative != normalized.as_posix():
            raise ValueError(f"{path}: hotspot path {relative!r} must be a normalized repository-relative path")
        if normalized.suffix.lower() not in suffixes:
            raise ValueError(f"{path}: hotspot {relative!r} is not a configured C/C++ source file")
        if not isinstance(maximum, int) or isinstance(maximum, bool) or maximum <= policy["default_max_lines"]:
            raise ValueError(f"{path}: hotspot {relative!r} must have an integer limit above default_max_lines")
    return policy


def tracked_files(root: Path, manifest: Path | None = None) -> list[str]:
    """Return deterministic repository-relative tracked paths."""
    if manifest is not None:
        return sorted({line.strip().replace("\\", "/") for line in manifest.read_text(encoding="utf-8").splitlines() if line.strip()})
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"], check=False, capture_output=True
    )
    if result.returncode != 0:
        diagnostic = result.stderr.decode(errors="replace").strip()
        raise OSError(f"cannot enumerate tracked files with git ls-files: {diagnostic}")
    return sorted(path.decode(errors="surrogateescape") for path in result.stdout.split(b"\0") if path)


def is_project_source(relative: str, policy: dict[str, Any]) -> bool:
    """Identify tracked project C/C++ files covered by the policy."""
    normalized = relative.replace("\\", "/")
    return PurePosixPath(normalized).suffix.lower() in policy["source_suffixes"] and not any(
        normalized.startswith(prefix) for prefix in policy["excluded_prefixes"]
    )


def physical_line_count(path: Path) -> int:
    """Count physical lines, including a final line without a newline."""
    content = path.read_bytes()
    return content.count(b"\n") + (1 if content and not content.endswith(b"\n") else 0)


def audit(root: Path, files: list[str], policy: dict[str, Any]) -> tuple[list[str], int]:
    """Return source-size violations and the number of inspected files."""
    errors: list[str] = []
    inspected: set[str] = set()
    hotspots = policy["hotspots"]
    for relative in files:
        normalized = relative.replace("\\", "/")
        if not is_project_source(normalized, policy):
            continue
        path = root / PurePosixPath(normalized)
        if not path.is_file():
            errors.append(f"tracked source file is missing from the worktree: {normalized}")
            continue
        inspected.add(normalized)
        line_count = physical_line_count(path)
        maximum = hotspots.get(normalized, policy["default_max_lines"])
        if line_count > maximum:
            rule = "hotspot baseline" if normalized in hotspots else "default limit"
            errors.append(
                f"{normalized}: {line_count} physical lines exceeds its {rule} of {maximum}; "
                "extract responsibility or explicitly review and update tests/source_file_size_policy.json"
            )
    for relative in sorted(set(hotspots) - inspected):
        errors.append(f"hotspot baseline references a missing, excluded, or untracked source file: {relative}; remove or correct the stale entry")
    return errors, len(inspected)


def main(argv: list[str] | None = None) -> int:
    """Run the source-size policy check from any working directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--tracked-manifest", type=Path, help="newline-separated tracked paths for isolated tests")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        policy = load_policy(args.policy.resolve())
        files = tracked_files(root, args.tracked_manifest.resolve() if args.tracked_manifest else None)
        errors, inspected = audit(root, files, policy)
    except (OSError, ValueError) as error:
        print(f"Source-size policy error: {error}", file=sys.stderr)
        return 2
    if errors:
        print("Source-size policy violations:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Source-size policy passed: inspected {inspected} tracked project C/C++ files (default maximum {policy['default_max_lines']} lines; {len(policy['hotspots'])} hotspot baselines).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
