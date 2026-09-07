#!/usr/bin/env python3
"""Enforce ownership of wxWidgets application bootstrap composition."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = json.loads(
    (ROOT / "docs/developer/repository_structure_baseline.json").read_text(encoding="utf-8")
)
LOWER_LEVEL_MODULES = tuple(BASELINE["module_guard_sets"]["application_bootstrap_lower_level"])
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


def check() -> list[str]:
    """Return violations of the durable application-bootstrap boundary."""
    errors: list[str] = []
    app_header = ROOT / "app/perastage_app.h"
    app_source = ROOT / "app/perastage_app.cpp"
    app_cmake = ROOT / "app/CMakeLists.txt"
    main_source = ROOT / "main.cpp"
    root_cmake = ROOT / "CMakeLists.txt"

    for path in (app_header, app_source, app_cmake):
        if not path.is_file():
            errors.append(f"App bootstrap owner is missing: {path.relative_to(ROOT)}")
    if errors:
        return errors

    header = app_header.read_text(encoding="utf-8")
    implementation = app_source.read_text(encoding="utf-8")
    main = main_source.read_text(encoding="utf-8")
    cmake = root_cmake.read_text(encoding="utf-8")
    module_cmake = app_cmake.read_text(encoding="utf-8")

    if not re.search(r"class\s+MyApp\s*:\s*public\s+wxApp", header):
        errors.append("app/perastage_app.h must own the MyApp declaration")
    if "MyApp::OnInit" not in implementation or "MyApp::OnExit" not in implementation:
        errors.append("app/perastage_app.cpp must own the MyApp lifecycle implementation")
    if "wxIMPLEMENT_APP(MyApp)" not in main:
        errors.append("main.cpp must retain the wxWidgets application-entry macro")
    if re.search(r"\bMyApp::", main) or re.search(r"class\s+MyApp\b", main):
        errors.append("main.cpp must not retain MyApp declarations or method implementations")
    if not re.search(r"add_subdirectory\s*\(\s*app\s*\)", cmake):
        errors.append("Root CMake must register app/")
    executable = re.search(r"add_executable\s*\(\s*\$\{PROJECT_NAME\}(.*?)\)", cmake, re.DOTALL)
    if not executable or "main.cpp" not in executable.group(1):
        errors.append("Root add_executable must own main.cpp")
    elif re.search(r"(?:^|\s)app[/\\]", executable.group(1)):
        errors.append("Root add_executable must not directly own App implementation sources")
    for required in ("perastage_app.cpp", "perastage_app.h"):
        if required not in module_cmake:
            errors.append(f"app/CMakeLists.txt must explicitly register {required}")

    include_pattern = re.compile(r'^\s*#\s*include\s*[<\"](?:app/)?perastage_app\.h[>\"]', re.MULTILINE)
    for module in LOWER_LEVEL_MODULES:
        for path in (ROOT / module).rglob("*"):
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
                if include_pattern.search(path.read_text(encoding="utf-8", errors="replace")):
                    errors.append(f"Lower-level module must not include App headers: {path.relative_to(ROOT)}")
    return errors


def main() -> int:
    """Run the application-bootstrap ownership policy check."""
    errors = check()
    if errors:
        print("Application bootstrap ownership check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Application bootstrap ownership check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
