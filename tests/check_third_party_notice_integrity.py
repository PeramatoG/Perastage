#!/usr/bin/env python3
"""Validate curated third-party inventory coverage and linked legal texts."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NOTICE = ROOT / "THIRD_PARTY_NOTICES.md"
HOST_ONLY_DEPENDENCIES = {"gettext"}
BUNDLED_COMPONENTS = {"nlohmann/json", "stb_easy_font", "noto sans", "lucide"}


def dependency_name(entry: object) -> str:
    """Return a vcpkg dependency name from string or object syntax."""
    return entry if isinstance(entry, str) else str(entry["name"])


def main() -> int:
    """Reject broken notice links, known mislabeling, and inventory omissions."""
    text = NOTICE.read_text(encoding="utf-8")
    lowered = text.lower()
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

    nanovg_row = next((line for line in text.splitlines() if "NanoVG" in line), "")
    if not nanovg_row or "MIT" in nanovg_row or "Zlib" not in nanovg_row:
        errors.append("NanoVG must be represented as Zlib, not MIT")

    manifest = json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8"))
    runtime_dependencies = {
        dependency_name(entry).lower()
        for entry in manifest["dependencies"]
        if dependency_name(entry).lower() not in HOST_ONLY_DEPENDENCIES
    }
    for dependency in sorted(runtime_dependencies):
        display_name = "libcurl" if dependency == "curl" else dependency
        if display_name not in lowered:
            errors.append(f"direct runtime dependency is absent from notices: {dependency}")
    for component in sorted(BUNDLED_COMPONENTS):
        if component not in lowered:
            errors.append(f"bundled source or asset is absent from notices: {component}")

    if errors:
        print("Third-party notice integrity check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Third-party notice integrity check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
