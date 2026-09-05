#!/usr/bin/env python3
"""Validate ownership of localization build configuration in CMake."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
LOCALIZATION_MODULE = ROOT / "cmake" / "PerastageLocalization.cmake"
RUNTIME_STAGING_MODULE = ROOT / "cmake" / "PerastageRuntimeStaging.cmake"


def has_call(source: str, command: str, first_argument: str) -> bool:
    """Return whether a CMake command owns the requested first argument."""
    return bool(
        re.search(
            rf"{re.escape(command)}\s*\(\s*{re.escape(first_argument)}\b",
            source,
            re.IGNORECASE,
        )
    )


def main() -> int:
    """Reject localization build logic outside its dedicated CMake module."""
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    localization_cmake = LOCALIZATION_MODULE.read_text(encoding="utf-8")
    runtime_staging_cmake = RUNTIME_STAGING_MODULE.read_text(encoding="utf-8")
    errors: list[str] = []

    localization_include = re.compile(
        r"include\s*\(\s*(?:\"|')?cmake/PerastageLocalization\.cmake(?:\"|')?\s*\)",
        re.IGNORECASE,
    )
    if len(localization_include.findall(root_cmake)) != 1:
        errors.append("root CMake must include PerastageLocalization.cmake exactly once")

    if has_call(root_cmake, "find_package", "Gettext"):
        errors.append("root CMake still owns Gettext discovery")
    if not has_call(localization_cmake, "find_package", "Gettext"):
        errors.append("localization module does not own Gettext discovery")

    target_names = (
        "perastage_translations",
        "perastage_update_pot",
        "perastage_update_po",
        "perastage_check_translations",
        "perastage_update_catalogs",
    )
    for target_name in target_names:
        if has_call(root_cmake, "add_custom_target", target_name):
            errors.append(f"root CMake still defines {target_name}")
        if not has_call(localization_cmake, "add_custom_target", target_name):
            errors.append(f"localization module does not define {target_name}")

    build_contracts = (
        "PerastageCompileGettextCatalog.cmake",
        "scripts/localization_catalog.py",
        "add_dependencies(${PROJECT_NAME} perastage_translations)",
    )
    for contract in build_contracts:
        if contract in root_cmake:
            errors.append(f"root CMake still owns localization build contract: {contract}")
        if contract not in localization_cmake:
            errors.append(f"localization module is missing build contract: {contract}")

    unrelated_dependency = "add_dependencies(${PROJECT_NAME} perastage_generated_dummy_gdtf)"
    if unrelated_dependency not in root_cmake:
        errors.append("root CMake must retain the generated dummy GDTF dependency")
    if unrelated_dependency in localization_cmake:
        errors.append("localization build module must not own the generated dummy GDTF dependency")

    runtime_contracts = (
        "PERASTAGE_RUNTIME_LOCALE_DIR",
        "PERASTAGE_RUNTIME_MO",
        "PerastageCopyRuntimeCatalog.cmake",
        "PerastageVerifyFileExists.cmake",
    )
    for contract in runtime_contracts:
        if contract in root_cmake:
            errors.append(f"root CMake still owns runtime staging contract: {contract}")
        if contract in localization_cmake:
            errors.append(f"localization build module must not own runtime staging: {contract}")
        if contract not in runtime_staging_cmake:
            errors.append(f"runtime staging module is missing localization contract: {contract}")

    if errors:
        print("Localization build ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Localization build ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
