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


def cache_text(
    launcher: str,
    *,
    c_compiler: str | None = None,
    cxx_compiler: str | None = None,
    bash_executable: str | None = None,
    policy_default_cmp0141: str | None = None,
    msvc_debug_information_format: str | None = None,
) -> str:
    entries: list[tuple[str, str, str]] = []
    optional_entries = (
        ("CMAKE_C_COMPILER", c_compiler, "FILEPATH"),
        ("CMAKE_CXX_COMPILER", cxx_compiler, "FILEPATH"),
        ("BASH_EXECUTABLE", bash_executable, "FILEPATH"),
    )
    entries.extend((name, value, kind) for name, value, kind in optional_entries if value is not None)
    entries.extend((name, launcher, kind) for name, kind in (
        ("PERASTAGE_COMPILER_CACHE_PROGRAM", "FILEPATH"),
        ("CMAKE_C_COMPILER_LAUNCHER", "STRING"),
        ("CMAKE_CXX_COMPILER_LAUNCHER", "STRING"),
    ))
    if policy_default_cmp0141 is not None:
        entries.append(("CMAKE_POLICY_DEFAULT_CMP0141", policy_default_cmp0141, "STRING"))
    if msvc_debug_information_format is not None:
        entries.append(("CMAKE_MSVC_DEBUG_INFORMATION_FORMAT", msvc_debug_information_format, "STRING"))
    return "".join(f'set({name} {bracket_quote(value)} CACHE {kind} "" FORCE)\n' for name, value, kind in entries)


def exact_executable(parser: argparse.ArgumentParser, value: str, label: str) -> str:
    path = Path(value).expanduser().resolve()
    if not path.is_file():
        parser.error(f"{label} executable does not exist: {path}")
    return str(path).replace("\\", "/")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--c-compiler")
    parser.add_argument("--cxx-compiler")
    parser.add_argument("--bash-executable")
    parser.add_argument("--policy-default-cmp0141")
    parser.add_argument("--msvc-debug-information-format")
    args = parser.parse_args()
    launcher = exact_executable(parser, args.launcher, "launcher")
    c_compiler = exact_executable(parser, args.c_compiler, "C compiler") if args.c_compiler else None
    cxx_compiler = exact_executable(parser, args.cxx_compiler, "C++ compiler") if args.cxx_compiler else None
    bash_executable = exact_executable(parser, args.bash_executable, "Bash") if args.bash_executable else None
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(cache_text(
        launcher,
        c_compiler=c_compiler,
        cxx_compiler=cxx_compiler,
        bash_executable=bash_executable,
        policy_default_cmp0141=args.policy_default_cmp0141,
        msvc_debug_information_format=args.msvc_debug_information_format,
    ), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
