#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = [
    re.compile(r"wxFile(?:Input|Output)Stream\s+\w+\([^;]*(?:archivePath|target|path)\.string\(\)"),
    re.compile(r"wxFFileInputStream\s+\w+\([^;]*(?:archivePath|target|path)\.string\(\)"),
    re.compile(r"wxFileName::Mkdir\([^;]*(?:target|path)\.string\(\)"),
]


def main() -> int:
    failures: list[str] = []
    for base in [ROOT / "core", ROOT / "tests"]:
        for path in base.rglob("*.cpp"):
            text = path.read_text(encoding="utf-8", errors="ignore")
            for pattern in PATTERNS:
                if pattern.search(text):
                    failures.append(str(path.relative_to(ROOT)))
                    break
    if failures:
        print("wxWidgets filesystem APIs must use WxPathUtils at audited path boundaries:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
