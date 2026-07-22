#!/usr/bin/env python3
"""Diagnose restored macOS vcpkg SDK metadata and purge stale Debug caches."""
from __future__ import annotations

import argparse
import os
import re
import shutil
from pathlib import Path

SDK_PATTERN = re.compile(r"/Applications/[^\s\"'<>]*Xcode[^\s\"'<>]*/[^\s\"'<>]*/SDKs/MacOSX(?:[0-9]+(?:\.[0-9]+)*)?\.sdk")


def _resolve_existing(path: Path) -> Path | None:
    try:
        if path.exists():
            return path.resolve()
    except OSError:
        return None
    return None


def discover_sdk_paths(roots: list[Path]) -> set[str]:
    found: set[str] = set()
    for root in roots:
        if not root.is_dir():
            continue
        for current, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in {".git", "downloads"}]
            for name in files:
                path = Path(current) / name
                try:
                    text = path.read_text(encoding="utf-8", errors="ignore")
                except OSError:
                    continue
                found.update(SDK_PATTERN.findall(text))
    return found


def classify_sdk_paths(current_sdk: Path, referenced_paths: set[str]) -> tuple[Path, list[str], list[str]]:
    current_resolved = current_sdk.resolve()
    equivalent: list[str] = []
    stale: list[str] = []
    for value in sorted(referenced_paths):
        referenced = Path(value)
        resolved = _resolve_existing(referenced)
        if resolved is not None and resolved == current_resolved:
            equivalent.append(value)
        elif referenced == current_sdk:
            equivalent.append(value)
        else:
            stale.append(value)
    return current_resolved, equivalent, stale


def purge_cache_roots(roots: list[Path]) -> None:
    for root in roots:
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--current-sdk", required=True, type=Path)
    parser.add_argument("--scan-root", action="append", default=[], type=Path)
    parser.add_argument("--purge-root", action="append", default=[], type=Path)
    args = parser.parse_args()

    referenced = discover_sdk_paths(args.scan_root)
    current_resolved, equivalent, stale = classify_sdk_paths(args.current_sdk, referenced)
    print(f"Current macOS SDK path: {args.current_sdk}")
    print(f"Current macOS SDK resolved path: {current_resolved}")
    if equivalent:
        print("Equivalent restored SDK metadata paths:")
        for value in equivalent:
            print(f"  {value}")
    if not stale:
        print("No stale macOS SDK metadata found in restored vcpkg caches.")
        return 0
    print("Stale macOS SDK metadata found in restored vcpkg caches:")
    for value in stale:
        print(f"  {value}")
    print("Purging ABI-sensitive restored Debug vcpkg caches before reinstall.")
    purge_cache_roots(args.purge_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
