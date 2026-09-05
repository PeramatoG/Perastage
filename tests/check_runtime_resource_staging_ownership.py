#!/usr/bin/env python3
"""Validate ownership and boundaries of build-tree runtime asset staging."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
RUNTIME_STAGING_MODULE = ROOT / "cmake" / "PerastageRuntimeStaging.cmake"


def main() -> int:
    """Reject runtime staging outside its module or across adjacent boundaries."""
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    runtime_cmake = RUNTIME_STAGING_MODULE.read_text(encoding="utf-8")
    errors: list[str] = []

    runtime_include = re.compile(
        r"include\s*\(\s*(?:\"|')?cmake/PerastageRuntimeStaging\.cmake(?:\"|')?\s*\)",
        re.IGNORECASE,
    )
    if len(runtime_include.findall(root_cmake)) != 1:
        errors.append("root CMake must include PerastageRuntimeStaging.cmake exactly once")

    if re.search(r"add_custom_command\s*\([^)]*\bPOST_BUILD\b", root_cmake,
                 re.IGNORECASE | re.DOTALL):
        errors.append("root CMake still owns POST_BUILD runtime staging")

    contracts = (
        "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources",
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>",
        "${CMAKE_SOURCE_DIR}/resources",
        "PERASTAGE_RUNTIME_LOCALE_DIR",
        "${PERASTAGE_GENERATED_LOCALE_DIR}",
        "PerastageCopyRuntimeCatalog.cmake",
        "PerastageVerifyFileExists.cmake",
        "${CMAKE_SOURCE_DIR}/help.md",
        "${CMAKE_SOURCE_DIR}/LICENSE.txt",
        "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md",
        "${CMAKE_SOURCE_DIR}/licenses",
        "fixtures trusses misc scene_objects projects default_layouts hoists",
        "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}",
        "library/fixtures/Dummy 1ch.gdtf",
    )
    for contract in contracts:
        if contract not in runtime_cmake:
            errors.append(f"runtime staging module is missing contract: {contract}")

    forbidden_contracts = (
        r"\binstall\s*\(",
        r"\binclude\s*\(\s*CPack\b",
        r"\bset\s*\(\s*CPACK_",
        r"\bfind_package\s*\(\s*Gettext\b",
        r"\badd_custom_(?:command|target)\s*\([^)]*\bmsgfmt\b",
        r"PerastageCompileGettextCatalog\.cmake",
    )
    for pattern in forbidden_contracts:
        if re.search(pattern, runtime_cmake, re.IGNORECASE | re.DOTALL):
            errors.append(f"runtime staging module crosses an ownership boundary: {pattern}")

    localization_gate = re.search(
        r"if\s*\(\s*PERASTAGE_ENABLE_LOCALIZATION\s*\).*PerastageCopyRuntimeCatalog\.cmake",
        runtime_cmake,
        re.IGNORECASE | re.DOTALL,
    )
    if not localization_gate:
        errors.append("runtime catalog staging must remain gated by PERASTAGE_ENABLE_LOCALIZATION")

    if errors:
        print("Runtime resource staging ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Runtime resource staging ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
