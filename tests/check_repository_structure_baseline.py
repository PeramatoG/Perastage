#!/usr/bin/env python3
"""Validate the recorded ORG-001 repository structure baseline."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


def load_baseline(path: Path) -> dict:
    """Load the JSON baseline and reject malformed top-level data."""
    with path.open(encoding="utf-8") as stream:
        baseline = json.load(stream)
    if baseline.get("schema_version") != 1:
        raise ValueError(f"unsupported schema_version in {path}")
    return baseline


def flatten_groups(groups: dict[str, list[str]]) -> list[str]:
    """Flatten categorized path lists while retaining deterministic order."""
    return [path for paths in groups.values() for path in paths]


def audit_repository(root: Path, baseline: dict) -> list[str]:
    """Return actionable violations of the repository structure baseline."""
    errors: list[str] = []
    required_directories = flatten_groups(baseline["top_level_directories"])
    required_files = flatten_groups(baseline["root_file_roles"])
    entry_points = flatten_groups(baseline["development_entry_points"])

    for relative in required_directories:
        if not (root / relative).is_dir():
            errors.append(f"required top-level directory is missing: {relative}/")
    for relative in required_files + entry_points:
        if not (root / relative).is_file():
            errors.append(f"required repository entry point is missing: {relative}")

    allowed_root_sources = set(baseline["source_registration"]["root_project_sources"])
    actual_root_sources = {
        path.name
        for path in root.iterdir()
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
    }
    for relative in sorted(actual_root_sources - allowed_root_sources):
        errors.append(
            f"unexpected root project source: {relative}; root-level C/C++ files must be "
            "recorded architectural entry points in repository_structure_baseline.json"
        )
    for relative in sorted(allowed_root_sources - actual_root_sources):
        errors.append(f"recorded root project source is missing: {relative}")

    root_cmake_path = root / "CMakeLists.txt"
    if root_cmake_path.is_file():
        cmake = root_cmake_path.read_text(encoding="utf-8")
        actual_subdirectories = set(re.findall(r"add_subdirectory\s*\(\s*([^\s\)]+)", cmake))
        expected_subdirectories = set(baseline["source_registration"]["module_cmake_directories"])
        expected_subdirectories.update(baseline["source_registration"]["conditional_subdirectories"])
        if actual_subdirectories != expected_subdirectories:
            errors.append(
                "root add_subdirectory registrations differ from the baseline: "
                f"expected {sorted(expected_subdirectories)}, found {sorted(actual_subdirectories)}"
            )
        for module in baseline["source_registration"]["module_cmake_directories"]:
            module_cmake = root / module / "CMakeLists.txt"
            if not module_cmake.is_file():
                errors.append(f"module source-registration file is missing: {module}/CMakeLists.txt")
            elif re.search(
                r"file\s*\(\s*GLOB_RECURSE\b",
                module_cmake.read_text(encoding="utf-8"),
                re.IGNORECASE,
            ):
                errors.append(f"{module}/CMakeLists.txt uses forbidden recursive project-source discovery")
        executable_match = re.search(r"add_executable\s*\(([^)]*)\)", cmake, re.DOTALL)
        executable_sources = executable_match.group(1) if executable_match else ""
        for entry_point in baseline["source_registration"]["root_compiled_entry_points"]:
            if not re.search(rf"\b{re.escape(entry_point)}\b", executable_sources):
                errors.append(f"root compiled entry point is not registered by add_executable: {entry_point}")
        for group in baseline["source_registration"]["root_registered_source_groups"]:
            if not re.search(rf"(^|\s){re.escape(group)}/", executable_sources):
                errors.append(f"root source group is not registered by add_executable: {group}/")
        if re.search(r"file\s*\(\s*GLOB_RECURSE\b", cmake, re.IGNORECASE):
            errors.append("root CMakeLists.txt uses forbidden recursive project-source discovery")

    return errors


def main() -> int:
    """Parse command-line paths, run the audit, and report its result."""
    parser = argparse.ArgumentParser(description=__doc__)
    default_root = Path(__file__).resolve().parent.parent
    parser.add_argument("--repo-root", type=Path, default=default_root)
    parser.add_argument(
        "--baseline",
        type=Path,
        default=default_root / "docs/developer/repository_structure_baseline.json",
    )
    args = parser.parse_args()

    try:
        baseline = load_baseline(args.baseline.resolve())
        errors = audit_repository(args.repo_root.resolve(), baseline)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"ERROR: cannot audit repository structure: {error}", file=sys.stderr)
        return 2
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("OK: repository structure matches the recorded ORG-001 baseline.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
