#!/usr/bin/env python3
"""Validate fixed Git checkout identities for dependency orchestration."""

from __future__ import annotations

import subprocess
from pathlib import Path


def checkout_head(checkout: Path) -> str:
    """Return the exact commit checked out at a registry path."""
    result = subprocess.run(
        ["git", "-C", str(checkout), "rev-parse", "HEAD"],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or "git could not read HEAD"
        raise ValueError(f"vcpkg checkout at {checkout} is not a readable Git checkout: {detail}")
    return result.stdout.strip()


def require_checkout_head(checkout: Path, expected: str, label: str) -> None:
    """Reject a checkout whose identity differs from its claimed baseline."""
    actual = checkout_head(checkout)
    if actual != expected:
        raise ValueError(
            f"{label} vcpkg checkout HEAD is {actual}; expected baseline {expected}. "
            "Check out the expected commit and retry."
        )
