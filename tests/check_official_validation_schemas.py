#!/usr/bin/env python3
"""Verify pinned official validation schemas and their single CMake owner."""

from __future__ import annotations

import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
REVISION = "e199c6ed635de23cb5ebf9654ee54a358775a065"
SCHEMAS = {
    Path("resources/standards/gdtf/1.2/gdtf.xsd"): "13a044d297f19d0437965657fb21d02f7b2b02541aabeba5f11e5000074ac2f2",
    Path("resources/standards/mvr/1.6/mvr.xsd"): "85bc42015b706b2ab2bcc2ce69649e7245272b3606217263c90126cda21cb86c",
}


def fail(message: str) -> None:
    """Report one integrity failure and stop."""
    raise SystemExit(f"Official validation schema check failed: {message}")


def main() -> None:
    """Validate schema hashes, provenance pins, and CMake embedding paths."""
    cmake = (ROOT / "core/CMakeLists.txt").read_text(encoding="utf-8")
    discovered = set((ROOT / "resources/standards").rglob("*.xsd"))
    expected = {ROOT / path for path in SCHEMAS}
    if discovered != expected:
        fail("resources/standards must contain only the two pinned official XSD files")
    for relative, expected_hash in SCHEMAS.items():
        path = ROOT / relative
        actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual_hash != expected_hash:
            fail(f"SHA-256 mismatch for {relative}")
        provenance = (path.parent / "PROVENANCE.md").read_text(encoding="utf-8")
        if REVISION not in provenance or expected_hash not in provenance:
            fail(f"missing revision or SHA-256 provenance for {relative}")
        cmake_path = relative.as_posix()
        if cmake.count(cmake_path) != 1:
            fail(f"CMake must embed {cmake_path} exactly once")
    print("Official validation schema integrity check passed")


if __name__ == "__main__":
    main()
