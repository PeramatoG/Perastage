#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CMAKE = ROOT / "tests/CMakeLists.txt"
DIRECT_SHELL_TEST = re.compile(r"add_test\s*\([^)]*COMMAND\s+[^)]*\.sh(?:\s|\))", re.DOTALL)


def main() -> int:
    text = CMAKE.read_text(encoding="utf-8")
    matches = DIRECT_SHELL_TEST.findall(text)
    if matches:
        print("Shell CTest registrations must use add_bash_policy_test:")
        for match in matches:
            print(match.strip())
        return 1
    print("OK: shell CTest registrations use add_bash_policy_test.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
