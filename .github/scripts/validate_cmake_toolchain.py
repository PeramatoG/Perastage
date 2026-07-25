#!/usr/bin/env python3
"""Validate CMake toolchain metadata without relying on one cache layout."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore") if path.exists() else ""


def cache_values(cache: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in cache.splitlines():
        if not line or line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key = key_type.split(":", 1)[0]
        values[key] = value
    return values


def cmake_set_value(text: str, name: str) -> str | None:
    match = re.search(rf'^\s*set\s*\(\s*{re.escape(name)}\s+"?([^"\)]+)"?\s*\)', text, re.M)
    return match.group(1).strip() if match else None


def compiler_metadata(build_dir: Path, language: str) -> dict[str, str]:
    cache = cache_values(read(build_dir / "CMakeCache.txt"))
    prefix = "CMAKE_CXX" if language == "CXX" else "CMAKE_C"
    metadata = {
        "compiler": cache.get(f"{prefix}_COMPILER", ""),
        "id": cache.get(f"{prefix}_COMPILER_ID", ""),
        "architecture": cache.get(f"{prefix}_COMPILER_ARCHITECTURE_ID", ""),
    }
    pattern = "CMakeCXXCompiler.cmake" if language == "CXX" else "CMakeCCompiler.cmake"
    for cmake_file in (build_dir / "CMakeFiles").glob(f"*/{pattern}"):
        text = read(cmake_file)
        metadata["compiler"] = metadata["compiler"] or cmake_set_value(text, f"{prefix}_COMPILER") or ""
        metadata["id"] = metadata["id"] or cmake_set_value(text, f"{prefix}_COMPILER_ID") or ""
        metadata["architecture"] = metadata["architecture"] or cmake_set_value(text, f"{prefix}_COMPILER_ARCHITECTURE_ID") or ""
    if not metadata["compiler"]:
        commands = build_dir / "compile_commands.json"
        if commands.exists():
            try:
                entries = json.loads(read(commands))
                if entries:
                    metadata["compiler"] = str(entries[0].get("command", "")).split()[0]
            except json.JSONDecodeError:
                pass
    return metadata


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate(args: argparse.Namespace) -> None:
    build_dir = Path(args.build_dir)
    cache = cache_values(read(build_dir / "CMakeCache.txt"))
    require(cache, f"CMakeCache.txt was not found or could not be read in {build_dir}")
    c = compiler_metadata(build_dir, "C")
    cxx = compiler_metadata(build_dir, "CXX")
    require(c["id"] == args.expected_c_id, f"Expected C compiler ID {args.expected_c_id}, found {c['id'] or '<missing>'}.")
    require(cxx["id"] == args.expected_cxx_id, f"Expected C++ compiler ID {args.expected_cxx_id}, found {cxx['id'] or '<missing>'}.")
    paths = [c["compiler"], cxx["compiler"]]
    if args.expected_compiler_path_regex:
        expected = re.compile(args.expected_compiler_path_regex, re.I)
        for path in paths:
            require(expected.search(path.replace("\\", "/")), f"Compiler path does not match expected pattern: {path}")
    if args.forbidden_compiler_path_regex:
        forbidden = re.compile(args.forbidden_compiler_path_regex, re.I)
        for path in paths:
            require(not forbidden.search(path.replace("\\", "/")), f"Compiler path matches forbidden pattern: {path}")
    if args.expected_architecture:
        for item in (c, cxx):
            require(item["architecture"] == args.expected_architecture, f"Expected architecture {args.expected_architecture}, found {item['architecture'] or '<missing>'}.")
    if args.expected_generator:
        require(cache.get("CMAKE_GENERATOR") == args.expected_generator, f"Expected generator {args.expected_generator}, found {cache.get('CMAKE_GENERATOR', '<missing>')}.")
    if args.expected_build_type:
        require(cache.get("CMAKE_BUILD_TYPE") == args.expected_build_type, f"Expected build type {args.expected_build_type}, found {cache.get('CMAKE_BUILD_TYPE', '<missing>')}.")
    if args.expected_launcher:
        expected_launcher = str(Path(args.expected_launcher).resolve()).replace("\\", "/").lower()
        require(cache.get("PERASTAGE_ENABLE_COMPILER_CACHE") == "ON", "PERASTAGE_ENABLE_COMPILER_CACHE must be ON.")
        selected = cache.get("PERASTAGE_COMPILER_CACHE_PROGRAM", "").replace("\\", "/").lower()
        require(selected == expected_launcher, f"Expected compiler cache program {expected_launcher}, found {selected or '<missing>'}.")
        for language in ("C", "CXX"):
            launcher = cache.get(f"CMAKE_{language}_COMPILER_LAUNCHER", "").replace("\\", "/").lower()
            require(launcher == expected_launcher, f"Expected {language} compiler launcher {expected_launcher}, found {launcher or '<missing>'}.")
    print(f"OK: CMake toolchain uses {c['id']} and {cxx['id']} with generator {cache.get('CMAKE_GENERATOR', '<unknown>')}.")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--expected-c-id", required=True)
    parser.add_argument("--expected-cxx-id", required=True)
    parser.add_argument("--expected-compiler-path-regex")
    parser.add_argument("--forbidden-compiler-path-regex")
    parser.add_argument("--expected-architecture")
    parser.add_argument("--expected-generator")
    parser.add_argument("--expected-build-type")
    parser.add_argument("--expected-launcher")
    validate(parser.parse_args())
    return 0


if __name__ == "__main__":
    sys.exit(main())
