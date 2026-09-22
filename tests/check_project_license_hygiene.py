#!/usr/bin/env python3
"""Validate the single project-license candidate and or-later metadata."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANONICAL_GPLV3_SHA256 = "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986"


def normalize_newlines(text: str) -> str:
    """Normalize conventional text-file line endings to LF."""
    return text.replace("\r\n", "\n").replace("\r", "\n")


def main() -> int:
    """Reject modified GPL text, competing root candidates, and metadata drift."""
    errors: list[str] = []
    license_path = ROOT / "LICENSE.txt"
    notice_path = ROOT / "THIRD_PARTY_NOTICES.md"
    if not license_path.is_file():
        errors.append("LICENSE.txt is missing")
        license_text = ""
    else:
        license_text = normalize_newlines(license_path.read_bytes().decode("utf-8"))
        # Hash newline-normalized text so canonical content is checked identically on every platform.
        actual_digest = hashlib.sha256(license_text.encode("utf-8")).hexdigest()
        if actual_digest != CANONICAL_GPLV3_SHA256:
            errors.append(
                "LICENSE.txt does not match the canonical GPLv3 text: "
                f"expected SHA-256 {CANONICAL_GPLV3_SHA256}, got {actual_digest}"
            )
    obsolete_notice = ROOT / ("THIRD_PARTY_" + "LICENSES.md")
    if obsolete_notice.exists():
        errors.append(f"obsolete {obsolete_notice.name} still exists")
    if not notice_path.is_file():
        errors.append("THIRD_PARTY_NOTICES.md is missing")

    if "software and other kinds of works." not in license_text:
        errors.append("LICENSE.txt is missing the canonical GPLv3 sentence")
    if "including the Perastage project" in license_text:
        errors.append("LICENSE.txt contains the former project-specific edit")
    if not license_text.startswith("                    GNU GENERAL PUBLIC LICENSE\n"):
        errors.append("LICENSE.txt has a project title or copyright preamble")

    candidate = re.compile(r"^(?:LICENSE|LICENCE|COPYING|COPYRIGHT|PATENTS|UNLICENSE|OFL)(?:[._-].*)?$", re.I)
    candidates = sorted(path.name for path in ROOT.iterdir() if path.is_file() and candidate.match(path.name))
    if candidates != ["LICENSE.txt"]:
        errors.append(f"root project-license candidates are {candidates!r}, expected ['LICENSE.txt']")

    expression = "GPL-3.0-or-later"
    for relative in ("README.md", "CONTRIBUTING.md"):
        if expression not in (ROOT / relative).read_text(encoding="utf-8"):
            errors.append(f"{relative} does not declare {expression}")
    manifest = json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8"))
    if manifest.get("license") != expression:
        errors.append(f"vcpkg.json license must be {expression}")

    if errors:
        print("Project license hygiene check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Project license hygiene check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
