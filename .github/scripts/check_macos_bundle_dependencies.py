#!/usr/bin/env python3
"""Require both executables and resolvable, bundled non-system Mach-O dependencies."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess


def output(*arguments: str) -> str:
    """Run native inspection tools without suppressing failures."""
    return subprocess.check_output(arguments, text=True)


def expand_path(value: str, loader: Path, executable: Path) -> Path | None:
    """Expand dyld-relative paths in the context of a console/app executable."""
    for prefix, base in (("@loader_path/", loader.parent),
                         ("@executable_path/", executable.parent)):
        if value.startswith(prefix):
            return base / value[len(prefix):]
    return Path(value) if value.startswith("/") else None


def check_dependency(value: str, loader: Path, executable: Path,
                     bundle: Path, rpaths: list[str]) -> None:
    """Reject missing or external non-system dependencies, including relative escapes."""
    if value.startswith(("/usr/lib/", "/System/Library/")):
        return
    if value.startswith("@rpath/"):
        candidates = [base / value[len("@rpath/"):]
                      for entry in rpaths
                      if (base := expand_path(entry + "/", loader, executable)) is not None]
    else:
        candidate = expand_path(value, loader, executable)
        candidates = [candidate] if candidate is not None else []
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved.is_relative_to(bundle.resolve()) and resolved.is_file():
            return
    raise RuntimeError(f"Unresolved or external dependency in {loader}: {value}")


def rpaths(path: Path) -> list[str]:
    """Read LC_RPATH entries while preserving paths containing spaces."""
    entries = []
    in_rpath = False
    for line in output("otool", "-l", str(path)).splitlines():
        stripped = line.strip()
        if stripped.startswith("cmd "):
            in_rpath = stripped == "cmd LC_RPATH"
        elif in_rpath and stripped.startswith("path "):
            entries.append(stripped[5:].rsplit(" (offset ", 1)[0])
            in_rpath = False
    return entries


def check(bundle: Path) -> None:
    """Audit the full bundle from both executable contexts, including the shipped CLI."""
    executables = [bundle / "Contents/MacOS" / name
                   for name in ("Perastage", "perastage-cli")]
    for executable in executables:
        if not executable.is_file():
            raise RuntimeError(f"Missing bundled executable: {executable}")
    for path in (bundle / "Contents").rglob("*"):
        if not path.is_file() or "Mach-O" not in output("file", "-b", str(path)):
            continue
        dependencies = [line.strip().split(" (compatibility version", 1)[0]
                        for line in output("otool", "-L", str(path)).splitlines()[1:]]
        for executable in executables:
            # An executable's @loader_path is relative to that executable,
            # even when its run paths are inherited by a nested dylib.
            executable_rpaths = [str(expanded) for entry in rpaths(executable)
                                 if (expanded := expand_path(entry + "/", executable, executable))
                                 is not None]
            search_paths = rpaths(path) + executable_rpaths
            for dependency in dependencies:
                check_dependency(dependency, path, executable, bundle, search_paths)
    print("OK: GUI, CLI and nested Mach-O runtime dependencies resolve inside the bundle.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    check(parser.parse_args().bundle)
