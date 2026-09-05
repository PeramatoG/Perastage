#!/usr/bin/env python3
"""Validate ownership of application dependency discovery in CMake."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
DEPENDENCY_MODULE = ROOT / "cmake" / "PerastageDependencies.cmake"


def main() -> int:
    """Reject dependency discovery that escapes its dedicated CMake module."""
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    dependency_cmake = DEPENDENCY_MODULE.read_text(encoding="utf-8")
    errors: list[str] = []

    include_statement = "include(cmake/PerastageDependencies.cmake)"
    if root_cmake.count(include_statement) != 1:
        errors.append(f"root CMake must include {DEPENDENCY_MODULE.name} exactly once")

    application_packages = (
        "wxWidgets",
        "tinyxml2",
        "OpenGL",
        "CURL",
        "GLEW",
        "meshoptimizer",
        "nanovg",
        "podofo",
        "ZLIB",
        "Backward",
        "mdns",
    )
    for package in application_packages:
        if re.search(rf"find_package\s*\(\s*{re.escape(package)}\b", root_cmake):
            errors.append(f"root CMake still discovers application dependency {package}")
        if not re.search(rf"find_package\s*\(\s*{re.escape(package)}\b", dependency_cmake):
            errors.append(f"dependency module does not discover {package}")

    required_contracts = (
        "find_package(Python3 COMPONENTS Interpreter REQUIRED)",
        "set(_wx_libs",
        "set(_wx_includes",
        "find_path(NANOVG_INCLUDE_DIR nanovg.h)",
        "add_library(nanovg::nanovg UNKNOWN IMPORTED)",
        "add_library(podofo::podofo UNKNOWN IMPORTED)",
        "perastage_probe_wx_secretstore(PERASTAGE_WX_SECRETSTORE_ENABLED)",
        "if(PERASTAGE_ENABLE_MVR_XCHANGE_MDNS)",
    )
    for contract in required_contracts:
        if contract not in dependency_cmake:
            errors.append(f"dependency module is missing contract: {contract}")

    if re.search(r"find_package\s*\(\s*Gettext\b", root_cmake):
        errors.append("root CMake still discovers localization dependency Gettext")
    if re.search(r"find_package\s*\(\s*Gettext\b", dependency_cmake):
        errors.append("application dependency module must not discover localization dependency Gettext")

    if errors:
        print("Dependency discovery ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Dependency discovery ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
