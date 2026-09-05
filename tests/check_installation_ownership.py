#!/usr/bin/env python3
"""Validate ownership and boundaries of install-tree configuration."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"
INSTALL_MODULE = ROOT / "cmake" / "PerastageInstall.cmake"


def main() -> int:
    """Reject installation rules outside their dedicated CMake module."""
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    install_cmake = INSTALL_MODULE.read_text(encoding="utf-8")
    errors: list[str] = []

    install_include = re.compile(
        r"include\s*\(\s*(?:\"|')?cmake/PerastageInstall\.cmake(?:\"|')?\s*\)",
        re.IGNORECASE,
    )
    if len(install_include.findall(root_cmake)) != 1:
        errors.append("root CMake must include PerastageInstall.cmake exactly once")

    if re.search(r"\binstall\s*\(", root_cmake, re.IGNORECASE):
        errors.append("root CMake still owns install rules")
    if re.search(r"add_custom_target\s*\(\s*perastage_stage\b", root_cmake,
                 re.IGNORECASE):
        errors.append("root CMake still defines perastage_stage")

    contracts = (
        "install(TARGETS ${PROJECT_NAME}",
        "BUNDLE DESTINATION .",
        "install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${PERASTAGE_INSTALL_RESOURCE_DEST})",
        "PERASTAGE_GENERATED_LOCALE_DIR",
        "PERASTAGE_TRANSLATION_LANGUAGES",
        "${PERASTAGE_INSTALL_RESOURCE_DEST}/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES",
        "${PERASTAGE_INSTALL_RESOURCE_DEST}/resources/locale/${PERASTAGE_TRANSLATION_LANGUAGE}/LC_MESSAGES",
        "${CMAKE_SOURCE_DIR}/library",
        "${PERASTAGE_GENERATED_DUMMY_GDTF_ARCHIVE}",
        "${CMAKE_SOURCE_DIR}/licenses",
        "${CMAKE_SOURCE_DIR}/LICENSE.txt",
        "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md",
        "${CMAKE_SOURCE_DIR}/help.md",
        "OPTIONAL",
        "share/applications",
        "share/mime/packages",
        "share/icons/hicolor/1024x1024/apps",
        "RENAME perastage.png",
        "update-mime-database",
        "update-desktop-database",
        "include(InstallRequiredSystemLibraries)",
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin",
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin",
        "file(GLOB _vcpkg_bin_dlls",
        "CONFIGURATIONS Release RelWithDebInfo MinSizeRel",
        "CONFIGURATIONS Debug",
        "CMAKE_BUILD_TYPE STREQUAL \"Debug\"",
        "wxWidgets_LIBRARY_DIRS",
        "wxWidgets_LIBRARIES",
        "${wx_dir}/../bin",
        "${wx_dir}/wxmsw*.dll",
        "${wx_dir}/wxbase*.dll",
        "${wx_dir}/nwxmsw*.dll",
        "${wx_dir}/nwxbase*.dll",
        "wxWidgets_USE_DEBUG",
        ".*(d_vc_|ud_vc_).*\\\\.dll$",
        "add_custom_target(perastage_stage",
        "${CMAKE_SOURCE_DIR}/out/install/$<CONFIG>",
        "${CMAKE_SOURCE_DIR}/out/install/${CMAKE_BUILD_TYPE}",
        "${CMAKE_SOURCE_DIR}/out/install",
        "${CMAKE_COMMAND} -E rm -rf",
        "${CMAKE_COMMAND} --install",
    )
    for contract in contracts:
        if contract not in install_cmake:
            errors.append(f"install module is missing contract: {contract}")

    forbidden_contracts = (
        r"\bPOST_BUILD\b",
        r"\bfind_package\s*\(\s*Gettext\b",
        r"PerastageCompileGettextCatalog\.cmake",
        r"\binclude\s*\(\s*CPack\b",
        r"\bset\s*\(\s*CPACK_",
        r"\bperastage_symbols\b",
        r"\btarget_(?:sources|link_libraries|compile_options|link_options)\s*\(",
        r"\bset_target_properties\s*\(",
    )
    for pattern in forbidden_contracts:
        if re.search(pattern, install_cmake, re.IGNORECASE):
            errors.append(f"install module crosses an ownership boundary: {pattern}")

    if not re.search(
        r"if\s*\(\s*PERASTAGE_ENABLE_LOCALIZATION\s*\).*?install\s*\(\s*FILES\s+"
        r"\"\$\{PERASTAGE_GENERATED_LOCALE_DIR\}",
        install_cmake,
        re.IGNORECASE | re.DOTALL,
    ):
        errors.append("generated locale installation must remain gated by localization")

    if errors:
        print("Installation ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Installation ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
