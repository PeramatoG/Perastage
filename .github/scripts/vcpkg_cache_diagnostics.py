#!/usr/bin/env python3
"""Capture compact, non-secret vcpkg cache and ABI diagnostics."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

REPRESENTATIVE_PORTS = ("zlib", "vcpkg-cmake-config")


def _read_abi_files(packages_root: Path) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for port in REPRESENTATIVE_PORTS:
        entries: list[str] = []
        for path in sorted(packages_root.rglob("*vcpkg_abi_info.txt")):
            if port not in str(path.relative_to(packages_root)):
                continue
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()[:40]
            except OSError:
                continue
            entries.append(f"{path.relative_to(packages_root)}: " + "; ".join(lines))
        result[port] = entries
    return result


def _installed_status(installed_root: Path) -> list[str]:
    status = installed_root / "vcpkg" / "status"
    try:
        paragraphs = status.read_text(encoding="utf-8", errors="replace").split("\n\n")
    except OSError:
        return []
    return ["; ".join(line for line in paragraph.splitlines() if line.startswith(("Package:", "Version:", "Architecture:")))
            for paragraph in paragraphs if any(f"Package: {port}" in paragraph for port in REPRESENTATIVE_PORTS)]


def capture(args: argparse.Namespace) -> dict[str, object]:
    archives = sorted(path.name for path in args.binary_root.rglob("*.zip")) if args.binary_root.exists() else []
    return {
        "label": args.label,
        "triplet": args.triplet,
        "baseline": args.baseline,
        "compiler": args.compiler,
        "sdk": args.sdk,
        "vcpkg_version": args.vcpkg_version,
        "installed_representatives": _installed_status(args.installed_root),
        "abi_representatives": _read_abi_files(args.packages_root),
        "binary_archive_count": len(archives),
        "binary_archive_identifiers": archives[:20],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True)
    parser.add_argument("--installed-root", required=True, type=Path)
    parser.add_argument("--packages-root", required=True, type=Path)
    parser.add_argument("--binary-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--compiler", default="unknown")
    parser.add_argument("--sdk", default="unknown")
    parser.add_argument("--vcpkg-version", default="unknown")
    args = parser.parse_args()
    payload = capture(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    history: list[dict[str, object]] = []
    try:
        history = json.loads(args.output.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        pass
    history.append(payload)
    args.output.write_text(json.dumps(history, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
