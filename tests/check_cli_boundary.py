#!/usr/bin/env python3
"""Enforce the dedicated headless CLI ownership and lifecycle boundary."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.MULTILINE)
FORBIDDEN_INCLUDE_BASENAMES = {
    "configmanager.h",
    "credentialstore.h",
    "gdtfnet.h",
    "mainwindow.h",
    "perastage_app.h",
    "projectutils.h",
    "splashscreen.h",
    "startup_profile.h",
}
FORBIDDEN_INCLUDE_PATHS = {"localization/localization_manager.h"}
ALLOWED_LINK_TARGETS = {
    "perastage_cli_support",
    "perastage_inspection_gdtf",
    "perastage_inspection_mvr",
    "perastage_inspection_resource",
    "perastage_inspection_serialization",
}


def normalized_include(include: str) -> str:
    """Normalize include separators for platform-independent comparisons."""
    return include.replace("\\", "/").lower()


def forbidden_include(include: str) -> bool:
    """Identify direct GUI, wxWidgets, and application-state bootstrap includes."""
    normalized = normalized_include(include)
    basename = normalized.rsplit("/", 1)[-1]
    if normalized.startswith("wx/"):
        return True
    if basename in FORBIDDEN_INCLUDE_BASENAMES or normalized in FORBIDDEN_INCLUDE_PATHS:
        return True
    return any(
        normalized.startswith(f"{module}/")
        for module in ("app", "gui", "models", "mvr", "viewer2d", "viewer3d", "viewer_common")
    )


def check(root: Path = REPOSITORY_ROOT) -> list[str]:
    """Return violations of the dedicated CLI architecture contract."""
    root = root.resolve()
    cli = root / "cli"
    errors: list[str] = []
    cli_cmake_path = cli / "CMakeLists.txt"
    root_cmake_path = root / "CMakeLists.txt"
    if not cli_cmake_path.is_file():
        return ["cli/ must have explicit local CMake ownership"]
    if not root_cmake_path.is_file():
        return ["Root CMakeLists.txt is missing"]
    cli_cmake = cli_cmake_path.read_text(encoding="utf-8")
    root_cmake = root_cmake_path.read_text(encoding="utf-8")

    requirements = (
        (cli_cmake, r"add_executable\s*\(\s*perastage_cli\b", "perastage_cli target is missing"),
        (cli_cmake, r"OUTPUT_NAME\s+[\"']?perastage-cli[\"']?", "CLI output name must be perastage-cli"),
        (root_cmake, r"add_subdirectory\s*\(\s*cli\s*\)", "Root CMake must register cli/"),
    )
    for text, pattern, message in requirements:
        if not re.search(pattern, text, re.IGNORECASE):
            errors.append(message)

    contamination = re.compile(
        r"target_sources\s*\(\s*\$\{PROJECT_NAME\}[^)]*(?:cli[/\\]|cli_runner|perastage_cli)",
        re.IGNORECASE | re.DOTALL,
    )
    for path, text in ((root_cmake_path, root_cmake), (cli_cmake_path, cli_cmake)):
        if contamination.search(text):
            errors.append(f"CLI sources must not be registered into the GUI target: {path.relative_to(root)}")

    forbidden_cli_cmake = {
        r"(?:WIN32_EXECUTABLE|MACOSX_BUNDLE)\s+TRUE": "CLI must remain a console, non-bundle executable",
        r"target_link_libraries\s*\(\s*perastage_cli(?:_support)?\b[^)]*\b(?:app|gui|viewer2d|viewer3d|viewer_common)\b": "CLI targets must not link application, GUI, or viewer targets",
        r"install\s*\([^)]*\bperastage_cli\b": "CLI must not be installed",
    }
    for pattern, message in forbidden_cli_cmake.items():
        if re.search(pattern, cli_cmake, re.IGNORECASE | re.DOTALL):
            errors.append(message)

    link_blocks = re.findall(
        r"target_link_libraries\s*\(\s*perastage_cli(?:_support)?\b([^)]*)\)",
        cli_cmake,
        re.IGNORECASE | re.DOTALL,
    )
    for block in link_blocks:
        linked_targets = set(re.findall(r"\bperastage_[a-z0-9_]+\b", block.lower()))
        for target in sorted(linked_targets - ALLOWED_LINK_TARGETS):
            errors.append(f"CLI target has a forbidden direct link dependency: {target}")

    forbidden_tokens = re.compile(r"wxIMPLEMENT_APP|\bwxApp\b|\bConfigManager\b")
    for path in cli.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        source = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_RE.findall(source):
            if forbidden_include(include):
                errors.append(
                    f'CLI source has a forbidden direct include "{include}": {path.relative_to(root)}'
                )
        if forbidden_tokens.search(source):
            errors.append(f"CLI source uses a GUI/application bootstrap token: {path.relative_to(root)}")

    packaging_files = [
        root / "cmake/PerastageInstall.cmake",
        root / "cmake/PerastageRuntimeStaging.cmake",
        root / "cmake/PerastagePackaging.cmake",
    ]
    packaging_files.extend(path for path in (root / "packaging").rglob("*") if path.is_file())
    for path in packaging_files:
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "perastage-cli" in text or "perastage_cli" in text:
            errors.append(f"Development CLI must not be installed or packaged: {path.relative_to(root)}")
    return errors


def main() -> int:
    """Run the CLI architecture policy check."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=REPOSITORY_ROOT)
    args = parser.parse_args()
    errors = check(args.root)
    if errors:
        print("CLI boundary check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("CLI boundary check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
