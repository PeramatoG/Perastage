#!/usr/bin/env python3
"""Reject test target include directories that expose the repository root."""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CMAKE_FILE = REPO_ROOT / "tests" / "CMakeLists.txt"
SOURCE = CMAKE_FILE.read_text(encoding="utf-8")

TARGET_INCLUDE_RE = re.compile(r"target_include_directories\s*\((.*?)\)", re.DOTALL)
ROOT_TOKENS = {"..", "${CMAKE_SOURCE_DIR}", "${PROJECT_SOURCE_DIR}", "${CMAKE_CURRENT_SOURCE_DIR}/.."}
violations: list[str] = []

for match in TARGET_INCLUDE_RE.finditer(SOURCE):
    body = re.sub(r"#.*", "", match.group(1))
    tokens = re.split(r"\s+", body.strip())
    if not tokens:
        continue
    target = tokens[0]
    for token in tokens[1:]:
        normalized = token.strip('"')
        if normalized in ROOT_TOKENS:
            violations.append(f"{target}: {normalized}")

if violations:
    print("Test C/C++ targets must not expose the repository root as an include directory.", file=sys.stderr)
    for violation in violations:
        print(f"  {violation}", file=sys.stderr)
    sys.exit(1)
