#!/usr/bin/env python3
"""Validate the recorded ORG-001 repository structure baseline."""

from __future__ import annotations

import argparse
from collections import Counter
import json
import re
import subprocess
import sys
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
MACHINE_PATH_PATTERN = re.compile(
    r"(?<![A-Za-z])(?:[A-Za-z]:[/\\](?:[^\s\"']+)|/(?:home|Users)/[^\s\"']+)"
)
SOURCE_GLOB_PATTERN = re.compile(
    r"file\s*\(\s*GLOB(?:_RECURSE)?\b(?P<body>.*?)\)", re.IGNORECASE | re.DOTALL
)
SOURCE_GLOB_SUFFIX_PATTERN = re.compile(
    r"(?:\*|\[[^]]+\])\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx)\b", re.IGNORECASE
)


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


def tracked_files(root: Path, manifest: Path | None = None) -> set[str]:
    """Return normalized repository-controlled paths without consulting a branch."""
    if manifest is not None:
        return {
            line.strip().replace("\\", "/")
            for line in manifest.read_text(encoding="utf-8").splitlines()
            if line.strip()
        }
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        diagnostic = result.stderr.decode(errors="replace").strip()
        raise OSError(f"cannot enumerate tracked files with git ls-files: {diagnostic}")
    return {path.decode(errors="surrogateescape") for path in result.stdout.split(b"\0") if path}


def is_machine_path_configuration(relative: str, policy: dict) -> bool:
    """Identify tracked shared build/development configuration covered by the path guard."""
    if relative in policy["root_files"]:
        return True
    return any(relative.startswith(prefix) for prefix in policy["directory_prefixes"]) and any(
        relative.lower().endswith(suffix) for suffix in policy["file_suffixes"]
    )


def audit_machine_paths(root: Path, files: set[str], policy: dict) -> list[str]:
    """Reject suspicious absolute paths beyond the exact transitional allowance."""
    allowed = Counter(
        (item["file"], item["value"])
        for item in policy["grandfathered_occurrences"]
        for _ in range(item["count"])
    )
    found: Counter[tuple[str, str]] = Counter()
    locations: dict[tuple[str, str], list[int]] = {}
    for relative in sorted(files):
        if not is_machine_path_configuration(relative, policy):
            continue
        path = root / relative
        if not path.is_file():
            continue
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for match in MACHINE_PATH_PATTERN.finditer(line):
                key = (relative, match.group(0).rstrip(",;)"))
                found[key] += 1
                locations.setdefault(key, []).append(line_number)
    errors = []
    for (relative, value), count in sorted((found - allowed).items()):
        line_numbers = locations[(relative, value)][-count:]
        errors.append(
            f"{relative}:{line_numbers[0]} contains machine-specific absolute path {value!r} "
            f"({count} unapproved occurrence(s)); use a project/environment variable or update "
            "the narrow transitional baseline during an intentional migration"
        )
    return errors


def audit_source_discovery(root: Path, files: set[str]) -> list[str]:
    """Reject CMake globs that discover production C/C++ sources."""
    errors = []
    cmake_files = sorted(
        relative
        for relative in files
        if relative.endswith("CMakeLists.txt") or relative.lower().endswith(".cmake")
    )
    for relative in cmake_files:
        path = root / relative
        if not path.is_file():
            continue
        cmake = re.sub(r"(?m)#.*$", "", path.read_text(encoding="utf-8"))
        for match in SOURCE_GLOB_PATTERN.finditer(cmake):
            construct = match.group(0)
            if SOURCE_GLOB_SUFFIX_PATTERN.search(match.group("body")):
                line_number = cmake.count("\n", 0, match.start()) + 1
                compact = " ".join(construct.split())
                errors.append(
                    f"{relative}:{line_number} uses forbidden production-source discovery "
                    f"{compact!r}; register C/C++ sources explicitly in their owning CMake target"
                )
    return errors


def audit_repository(root: Path, baseline: dict, files: set[str]) -> list[str]:
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
        relative for relative in files if "/" not in relative and Path(relative).suffix.lower() in SOURCE_SUFFIXES
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
        executable_match = re.search(r"add_executable\s*\(([^)]*)\)", cmake, re.DOTALL)
        executable_sources = executable_match.group(1) if executable_match else ""
        for entry_point in baseline["source_registration"]["root_compiled_entry_points"]:
            if not re.search(rf"\b{re.escape(entry_point)}\b", executable_sources):
                errors.append(f"root compiled entry point is not registered by add_executable: {entry_point}")
        for group in baseline["source_registration"]["root_registered_source_groups"]:
            if not re.search(rf"(^|\s){re.escape(group)}/", executable_sources):
                errors.append(f"root source group is not registered by add_executable: {group}/")

    guard = baseline["structural_guard"]
    errors.extend(audit_machine_paths(root, files, guard["machine_path_scan"]))
    errors.extend(audit_source_discovery(root, files))

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
    parser.add_argument(
        "--tracked-files-from",
        type=Path,
        help="newline-delimited tracked paths for deterministic non-Git fixtures",
    )
    args = parser.parse_args()

    try:
        baseline = load_baseline(args.baseline.resolve())
        root = args.repo_root.resolve()
        files = tracked_files(root, args.tracked_files_from)
        errors = audit_repository(root, baseline, files)
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
