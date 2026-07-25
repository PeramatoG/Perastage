#!/usr/bin/env python3
"""Write an injection-safe CMake initial cache for one compiler launcher."""
from __future__ import annotations

import argparse
from pathlib import Path


def bracket_quote(value: str) -> str:
    for level in range(16):
        equals = "=" * level
        closing = f"]{equals}]"
        if closing not in value:
            return f"[{equals}[{value}]{equals}]"
    raise ValueError("launcher path contains unsupported CMake bracket delimiters")


def cache_text(launcher: str) -> str:
    quoted = bracket_quote(launcher)
    entries = (
        ("PERASTAGE_COMPILER_CACHE_PROGRAM", "FILEPATH"),
        ("CMAKE_C_COMPILER_LAUNCHER", "STRING"),
        ("CMAKE_CXX_COMPILER_LAUNCHER", "STRING"),
    )
    return "".join(f'set({name} {quoted} CACHE {kind} "" FORCE)\n' for name, kind in entries)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    launcher = Path(args.launcher).expanduser().resolve()
    if not launcher.is_file():
        parser.error(f"launcher executable does not exist: {launcher}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(cache_text(str(launcher)), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
