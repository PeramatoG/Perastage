#!/usr/bin/env python3
"""Enforce the dedicated headless CLI ownership and lifecycle boundary."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "cli"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


def check() -> list[str]:
    """Return violations of the dedicated CLI architecture contract."""
    errors: list[str] = []
    cmake_path = CLI / "CMakeLists.txt"
    if not cmake_path.is_file():
        return ["cli/ must have explicit local CMake ownership"]
    cmake = cmake_path.read_text(encoding="utf-8")
    root_cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

    requirements = (
        (r"add_executable\s*\(\s*perastage_cli\b", "perastage_cli target is missing"),
        (r"OUTPUT_NAME\s+[\"']?perastage-cli[\"']?", "CLI output name must be perastage-cli"),
        (r"add_subdirectory\s*\(\s*cli\s*\)", "Root CMake must register cli/"),
    )
    for pattern, message in requirements:
        text = root_cmake if "Root" in message else cmake
        if not re.search(pattern, text, re.IGNORECASE):
            errors.append(message)

    forbidden_cmake = {
        r"target_sources\s*\(\s*\$\{PROJECT_NAME\}.*?cli[/\\]": "CLI sources must not be registered into the GUI target",
        r"(?:WIN32_EXECUTABLE|MACOSX_BUNDLE)\s+TRUE": "CLI must remain a console, non-bundle executable",
        r"target_link_libraries\s*\(\s*perastage_cli(?:_support)?\b[^)]*\b(?:app|gui|viewer2d|viewer3d|viewer_common)\b": "CLI targets must not link application, GUI, or viewer targets",
        r"install\s*\([^)]*\bperastage_cli\b": "CLI must not be installed",
    }
    for pattern, message in forbidden_cmake.items():
        if re.search(pattern, cmake, re.IGNORECASE | re.DOTALL):
            errors.append(message)

    forbidden_source = re.compile(
        r"wxIMPLEMENT_APP|wxApp|ConfigManager|(?:^|[/\\])(?:app|gui|viewer2d|viewer3d|viewer_common)(?:[/\\]|\.)",
        re.MULTILINE,
    )
    for path in CLI.rglob("*"):
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            if forbidden_source.search(path.read_text(encoding="utf-8", errors="replace")):
                errors.append(f"CLI source crosses the GUI/application lifecycle boundary: {path.relative_to(ROOT)}")

    packaging_files = [ROOT / "cmake/PerastageInstall.cmake", ROOT / "cmake/PerastageRuntimeStaging.cmake"]
    packaging_files.extend(path for path in (ROOT / "packaging").rglob("*") if path.is_file())
    for path in packaging_files:
        if "perastage-cli" in path.read_text(encoding="utf-8", errors="replace") or "perastage_cli" in path.read_text(encoding="utf-8", errors="replace"):
            errors.append(f"Development CLI must not be installed or packaged: {path.relative_to(ROOT)}")
    return errors


def main() -> int:
    """Run the CLI architecture policy check."""
    errors = check()
    if errors:
        print("CLI boundary check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("CLI boundary check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
