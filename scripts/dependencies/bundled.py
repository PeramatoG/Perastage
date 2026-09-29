#!/usr/bin/env python3
"""Build notify-only version messages for manually maintained bundled sources."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def nlohmann_version(header: Path) -> str:
    """Read the nlohmann/json semantic version macros from the vendored header."""
    text = header.read_text(encoding="utf-8")
    values = []
    for component in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"^#define NLOHMANN_JSON_VERSION_{component}\s+(\d+)\b", text, re.MULTILINE)
        if not match:
            raise ValueError(f"missing NLOHMANN_JSON_VERSION_{component} in {header}")
        values.append(int(match.group(1)))
    return ".".join(str(value) for value in values)


def version_tuple(version: str) -> tuple[int, int, int]:
    """Normalize a three-component release tag for deterministic comparison."""
    match = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)", version.strip())
    if not match:
        raise ValueError(f"unsupported nlohmann/json release version: {version!r}")
    return tuple(int(value) for value in match.groups())


def nlohmann_notice(vendored: str, upstream: str | None) -> str:
    """Describe bundled nlohmann/json status without changing its source."""
    if upstream is None:
        return f"nlohmann/json vendored: {vendored}; upstream lookup unavailable; replacement remains manual."
    normalized = ".".join(str(value) for value in version_tuple(upstream))
    status = "newer upstream release available" if version_tuple(normalized) > version_tuple(vendored) else "up to date"
    return f"nlohmann/json vendored: {vendored}; upstream: {normalized}; {status}; replacement remains manual."


def main() -> int:
    """Print the notify-only nlohmann/json review message."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--upstream")
    args = parser.parse_args()
    print(nlohmann_notice(nlohmann_version(args.header), args.upstream or None))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
