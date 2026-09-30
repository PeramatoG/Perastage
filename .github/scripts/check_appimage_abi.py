#!/usr/bin/env python3
"""Enforce the host-library ABI ceiling for every ELF object in an AppImage tree."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys


# Ubuntu 22.04 and GCC 11 define the intentional x86_64 AppImage ABI ceiling.
ABI_LIMITS = {
    "GLIBC": (2, 35),
    "GLIBCXX": (3, 4, 29),
    "CXXABI": (1, 3, 13),
}
VERSION_PATTERN = re.compile(r"\b(GLIBCXX|GLIBC|CXXABI)_([0-9]+(?:\.[0-9]+)+)\b")
FORBIDDEN_RUNTIME_PATTERNS = (
    re.compile(r"^libc\.so(?:\.|$)"),
    re.compile(r"^ld-linux(?:-|\.)"),
    re.compile(r"^ld-[0-9].*\.so(?:\.|$)"),
)


def parse_version(value: str) -> tuple[int, ...]:
    """Convert a dotted ABI version into numerically comparable components."""
    return tuple(int(component) for component in value.split("."))


def parse_required_versions(readelf_output: str) -> list[tuple[str, tuple[int, ...], str]]:
    """Extract imported ABI versions from readelf's version-needs section only."""
    required = []
    in_needs = False
    for line in readelf_output.splitlines():
        if line.startswith("Version needs section"):
            in_needs = True
            continue
        if in_needs and line and not line[0].isspace():
            in_needs = False
        if not in_needs:
            continue
        for match in VERSION_PATTERN.finditer(line):
            family, value = match.groups()
            required.append((family, parse_version(value), match.group(0)))
    return required


def exceeds_limit(family: str, version: tuple[int, ...]) -> bool:
    """Return whether a parsed ABI version exceeds its configured ceiling."""
    return version > ABI_LIMITS[family]


def is_elf(path: Path) -> bool:
    """Identify ELF files by magic bytes rather than names or executable bits."""
    try:
        with path.open("rb") as stream:
            return stream.read(4) == b"\x7fELF"
    except OSError:
        return False


def is_forbidden_runtime(path: Path) -> bool:
    """Identify a packaged glibc runtime or dynamic loader by its basename."""
    return any(pattern.match(path.name) for pattern in FORBIDDEN_RUNTIME_PATTERNS)


def check_tree(root: Path) -> int:
    """Inspect all packaged ELF objects and print compatibility diagnostics."""
    if not root.is_dir():
        print(f"AppImage extraction directory does not exist: {root}", file=sys.stderr)
        return 2

    elf_files = [path for path in root.rglob("*") if path.is_file() and is_elf(path)]
    highest: dict[str, tuple[int, ...] | None] = {family: None for family in ABI_LIMITS}
    violations = []

    for path in elf_files:
        relative = path.relative_to(root)
        if is_forbidden_runtime(path):
            violations.append(f"{relative}: bundled glibc/loader is forbidden")
        result = subprocess.run(
            ["readelf", "--version-info", "--wide", str(path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            violations.append(f"{relative}: readelf failed: {result.stderr.strip()}")
            continue
        for family, version, symbol in parse_required_versions(result.stdout):
            if highest[family] is None or version > highest[family]:
                highest[family] = version
            if exceeds_limit(family, version):
                violations.append(
                    f"{relative}: requires {symbol}, above "
                    f"{family}_{'.'.join(map(str, ABI_LIMITS[family]))}"
                )

    print(f"Inspected {len(elf_files)} ELF objects under {root}")
    for family, limit in ABI_LIMITS.items():
        found = highest[family]
        found_text = ".".join(map(str, found)) if found is not None else "none"
        print(f"Highest {family} requirement: {found_text} (limit {'.'.join(map(str, limit))})")
    if violations:
        print("AppImage ABI compatibility violations:", file=sys.stderr)
        for violation in violations:
            print(f"  - {violation}", file=sys.stderr)
        return 1
    return 0


def main() -> int:
    """Parse command-line arguments and check the extracted AppImage tree."""
    parser = argparse.ArgumentParser()
    parser.add_argument("appdir", type=Path, help="Extracted AppImage directory")
    return check_tree(parser.parse_args().appdir)


if __name__ == "__main__":
    raise SystemExit(main())
