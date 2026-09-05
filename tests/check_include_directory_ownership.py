#!/usr/bin/env python3
"""Validate application include-directory ownership after ORG-023/024."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROOT_CMAKE = ROOT / "CMakeLists.txt"

MODULE_INCLUDES = {
    "core": (".", "diagnostics", "layouts", "print"),
    "gui": (".", "mainwindow/controllers", "mainwindow/ids"),
    "models": (".",),
    "mvr": (".",),
    "viewer_common": (".",),
    "viewer2d": (".", "pdf"),
    "viewer3d": (".", "interfaces", "resources", "culling", "labels", "picking", "render"),
}
ROOT_SHARED_INCLUDES = (
    "${CMAKE_SOURCE_DIR}/third_party",
    "${CMAKE_SOURCE_DIR}/resources",
    "${_wx_includes}",
    "${NANOVG_INCLUDE_DIR}",
)
PHASE3_MODULES = (
    "cmake/PerastageDependencies.cmake",
    "cmake/PerastageLocalization.cmake",
    "cmake/platform/PerastagePlatform.cmake",
    "cmake/PerastageInstall.cmake",
    "cmake/PerastageRuntimeStaging.cmake",
    "cmake/PerastagePackaging.cmake",
)


def target_include_body(cmake: str) -> str | None:
    """Return the application's single target include-directory body."""
    matches = re.findall(
        r"target_include_directories\s*\(\s*\$\{PROJECT_NAME\}\s+PRIVATE(?P<body>.*?)\)",
        cmake,
        re.DOTALL,
    )
    return matches[0] if len(matches) == 1 else None


def main() -> int:
    """Check root, module, source-registration, and Phase 3 include contracts."""
    root = ROOT_CMAKE.read_text(encoding="utf-8")
    errors: list[str] = []
    root_body = target_include_body(root)
    if root_body is None:
        errors.append("root must contain exactly one application target include block")
    else:
        entries = tuple(line.strip() for line in root_body.splitlines() if line.strip())
        if entries != ROOT_SHARED_INCLUDES:
            errors.append(
                "root application includes must match the documented shared/dependency list: "
                f"expected {ROOT_SHARED_INCLUDES}, found {entries}"
            )
        if "${CMAKE_SOURCE_DIR}" in entries:
            errors.append("root must not expose CMAKE_SOURCE_DIR as a catch-all include")

    for module, relatives in MODULE_INCLUDES.items():
        cmake = (ROOT / module / "CMakeLists.txt").read_text(encoding="utf-8")
        body = target_include_body(cmake)
        if body is None:
            errors.append(f"{module} must contain exactly one application target include block")
            continue
        for relative in relatives:
            entry = "${CMAKE_CURRENT_SOURCE_DIR}" + (f"/{relative}" if relative != "." else "")
            if len(re.findall(rf"^\s*{re.escape(entry)}\s*$", body, re.MULTILINE)) != 1:
                errors.append(f"{module} does not own required include path {entry}")
        if "${CMAKE_SOURCE_DIR}" in body:
            errors.append(f"{module} must use module-relative include paths")
        if f"target_sources(${{PROJECT_NAME}}" not in cmake:
            errors.append(f"{module} lost explicit application source registration")

    production_cmake = [ROOT_CMAKE]
    production_cmake.extend(ROOT / module / "CMakeLists.txt" for module in MODULE_INCLUDES)
    for path in production_cmake:
        cmake = path.read_text(encoding="utf-8")
        if re.search(r"(?<!target_)\binclude_directories\s*\(", cmake):
            errors.append(f"{path.relative_to(ROOT)} uses global include_directories()")
        if re.search(r"/(?:home|Users)/|[A-Za-z]:[/\\]", cmake):
            errors.append(f"{path.relative_to(ROOT)} contains an absolute developer path")

    for module in PHASE3_MODULES:
        if root.count(f"include({module})") != 1:
            errors.append(f"root must preserve Phase 3 ownership for {module}")
    if re.search(r"file\s*\(\s*GLOB(?:_RECURSE)?\b", root, re.IGNORECASE):
        errors.append("root must keep explicit source registration")

    if errors:
        print("Include-directory ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print("Include-directory ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
