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

    # Arch package and CI assertions must agree with the canonical offline bundle.
    pkgbuild = (ROOT / "packaging/arch/PKGBUILD").read_text(encoding="utf-8")
    workflow = (ROOT / ".github/workflows/arch-package.yml").read_text(encoding="utf-8")
    for page in ("index.md", "shortcuts-and-command-bar.md"):
        if f'test -f "$pkgdir/opt/perastage/help/en/{page}"' not in pkgbuild:
            errors.append(f"Arch package must require English manual page: {page}")
        if f"grep -q '^opt/perastage/help/en/{page}$' arch-package-contents.txt" not in workflow:
            errors.append(f"Arch CI must require English manual page: {page}")
    if 'test ! -e "$pkgdir/opt/perastage/help.md"' not in pkgbuild:
        errors.append("Arch package must reject root legacy help")
    if "! grep -q '^opt/perastage/help.md$' arch-package-contents.txt" not in workflow:
        errors.append("Arch CI must reject root legacy help")

    # Every existing release product must carry the independent experimental CLI.
    package_contracts = {
        "packaging/arch/PKGBUILD": (
            'ln -s /opt/perastage/perastage-cli "$pkgdir/usr/bin/perastage-cli"',
            'test -x "$pkgdir/opt/perastage/perastage-cli"',
            'readlink "$pkgdir/usr/bin/perastage-cli"',
        ),
        ".github/workflows/arch-package.yml": (
            "grep -q '^opt/perastage/perastage-cli$' arch-package-contents.txt",
            "grep -q '^usr/bin/perastage-cli$' arch-package-contents.txt",
            'readlink "$CLI_PACKAGE_ROOT/usr/bin/perastage-cli"',
            '"$CLI_PACKAGE_ROOT/opt/perastage/perastage-cli" --version',
            '"$CLI_PACKAGE_ROOT/opt/perastage/perastage-cli" --help',
        ),
        ".github/workflows/windows-installer.yml": (
            '"Perastage.exe", "perastage-cli.exe"',
            '"./out/install/Release/perastage-cli.exe" --version',
            '"./out/install/Release/perastage-cli.exe" --help',
            "Validate installed CLI payload",
            '(Join-Path $installDir "perastage-cli.exe") --version',
            '(Join-Path $installDir "perastage-cli.exe") --help',
        ),
        ".github/workflows/linux-installer.yml": (
            'cp "$STAGE_DIR/perastage-cli" "$APPDIR/usr/bin/perastage-cli"',
            '--executable "$APPDIR/usr/bin/perastage-cli"',
            'test -x squashfs-root/usr/bin/perastage-cli',
            'squashfs-root/AppRun --cli --version',
            'squashfs-root/AppRun --cli --help',
            '"./Perastage-${VERSION}-x86_64.AppImage" --cli --version',
            '"./Perastage-${VERSION}-x86_64.AppImage" --cli --help',
        ),
    }
    for name in ("macos-installer.yml", "macos-15-manual-installer.yml"):
        package_contracts[f".github/workflows/{name}"] = (
            "test -x out/install/Release/Perastage.app/Contents/MacOS/perastage-cli",
            '-x "$CLI_PATH"',
            'check_macos_bundle_dependencies.py "$APP_PATH"',
            '"$CLI_PATH" --version',
            '"$CLI_PATH" --help',
            'check_macos_bundle_dependencies.py "$MOUNT_DIR/Perastage.app"',
            '"$MOUNT_DIR/Perastage.app/Contents/MacOS/perastage-cli" --version',
            '"$MOUNT_DIR/Perastage.app/Contents/MacOS/perastage-cli" --help',
        )
    for name, requirements in package_contracts.items():
        text = (ROOT / name).read_text(encoding="utf-8")
        for requirement in requirements:
            if requirement not in text:
                errors.append(f"{name} is missing shipped CLI contract: {requirement}")

    inno = (ROOT / "packaging/windows/Perastage.iss").read_text(encoding="utf-8")
    if not re.search(r'Source: "\{#StageDir\}\\\*";[^\n]*recursesubdirs', inno):
        errors.append("Inno Setup must consume the complete staged runtime payload")
    if re.search(r"^Name:.*perastage-cli", inno, re.MULTILINE | re.IGNORECASE):
        errors.append("Inno Setup must not create CLI shortcuts")

    install_cmake = (ROOT / "cmake/PerastageInstall.cmake").read_text(encoding="utf-8")
    if re.search(r"install\s*\([^)]*(?:tools/mcp|perastage[_-]mcp)",
                 install_cmake, re.IGNORECASE | re.DOTALL):
        errors.append("The MCP adapter must remain outside native release installation")

    if errors:
        print("Packaging ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Packaging ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
