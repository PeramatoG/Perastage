#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/ci-tests.yml"


def require(text: str, needle: str, message: str) -> bool:
    if needle in text:
        return True
    print(message)
    return False


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    ok = True
    ok &= require(text, "Prepare Windows Debug test tools", "Windows Debug must prepare test tools before Bash validation.")
    ok &= require(text, "Get-Command rg.exe", "Windows Debug must resolve ripgrep in PowerShell.")
    ok &= require(text, "choco.exe", "Windows Debug must install ripgrep when the hosted runner lacks it.")
    ok &= require(text, "windows-test-tools.txt", "Windows Debug must log resolved test tools.")
    ok &= require(text, "--noprofile --norc -c 'printf", "Git Bash execution probe must use a non-login shell.")
    ok &= require(text, "--noprofile --norc -c 'command -v rg && rg --version'", "Git Bash ripgrep probe must be separate from Bash execution.")
    ok &= require(text, "-DBASH_EXECUTABLE=\"$env:PERASTAGE_GIT_BASH\"", "Windows CMake configure must receive the validated Git Bash path.")
    if "bash -lc 'printf \"git-bash-ok" in text or "bash -lc 'printf" in text:
        print("Windows Debug Git Bash probes must not use login-shell -lc mode.")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
