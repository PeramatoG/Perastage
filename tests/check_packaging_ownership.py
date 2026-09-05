#!/usr/bin/env python3
"""Validate ownership and boundaries of CPack packaging configuration."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
PACKAGING_MODULE = ROOT / "cmake" / "PerastagePackaging.cmake"


def main() -> int:
    """Reject CPack configuration outside its dedicated packaging module."""
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    packaging_cmake = PACKAGING_MODULE.read_text(encoding="utf-8")
    errors: list[str] = []

    packaging_include = re.compile(
        r"include\s*\(\s*(?:\"|')?cmake/PerastagePackaging\.cmake(?:\"|')?\s*\)",
        re.IGNORECASE,
    )
    if len(packaging_include.findall(root_cmake)) != 1:
        errors.append("root CMake must include PerastagePackaging.cmake exactly once")
    elif (
        root_cmake.index("include(cmake/PerastageInstall.cmake)")
        > packaging_include.search(root_cmake).start()
    ):
        errors.append("root CMake must include packaging after installation configuration")

    if re.search(r"\bset\s*\(\s*CPACK_", root_cmake, re.IGNORECASE):
        errors.append("root CMake still owns CPack configuration")
    if re.search(r"\binclude\s*\(\s*CPack\s*\)", root_cmake, re.IGNORECASE):
        errors.append("root CMake still includes CPack directly")

    contracts = (
        'set(CPACK_PACKAGE_NAME "Perastage")',
        'set(CPACK_PACKAGE_VENDOR "Perastage")',
        'set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Perastage lighting plot editor")',
        'set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")',
        'set(CPACK_GENERATOR "NSIS")',
        "set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)",
        'set(CPACK_NSIS_DISPLAY_NAME "Perastage")',
        'set(CPACK_NSIS_PACKAGE_NAME "Perastage")',
        "string(CONCAT CPACK_NSIS_EXTRA_INSTALL_COMMANDS",
        "string(CONCAT CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS",
        "WriteRegStr HKCR '.mvr' '' 'Perastage.MVR'",
        "WriteRegStr HKCR 'Perastage.MVR' '' 'MVR Scene'",
        r"WriteRegStr HKCR 'Perastage.MVR\\\\DefaultIcon' '' '$INSTDIR\\\\Perastage.exe,0'",
        r"""WriteRegStr HKCR 'Perastage.MVR\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\Perastage.exe\\\" \\\"%1\\\"'""",
        "DeleteRegKey HKCR 'Perastage.MVR'",
        "DeleteRegValue HKCR '.mvr' ''",
        "if(PERASTAGE_ASSOCIATE_PSTG)",
        "string(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS",
        "string(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS",
        "WriteRegStr HKCR '.pstg' '' 'Perastage.Project'",
        "WriteRegStr HKCR 'Perastage.Project' '' 'Perastage Project'",
        r"WriteRegStr HKCR 'Perastage.Project\\\\DefaultIcon' '' '$INSTDIR\\\\Perastage.exe,0'",
        r"""WriteRegStr HKCR 'Perastage.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\Perastage.exe\\\" \\\"%1\\\"'""",
        "DeleteRegKey HKCR 'Perastage.Project'",
        "DeleteRegValue HKCR '.pstg' ''",
        'set(CPACK_GENERATOR "DragNDrop")',
        'set(CPACK_DMG_VOLUME_NAME "Perastage")',
        'set(CPACK_PACKAGE_FILE_NAME "Perastage-${PROJECT_VERSION}-macOS-arm64")',
    )
    for contract in contracts:
        if contract not in packaging_cmake:
            errors.append(f"packaging module is missing contract: {contract}")

    cpack_include = re.compile(r"\binclude\s*\(\s*CPack\s*\)", re.IGNORECASE)
    if len(cpack_include.findall(packaging_cmake)) != 1:
        errors.append("packaging module must include CPack exactly once")

    forbidden_contracts = (
        r"\binstall\s*\(",
        r"\bperastage_stage\b",
        r"\bPOST_BUILD\b",
        r"\bfind_package\s*\(",
        r"\bGettext\b",
        r"PerastageCompileGettextCatalog\.cmake",
        r"\bperastage_symbols\b",
        r"\btarget_(?:sources|link_libraries|compile_options|compile_definitions|"
        r"include_directories|link_options|link_directories|precompile_headers)\s*\(",
        r"\bset_target_properties\s*\(",
        r"PerastageInfo\.plist",
    )
    for pattern in forbidden_contracts:
        if re.search(pattern, packaging_cmake, re.IGNORECASE):
            errors.append(f"packaging module crosses an ownership boundary: {pattern}")

    if errors:
        print("Packaging ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Packaging ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
