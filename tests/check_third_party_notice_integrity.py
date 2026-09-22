#!/usr/bin/env python3
"""Validate curated third-party inventory coverage and linked legal texts."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTICE = ROOT / "THIRD_PARTY_NOTICES.md"
BUNDLED_COMPONENTS = {"nlohmann/json", "stb_easy_font", "noto sans", "lucide icons 0.562.0"}
DEPENDENCY_COMPONENT_NAMES = {"curl": "libcurl"}


def dependency_name(entry: object) -> str:
    """Return a vcpkg dependency name from string or object syntax."""
    return entry if isinstance(entry, str) else str(entry["name"])


def is_host_dependency(entry: object) -> bool:
    """Return whether an object-form manifest dependency is host-only."""
    return isinstance(entry, dict) and entry.get("host") is True


def component_entries(markdown: str) -> dict[str, str]:
    """Parse component table rows into normalized labels and complete rows."""
    entries: dict[str, str] = {}
    for line in markdown.splitlines():
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if len(cells) < 3 or cells[0] in {"Component", "---"}:
            continue
        link = re.fullmatch(r"\[([^]]+)\]\([^)]+\)(?:\s*/.*)?", cells[0])
        if link:
            entries[link.group(1).casefold()] = line
    return entries


def main() -> int:
    """Reject broken notice links, known mislabeling, and inventory omissions."""
    text = NOTICE.read_text(encoding="utf-8")
    entries = component_entries(text)
    errors: list[str] = []
    links = re.findall(r"\]\((licenses/[^)]+)\)", text)
    for link in sorted(set(links)):
        if not (ROOT / link).is_file():
            errors.append(f"linked legal text does not exist: {link}")

    obsolete = ROOT / "licenses" / "Lucide-MIT.txt"
    lucide = ROOT / "licenses" / "lucide_LICENSE.txt"
    if obsolete.exists():
        errors.append("obsolete licenses/Lucide-MIT.txt still exists")
    if not lucide.is_file():
        errors.append("licenses/lucide_LICENSE.txt is missing")
    else:
        lucide_text = lucide.read_text(encoding="utf-8")
        for marker in ("ISC License", "portions derived from Feather", "The MIT License (MIT)"):
            if marker not in lucide_text:
                errors.append(f"Lucide 0.562.0 notice is missing: {marker}")

    nanovg_row = entries.get("nanovg", "")
    if not nanovg_row or "MIT" in nanovg_row or "Zlib" not in nanovg_row:
        errors.append("NanoVG must have its own component row identifying Zlib, not MIT")

    curl_license = (ROOT / "licenses" / "curl_LICENSE.txt").read_text(encoding="utf-8")
    current_curl_marker = "Copyright (c) 1996 - 2026, Daniel Stenberg"
    if current_curl_marker not in curl_license or "Copyright (c) 1996 - 2025" in curl_license:
        errors.append("curl primary license does not match the pinned curl 8.21.0 legal text")

    manifest = json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8"))
    runtime_dependencies = {
        dependency_name(entry).casefold()
        for entry in manifest["dependencies"]
        if not is_host_dependency(entry)
    }
    for dependency in sorted(runtime_dependencies):
        component = DEPENDENCY_COMPONENT_NAMES.get(dependency, dependency)
        if component not in entries:
            errors.append(f"direct runtime dependency has no component row: {dependency}")
    for component in sorted(BUNDLED_COMPONENTS):
        if component not in entries:
            errors.append(f"bundled source or asset has no component row: {component}")

    if errors:
        print("Third-party notice integrity check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Third-party notice integrity check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
