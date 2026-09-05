#!/usr/bin/env python3
"""Validate ownership boundaries for platform-specific target configuration."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
PLATFORM_DIR = ROOT / "cmake" / "platform"


def require_contracts(text: str, contracts: tuple[str, ...], owner: str, errors: list[str]) -> None:
    """Record contracts that are missing from their expected owner."""
    for contract in contracts:
        if contract not in text:
            errors.append(f"{owner} is missing contract: {contract}")


def main() -> int:
    """Reject misplaced or incomplete platform target configuration."""
    root = ROOT_CMAKE.read_text(encoding="utf-8")
    dispatcher = (PLATFORM_DIR / "PerastagePlatform.cmake").read_text(encoding="utf-8")
    windows = (PLATFORM_DIR / "PerastageWindows.cmake").read_text(encoding="utf-8")
    macos = (PLATFORM_DIR / "PerastageMacOS.cmake").read_text(encoding="utf-8")
    linux = (PLATFORM_DIR / "PerastageLinux.cmake").read_text(encoding="utf-8")
    errors: list[str] = []

    dispatcher_include = "include(cmake/platform/PerastagePlatform.cmake)"
    if root.count(dispatcher_include) != 1:
        errors.append("root CMake must include the platform dispatcher exactly once")
    elif root.index(dispatcher_include) < root.index("add_executable(${PROJECT_NAME}"):
        errors.append("root CMake must dispatch platform configuration after target creation")

    require_contracts(
        dispatcher,
        (
            "if(WIN32)",
            "PerastageWindows.cmake",
            "elseif(APPLE)",
            "PerastageMacOS.cmake",
            "elseif(UNIX)",
            "PerastageLinux.cmake",
        ),
        "platform dispatcher",
        errors,
    )
    require_contracts(
        windows,
        (
            "resources/Perastage.rc",
            "WIN32_EXECUTABLE TRUE",
            "/Zc:__cplusplus",
            '"$<$<CONFIG:Release>:/Zi>"',
            '"$<$<CONFIG:Release>:/DEBUG>"',
            '"$<$<CONFIG:Release>:/OPT:REF>"',
            '"$<$<CONFIG:Release>:/OPT:ICF>"',
            "target_link_libraries(${PROJECT_NAME} PRIVATE Dbghelp)",
            "add_custom_target(perastage_symbols",
            "${CMAKE_SOURCE_DIR}/out/symbols/windows",
            "PDB_SOURCE=$<TARGET_PDB_FILE:${PROJECT_NAME}>",
            "cmake/PerastageCopySymbols.cmake",
        ),
        "Windows platform module",
        errors,
    )
    require_contracts(
        macos,
        (
            "resources/Perastage.icns",
            "resources/PerastageProject.icns",
            "resources/PerastageMVR.icns",
            'MACOSX_PACKAGE_LOCATION "Resources"',
            "cmake/PerastageInfo.plist.in",
            "MACOSX_BUNDLE TRUE",
            "MACOSX_BUNDLE_INFO_PLIST",
            'RESOURCE "${PERASTAGE_MACOS_RESOURCES}"',
        ),
        "macOS platform module",
        errors,
    )
    if "no additional target-level build configuration" not in linux:
        errors.append("Linux platform module must document its intentionally empty boundary")

    platform_contracts = (
        "resources/Perastage.rc",
        "WIN32_EXECUTABLE",
        "/Zc:__cplusplus",
        "Dbghelp",
        "perastage_symbols",
        "PERASTAGE_SYMBOLS_DIR",
        "resources/Perastage.icns",
        "MACOSX_PACKAGE_LOCATION",
        "PerastageInfo.plist.in",
        "MACOSX_BUNDLE",
    )
    for contract in platform_contracts:
        if contract in root:
            errors.append(f"root CMake still owns platform contract: {contract}")

    forbidden = {
        "dependency discovery": r"\bfind_package\s*\(",
        "localization": r"\b(?:Gettext|msgfmt|perastage_(?:translations|update_translations))\b",
        "install rules": r"\binstall\s*\(",
        "runtime staging": r"\bPOST_BUILD\b",
        "packaging": r"\bCPACK_|\binclude\s*\(\s*CPack\s*\)",
        "feature registration": r"\badd_subdirectory\s*\(",
        "generic includes": r"\btarget_include_directories\s*\(",
        "generic dependencies": r"\b(?:tinyxml2::tinyxml2|OpenGL::GL|CURL::libcurl|_wx_libs)\b",
    }
    for module_name, module_text in (("dispatcher", dispatcher), ("Windows", windows), ("macOS", macos), ("Linux", linux)):
        for responsibility, pattern in forbidden.items():
            if re.search(pattern, module_text, re.IGNORECASE):
                errors.append(f"{module_name} platform module contains forbidden {responsibility}")

    if errors:
        print("Platform CMake ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print("Platform CMake ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
