#!/usr/bin/env python3
"""Validate restored macOS vcpkg SDK metadata and purge incompatible caches."""
from __future__ import annotations

import argparse
import os
import re
import shutil
from pathlib import Path

# Match the SDK root only. In particular, metadata separators and a later
# /Applications reference cannot become part of the same candidate.
SDK_PATTERN = re.compile(
    r"/Applications/"
    r"(?:(?!/Applications/)[^\x00;\r\n\"'<>])*?Xcode"
    r"(?:(?!/Applications/)[^\x00;\r\n\"'<>])*?\.app/Contents/Developer/"
    r"Platforms/MacOSX\.platform/Developer/SDKs/"
    r"MacOSX(?:[0-9]+(?:\.[0-9]+)*)?\.sdk"
)

TEXT_METADATA_SUFFIXES = frozenset({".cmake", ".pc", ".la"})
MAX_METADATA_BYTES = 4 * 1024 * 1024
SdkReferences = dict[str, set[Path]]


def _resolve_existing(path: Path) -> Path | None:
    try:
        if path.exists():
            return path.resolve()
    except OSError:
        return None
    return None


def extract_sdk_paths(text: str) -> set[str]:
    """Extract complete SDK roots from textual build metadata."""
    return set(SDK_PATTERN.findall(text))


def _is_build_metadata(path: Path) -> bool:
    """Select vcpkg metadata consumed by CMake, pkg-config, or config scripts."""
    if path.suffix.lower() in TEXT_METADATA_SUFFIXES:
        return True
    return path.name.endswith("-config") and path.suffix == ""


def discover_sdk_paths(roots: list[Path]) -> SdkReferences:
    """Discover SDK references and retain the metadata file that supplied each one."""
    found: SdkReferences = {}
    for root in roots:
        if not root.is_dir():
            continue
        for current, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in {".git", "downloads", "doc", "docs"}]
            for name in files:
                path = Path(current) / name
                if not _is_build_metadata(path):
                    continue
                try:
                    if path.stat().st_size > MAX_METADATA_BYTES:
                        continue
                    text = path.read_text(encoding="utf-8")
                except (OSError, UnicodeError):
                    continue
                for sdk_path in extract_sdk_paths(text):
                    found.setdefault(sdk_path, set()).add(path)
    return found


def classify_sdk_paths(
    current_sdk: Path, referenced_paths: SdkReferences | set[str]
) -> tuple[Path, list[str], list[str]]:
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


def _write_outputs(result: str, reason: str, source: str = "") -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if not output_path:
        return
    with Path(output_path).open("a", encoding="utf-8") as output:
        output.write(f"guard-result={result}\n")
        output.write(f"guard-reason={reason}\n")
        output.write(f"invalidation-source={source}\n")


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
        reason = "compatible SDK metadata" if referenced else "no active SDK metadata found"
        print("No stale macOS SDK metadata found in restored vcpkg caches.")
        _write_outputs("retained", reason)
        return 0
    print("Stale macOS SDK metadata found in restored vcpkg caches:")
    for value in stale:
        sources = sorted(str(path) for path in referenced.get(value, set()))
        print(f"  {value}")
        for source in sources:
            print(f"    source: {source}")
    print("Purging ABI-sensitive restored Debug vcpkg caches before reinstall.")
    purge_cache_roots(args.purge_root)
    first_source = sorted(str(path) for path in referenced.get(stale[0], set()))
    _write_outputs("invalidated", "incompatible SDK metadata", first_source[0] if first_source else "")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
