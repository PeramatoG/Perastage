#!/usr/bin/env python3
"""Validate that the root CMake file remains focused on project orchestration."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT_CMAKE = Path(__file__).resolve().parents[1] / "CMakeLists.txt"


def main() -> int:
    """Check representative root ownership and Phase 3 module boundaries."""
    root = ROOT_CMAKE.read_text(encoding="utf-8")
    errors: list[str] = []

    required = (
        "project(\n    Perastage",
        "option(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT",
        "add_library(perastage_uuid OBJECT)",
        "add_executable(${PROJECT_NAME}",
        "if(PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT)",
        "PERASTAGE_MACOS15_LEGACY_THREAD_COMPAT=1",
        "target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)",
        "target_include_directories(${PROJECT_NAME} PRIVATE",
        "target_link_libraries(${PROJECT_NAME} PRIVATE",
        "if(BUILD_TESTING)\n    add_subdirectory(tests)",
    )
    required += tuple(f"add_subdirectory({module})" for module in (
        "core", "gui", "models", "mvr", "viewer2d", "viewer3d", "viewer_common"
    ))
    required += tuple(f"include({module})" for module in (
        "cmake/PerastageDependencies.cmake",
        "cmake/PerastageLocalization.cmake",
        "cmake/platform/PerastagePlatform.cmake",
        "cmake/PerastageInstall.cmake",
        "cmake/PerastageRuntimeStaging.cmake",
        "cmake/PerastagePackaging.cmake",
    ))
    for contract in required:
        if contract not in root:
            errors.append(f"root CMake is missing orchestration contract: {contract}")

    include_entries = (
        "${CMAKE_SOURCE_DIR}/gui",
        "${CMAKE_SOURCE_DIR}/gui/mainwindow/controllers",
        "${CMAKE_SOURCE_DIR}/gui/mainwindow/ids",
        "${CMAKE_SOURCE_DIR}/models",
        "${CMAKE_SOURCE_DIR}/core",
        "${CMAKE_SOURCE_DIR}/core/layouts",
        "${CMAKE_SOURCE_DIR}/core/print",
        "${CMAKE_SOURCE_DIR}/mvr",
        "${CMAKE_SOURCE_DIR}/third_party",
        "${CMAKE_SOURCE_DIR}/viewer3d",
        "${CMAKE_SOURCE_DIR}/viewer3d/interfaces",
        "${CMAKE_SOURCE_DIR}/viewer3d/resources",
        "${CMAKE_SOURCE_DIR}/viewer3d/culling",
        "${CMAKE_SOURCE_DIR}/viewer3d/labels",
        "${CMAKE_SOURCE_DIR}/viewer3d/picking",
        "${CMAKE_SOURCE_DIR}/viewer3d/render",
        "${CMAKE_SOURCE_DIR}/viewer2d",
        "${CMAKE_SOURCE_DIR}/viewer2d/pdf",
        "${CMAKE_SOURCE_DIR}/resources",
        "${_wx_includes}",
        "${NANOVG_INCLUDE_DIR}",
    )
    for entry in include_entries:
        if len(re.findall(rf"^\s*{re.escape(entry)}\s*$", root, re.MULTILINE)) != 1:
            errors.append(f"root generic include contract changed: {entry}")

    forbidden = {
        "dependency discovery": r"\bfind_package\s*\(",
        "catalog generation": r"PerastageCompileGettextCatalog|\bGETTEXT_(?:MSGFMT|XGETTEXT)_EXECUTABLE\b",
        "runtime staging": r"\bPOST_BUILD\b",
        "installation": r"\binstall\s*\(",
        "packaging": r"\bCPACK_|\binclude\s*\(\s*CPack\s*\)",
        "platform implementation": r"\b(?:WIN32_EXECUTABLE|MACOSX_BUNDLE|Dbghelp|perastage_symbols|Perastage\.rc|PerastageInfo\.plist)\b",
        "feature source list": r"\bset\s*\(\s*PERASTAGE_(?:CORE|GUI|MODELS|MVR|VIEWER2D|VIEWER3D)_SOURCES\b",
    }
    for responsibility, pattern in forbidden.items():
        if re.search(pattern, root, re.IGNORECASE):
            errors.append(f"root CMake regained {responsibility} ownership")

    if root.index("add_executable(${PROJECT_NAME}") > root.index("include(cmake/platform/PerastagePlatform.cmake)"):
        errors.append("platform dispatcher must follow principal target creation")
    if root.index("include(cmake/platform/PerastagePlatform.cmake)") > root.index("include(cmake/PerastageInstall.cmake)"):
        errors.append("platform target configuration must precede installation")

    if errors:
        print("Root CMake orchestration check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print("Root CMake orchestration check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
