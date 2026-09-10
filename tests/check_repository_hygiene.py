#!/usr/bin/env python3
"""Reject prohibited artifacts and unexpectedly large files tracked by Git."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any


DEFAULT_POLICY = Path(__file__).with_name("repository_hygiene_policy.json")
POLICY_KEYS = {
    "schema_version", "default_max_bytes", "prohibited_suffixes",
    "prohibited_filename_patterns", "prohibited_path_components",
    "restricted_asset_suffixes", "asset_classes", "exact_exceptions",
}
ASSET_KEYS = {"name", "path_prefix", "suffixes", "max_bytes"}


def _positive_integer(value: object, field: str) -> int:
    """Validate and return a positive byte limit."""
    if not isinstance(value, int) or isinstance(value, bool) or value < 1:
        raise ValueError(f"{field} must be a positive integer")
    return value


def _normalized_path(value: object, field: str, *, prefix: bool = False) -> str:
    """Validate a normalized repository-relative path or directory prefix."""
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a non-empty string")
    candidate = value[:-1] if prefix and value.endswith("/") else value
    path = PurePosixPath(candidate)
    if (prefix and not value.endswith("/")) or path.is_absolute() or ".." in path.parts or "\\" in value or path.as_posix() != candidate:
        kind = "directory prefix ending in '/'" if prefix else "path"
        raise ValueError(f"{field} must be a normalized repository-relative {kind}")
    return value


def _suffixes(value: object, field: str) -> list[str]:
    """Validate a unique, lowercase extension list."""
    if not isinstance(value, list) or not value:
        raise ValueError(f"{field} must be a non-empty list")
    if any(not isinstance(item, str) or not re.fullmatch(r"\.[a-z0-9]+", item) for item in value):
        raise ValueError(f"{field} must contain lowercase extensions beginning with '.'")
    if len(value) != len(set(value)):
        raise ValueError(f"{field} contains duplicate extensions")
    return value


def load_policy(path: Path) -> dict[str, Any]:
    """Load and strictly validate the versioned hygiene policy."""
    try:
        policy = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read valid JSON from {path}: {error}") from error
    if not isinstance(policy, dict) or set(policy) != POLICY_KEYS:
        raise ValueError(f"{path} must contain exactly these keys: {sorted(POLICY_KEYS)}")
    if policy["schema_version"] != 1:
        raise ValueError(f"{path} has unsupported schema_version {policy['schema_version']!r}")
    _positive_integer(policy["default_max_bytes"], "default_max_bytes")
    _suffixes(policy["prohibited_suffixes"], "prohibited_suffixes")

    patterns = policy["prohibited_filename_patterns"]
    if not isinstance(patterns, list) or not patterns or any(not isinstance(item, str) or not item for item in patterns):
        raise ValueError("prohibited_filename_patterns must be a non-empty string list")
    if len(patterns) != len(set(patterns)):
        raise ValueError("prohibited_filename_patterns contains duplicates")
    try:
        policy["_compiled_patterns"] = [re.compile(item, re.IGNORECASE) for item in patterns]
    except re.error as error:
        raise ValueError(f"invalid prohibited filename pattern: {error}") from error

    components = policy["prohibited_path_components"]
    if not isinstance(components, list) or not components or any(not isinstance(item, str) or not item or "/" in item or "\\" in item or item != item.lower() for item in components):
        raise ValueError("prohibited_path_components must contain unique lowercase path components")
    if len(components) != len(set(components)):
        raise ValueError("prohibited_path_components contains duplicates")
    restricted_suffixes = _suffixes(policy["restricted_asset_suffixes"], "restricted_asset_suffixes")
    overlap = set(restricted_suffixes) & set(policy["prohibited_suffixes"])
    if overlap:
        raise ValueError(f"suffixes cannot be both prohibited and restricted assets: {sorted(overlap)}")

    classes = policy["asset_classes"]
    if not isinstance(classes, list) or not classes:
        raise ValueError("asset_classes must be a non-empty list")
    seen_names: set[str] = set()
    seen_scopes: set[tuple[str, str]] = set()
    for index, rule in enumerate(classes):
        field = f"asset_classes[{index}]"
        if not isinstance(rule, dict) or set(rule) != ASSET_KEYS:
            raise ValueError(f"{field} must contain exactly these keys: {sorted(ASSET_KEYS)}")
        if not isinstance(rule["name"], str) or not rule["name"] or rule["name"] in seen_names:
            raise ValueError(f"{field}.name must be non-empty and unique")
        seen_names.add(rule["name"])
        _normalized_path(rule["path_prefix"], f"{field}.path_prefix", prefix=True)
        _positive_integer(rule["max_bytes"], f"{field}.max_bytes")
        for suffix in _suffixes(rule["suffixes"], f"{field}.suffixes"):
            scope = (rule["path_prefix"], suffix)
            if scope in seen_scopes:
                raise ValueError(f"duplicate asset-class scope for {scope[0]}*{scope[1]}")
            seen_scopes.add(scope)

    exceptions = policy["exact_exceptions"]
    if not isinstance(exceptions, dict):
        raise ValueError("exact_exceptions must map paths to positive byte limits")
    for relative, maximum in exceptions.items():
        _normalized_path(relative, "exact exception path")
        _positive_integer(maximum, f"exact exception {relative!r}")
    return policy


def tracked_files(root: Path, manifest: Path | None = None) -> list[str]:
    """Return deterministic repository-relative tracked paths."""
    if manifest is not None:
        paths = [line.strip() for line in manifest.read_text(encoding="utf-8").splitlines() if line.strip()]
        for relative in paths:
            _normalized_path(relative, "tracked manifest entry")
        return sorted(set(paths))
    result = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], check=False, capture_output=True)
    if result.returncode != 0:
        diagnostic = result.stderr.decode(errors="replace").strip()
        raise OSError(f"cannot enumerate tracked files with git ls-files: {diagnostic}")
    return sorted(path.decode(errors="surrogateescape") for path in result.stdout.split(b"\0") if path)


def _asset_rule(relative: str, policy: dict[str, Any]) -> dict[str, Any] | None:
    """Return the single narrowly scoped asset rule matching a path."""
    suffix = PurePosixPath(relative).suffix.lower()
    return next((rule for rule in policy["asset_classes"] if relative.startswith(rule["path_prefix"]) and suffix in rule["suffixes"]), None)


def audit(root: Path, files: list[str], policy: dict[str, Any]) -> tuple[list[str], int]:
    """Return hygiene violations and the number of tracked files inspected."""
    errors: list[str] = []
    inspected: set[str] = set()
    exceptions = policy["exact_exceptions"]
    prohibited_suffixes = set(policy["prohibited_suffixes"])
    restricted_suffixes = set(policy["restricted_asset_suffixes"])
    components = set(policy["prohibited_path_components"])
    for relative in files:
        path = root / PurePosixPath(relative)
        if not path.is_file():
            errors.append(f"tracked file is missing from the worktree: {relative}")
            continue
        inspected.add(relative)
        name = PurePosixPath(relative).name
        suffix = PurePosixPath(relative).suffix.lower()
        lowered_parts = {part.lower() for part in PurePosixPath(relative).parts[:-1]}
        reason = None
        rule = _asset_rule(relative, policy)
        if suffix in prohibited_suffixes:
            reason = f"prohibited artifact extension {suffix}"
        elif suffix in restricted_suffixes and rule is None:
            reason = f"intentional asset extension {suffix} is outside its approved path scope"
        elif any(pattern.fullmatch(name) for pattern in policy["_compiled_patterns"]):
            reason = "generated build filename"
        elif lowered_parts & components:
            reason = f"prohibited build/cache path component {sorted(lowered_parts & components)[0]!r}"
        if reason:
            errors.append(f"{relative}: tracked repository artifact is prohibited ({reason}); remove it from Git and keep generated output untracked")
            continue

        maximum = exceptions.get(relative, rule["max_bytes"] if rule else policy["default_max_bytes"])
        size = path.stat().st_size
        if size > maximum:
            scope = "exact exception" if relative in exceptions else rule["name"] if rule else "default tracked-file limit"
            errors.append(f"{relative}: {size} bytes exceeds the {scope} maximum of {maximum} bytes; reduce the file or review an explicit narrow policy change")
    for relative in sorted(set(exceptions) - inspected):
        errors.append(f"stale exact exception references a missing or untracked file: {relative}; remove or correct the policy entry")
    return errors, len(inspected)


def main(argv: list[str] | None = None) -> int:
    """Run the tracked-file hygiene check from any working directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--tracked-manifest", type=Path)
    args = parser.parse_args(argv)
    try:
        policy = load_policy(args.policy.resolve())
        files = tracked_files(args.root.resolve(), args.tracked_manifest.resolve() if args.tracked_manifest else None)
        errors, inspected = audit(args.root.resolve(), files, policy)
    except (OSError, ValueError) as error:
        print(f"Repository hygiene policy error: {error}", file=sys.stderr)
        return 2
    if errors:
        print("Repository hygiene policy violations:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Repository hygiene policy passed: inspected {inspected} tracked files (default maximum {policy['default_max_bytes']} bytes; {len(policy['asset_classes'])} asset classes; {len(policy['exact_exceptions'])} exact exceptions).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
