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
    r"(?<![A-Za-z])(?:[A-Za-z]:[/\\](?:[^\s\"']+)|/(?:home|Users)/[^\s\"']+|"
    r"/mnt/[A-Za-z]/(?:Users|home)/[^\s\"']+)"
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
    """Require suspicious absolute paths to match the exact transitional state."""
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
    for (relative, value), count in sorted((allowed - found).items()):
        errors.append(
            f"stale grandfathered machine-path baseline for {relative}: expected {count} more "
            f"occurrence(s) of {value!r}; update or remove the exception with the configuration change"
        )
    return errors


def audit_top_level_source_modules(files: set[str], baseline: dict) -> list[str]:
    """Reject production-looking source trees not classified by the top-level baseline."""
    classified_directories = set(flatten_groups(baseline["top_level_directories"]))
    classified_directories.update(
        Path(relative).parts[0]
        for relative in baseline["source_registration"]["generated_sources"]
        if len(Path(relative).parts) > 1
    )
    unknown_modules = {
        relative.split("/", 1)[0]
        for relative in files
        if "/" in relative
        and Path(relative).suffix.lower() in SOURCE_SUFFIXES
        and relative.split("/", 1)[0] not in classified_directories
    }
    return [
        f"unregistered top-level source module: {directory}/; introducing production C/C++ code "
        "requires intentional baseline classification, documented ownership, appropriate CMake "
        "registration, and architecture/repository-layout documentation"
        for directory in sorted(unknown_modules)
    ]


def documented_modules(root: Path, relative: str, marker: str) -> set[str] | None:
    """Read a deliberately parseable source-module marker from documentation."""
    path = root / relative
    if not path.is_file():
        return None
    match = re.search(
        rf"<!--\s*{re.escape(marker)}\s*:\s*([^>]+?)\s*-->",
        path.read_text(encoding="utf-8"),
    )
    if match is None:
        return None
    separator = ";" if "=" in match.group(1) else ","
    items = [item.strip() for item in match.group(1).split(separator) if item.strip()]
    if "=" in match.group(1) and any(not item.partition("=")[2].strip() for item in items):
        return None
    return {item.partition("=")[0].strip() for item in items}


def audit_documentation_contract(root: Path, baseline: dict) -> list[str]:
    """Keep stable architecture and layout markers aligned with the canonical inventory."""
    policy = baseline["documentation_contract"]
    source_modules = set(baseline["top_level_directories"]["source_modules"])
    errors: list[str] = []
    documents = (
        ("docs/developer/architecture.md", policy["architecture_source_modules_marker"]),
        ("docs/developer/repository_layout.md", policy["repository_layout_source_modules_marker"]),
    )
    for relative, marker in documents:
        documented = documented_modules(root, relative, marker)
        if documented is None:
            errors.append(
                f"{relative} is missing the parseable '<!-- {marker}: ... -->' source-module "
                "inventory required by repository_structure_baseline.json"
            )
        elif documented != source_modules:
            errors.append(
                f"{relative} source-module documentation is not aligned with the canonical baseline: "
                f"missing {sorted(source_modules - documented)}, unexpected {sorted(documented - source_modules)}; "
                f"update its '<!-- {marker}: ... -->' marker and module description"
            )

    root_roles = policy["root_source_roles"]
    allowed_sources = set(baseline["source_registration"]["root_project_sources"])
    if set(root_roles) != allowed_sources:
        errors.append(
            "documented root-source roles differ from source_registration.root_project_sources: "
            f"missing roles {sorted(allowed_sources - set(root_roles))}, stale roles "
            f"{sorted(set(root_roles) - allowed_sources)}; align documentation_contract.root_source_roles"
        )
    for source, role in sorted(root_roles.items()):
        if source not in baseline["root_file_roles"].get(role, []):
            errors.append(
                f"approved root source {source} has undocumented role {role!r}; add it to the matching "
                "root_file_roles entry and document that role in docs/developer/repository_layout.md"
            )
    layout = root / "docs/developer/repository_layout.md"
    role_marker = policy["repository_layout_root_source_roles_marker"]
    role_match = re.search(
        rf"<!--\s*{re.escape(role_marker)}\s*:\s*([^>]+?)\s*-->",
        layout.read_text(encoding="utf-8") if layout.is_file() else "",
    )
    documented_roles = {}
    if role_match:
        for item in role_match.group(1).split(","):
            source, separator, role = item.strip().partition("=")
            if separator and source and role:
                documented_roles[source] = role
    if documented_roles != root_roles:
        errors.append(
            f"docs/developer/repository_layout.md root-source role marker is not aligned: "
            f"expected {root_roles}, found {documented_roles}; update '<!-- {role_marker}: file=role -->' "
            "with the corresponding human-readable root-file role"
        )
    return errors


def audit_module_guard_sets(baseline: dict) -> list[str]:
    """Validate full and intentionally reduced guard inventories against source modules."""
    modules = set(baseline["top_level_directories"]["source_modules"])
    guards = baseline["module_guard_sets"]
    errors: list[str] = []
    dependency_modules = set(guards["dependency_directions"])
    if dependency_modules != modules:
        errors.append(
            "required dependency-direction module guard list is stale: "
            f"missing {sorted(modules - dependency_modules)}, unexpected {sorted(dependency_modules - modules)}; "
            "align module_guard_sets.dependency_directions without changing ACCEPTED_DIRECTIONS automatically"
        )
    expected_lower = modules - {"app"}
    lower_modules = set(guards["application_bootstrap_lower_level"])
    if lower_modules != expected_lower:
        errors.append(
            "application-bootstrap lower-level module guard list is stale: "
            f"missing {sorted(expected_lower - lower_modules)}, unexpected {sorted(lower_modules - expected_lower)}; "
            "align module_guard_sets.application_bootstrap_lower_level (the intentional source_modules minus app subset)"
        )
    return errors


def audit_local_configuration(root: Path, files: set[str], policy: dict) -> list[str]:
    """Reject tracked local build and IDE state and require critical ignore rules."""
    errors: list[str] = []
    prohibited_roots = set(policy["prohibited_root_files"])
    prohibited_paths = set(policy["prohibited_paths"])
    prefixes = tuple(policy["prohibited_directory_prefixes"])
    components = set(policy["prohibited_build_tree_components"])
    for relative in sorted(files):
        parts = relative.split("/")
        reason = None
        if "/" not in relative and relative in prohibited_roots:
            reason = "developer-local/generated root configuration"
        elif relative in prohibited_paths:
            reason = "project-local editor configuration intentionally kept untracked"
        elif relative.startswith(prefixes):
            reason = "repository-local build or IDE output"
        elif any(part in components for part in parts[:-1]):
            reason = "generated CMake/build-tree state"
        if reason:
            errors.append(
                f"tracked local configuration is prohibited: {relative} ({reason}); remove it from Git "
                "and keep local overrides/build output untracked"
            )

    gitignore = root / ".gitignore"
    lines = {
        line.strip() for line in gitignore.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    } if gitignore.is_file() else set()
    for pattern in policy["required_gitignore_patterns"]:
        if pattern not in lines:
            errors.append(
                f"critical local-only pattern is missing from .gitignore: {pattern}; restore it so "
                "ordinary Git usage does not stage architecture-prohibited local state"
            )
    return errors


def audit_third_party_ownership(root: Path, files: set[str], policy: dict) -> list[str]:
    """Reject conservative evidence of vendored C/C++ code outside its owned directory."""
    owned_directory = policy["owned_directory"]
    vendor_names = {name.lower() for name in policy["vendor_directory_names"]}
    exceptions = set(policy["exceptions"])
    errors = []
    for relative in sorted(files):
        if relative in exceptions or relative == owned_directory or relative.startswith(f"{owned_directory}/"):
            continue
        parts = Path(relative).parts
        is_source = Path(relative).suffix.lower() in SOURCE_SUFFIXES
        if is_source and any(part.lower() in vendor_names for part in parts[:-1]):
            errors.append(
                f"third-party ownership violation: {relative} is under a vendor-style directory "
                f"outside {owned_directory}/; move vendored code under {owned_directory}/ or record "
                "a narrow, justified exception"
            )
            continue
        if not is_source:
            continue
        path = root / relative
        if not path.is_file():
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        marker = next((item for item in policy["provenance_markers"] if item in content), None)
        if marker is not None:
            errors.append(
                f"third-party ownership violation: {relative} contains vendored provenance marker "
                f"{marker!r} outside {owned_directory}/; move the code or declare a justified exception"
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
    source_modules = set(baseline["top_level_directories"]["source_modules"])
    module_cmake_directories = set(
        baseline["source_registration"]["module_cmake_directories"]
    )

    if source_modules != module_cmake_directories:
        errors.append(
            "source-module classification differs from module CMake ownership: "
            f"documented source modules {sorted(source_modules)}, module CMake directories "
            f"{sorted(module_cmake_directories)}; every top-level source module must own its "
            "application source registration"
        )

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
            "restricted to explicit entry points so feature ownership remains modular; if a new "
            "entry point is intentional, align repository_structure_baseline.json root source and "
            "role contracts plus docs/developer/architecture.md and repository_layout.md"
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
                errors.append(
                    f"module source-registration file is missing: {module}/CMakeLists.txt; add explicit "
                    "module-level target_sources ownership in the same architecture change"
                )
            elif not re.search(r"\btarget_sources\s*\(", module_cmake.read_text(encoding="utf-8"), re.IGNORECASE):
                errors.append(
                    f"module source-registration file lacks explicit target_sources ownership: "
                    f"{module}/CMakeLists.txt"
                )
        executable_match = re.search(r"add_executable\s*\(([^)]*)\)", cmake, re.DOTALL)
        executable_sources = executable_match.group(1) if executable_match else ""
        for entry_point in baseline["source_registration"]["root_compiled_entry_points"]:
            if not re.search(rf"\b{re.escape(entry_point)}\b", executable_sources):
                errors.append(f"root compiled entry point is not registered by add_executable: {entry_point}")
        for group in baseline["source_registration"]["root_registered_source_groups"]:
            if not re.search(rf"(^|\s){re.escape(group)}/", executable_sources):
                errors.append(f"root source group is not registered by add_executable: {group}/")
        allowed_root_groups = set(
            baseline["source_registration"]["root_registered_source_groups"]
        )
        for module in sorted(source_modules - allowed_root_groups):
            feature_source_pattern = (
                rf"(^|\s)[\"']?{re.escape(module)}/[^\s\)]*\.(?:c|cc|cpp|cxx)\b"
            )
            if re.search(feature_source_pattern, executable_sources, re.IGNORECASE):
                errors.append(
                    f"root add_executable retains feature source ownership for {module}/; "
                    f"register application implementation sources in {module}/CMakeLists.txt"
                )

    guard = baseline["structural_guard"]
    errors.extend(audit_documentation_contract(root, baseline))
    errors.extend(audit_module_guard_sets(baseline))
    errors.extend(audit_top_level_source_modules(files, baseline))
    errors.extend(audit_local_configuration(root, files, guard["local_configuration"]))
    errors.extend(audit_third_party_ownership(root, files, guard["third_party_ownership"]))
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
