#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys

PROTECTED = [
    ".github/workflows/windows-installer.yml",
    ".github/workflows/linux-installer.yml",
    ".github/workflows/macos-installer.yml",
    ".github/workflows/macos-15-manual-installer.yml",
    ".github/workflows/arch-package.yml",
    ".github/workflows/main-patch-test-build.yml",
    ".github/workflows/compatibility-builds.yml",
    ".github/workflows/minor-draft-release.yml",
    ".github/release-artifact-contract.json",
]


def main() -> int:
    changed = subprocess.check_output(["git", "diff", "--name-only", "HEAD", "--", *PROTECTED], text=True).splitlines()
    if changed:
        print("Protected Release workflow or artifact contract files changed:")
        for path in changed:
            print(f"  {path}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
