#!/usr/bin/env python3
"""Validate cache-compatible MSVC debug flags in a CMake compile database."""
from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path

EMBEDDED_FLAGS = {"/Z7", "-Z7"}
PROGRAM_DATABASE_FLAGS = {"/Zi", "-Zi"}
EDIT_AND_CONTINUE_FLAGS = {"/ZI", "-ZI"}
DEBUG_INFORMATION_FLAGS = EMBEDDED_FLAGS | PROGRAM_DATABASE_FLAGS | EDIT_AND_CONTINUE_FLAGS
C_EXTENSIONS = {".c"}
CXX_EXTENSIONS = {".cc", ".cp", ".cpp", ".cxx", ".c++", ".cppm", ".ixx"}
RESOURCE_EXTENSIONS = {".rc"}
MSVC_COMPILERS = {"cl.exe"}
RESOURCE_COMPILERS = {"rc.exe"}


def tokens(entry: dict) -> list[str]:
    arguments = entry.get("arguments")
    if isinstance(arguments, list) and all(isinstance(item, str) for item in arguments):
        return arguments
    command = entry.get("command")
    if isinstance(command, str):
        return [item.strip('"') for item in shlex.split(command, posix=False)]
    raise ValueError("compile entry must contain a string command or string arguments array")


def validate(path: Path, sample_limit: int = 5) -> tuple[str, bool]:
    try:
        entries = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read compile database {path}: {error}") from error
    if not isinstance(entries, list):
        raise ValueError("compile database must contain a JSON array")
    counts = {"c": 0, "cxx": 0, "resource": 0, "unexpected": 0,
              "z7": 0, "zi": 0, "zI": 0, "missing": 0}
    offenders: list[str] = []
    unexpected: list[str] = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ValueError(f"compile entry {index} must be an object")
        source = str(entry.get("file", "<unknown>"))
        suffix = Path(source).suffix.lower()
        flags = tokens(entry)
        if not flags:
            raise ValueError(f"compile entry {index} has an empty command")
        executable = Path(flags[0].replace("\\", "/")).name.lower()
        if suffix in C_EXTENSIONS:
            language = "C"
            counts["c"] += 1
        elif suffix in CXX_EXTENSIONS:
            language = "C++"
            counts["cxx"] += 1
        elif suffix in RESOURCE_EXTENSIONS:
            if executable in RESOURCE_COMPILERS:
                counts["resource"] += 1
                continue
            language = "unexpected"
            reason = "resource source is not compiled by rc.exe"
        else:
            language = "unexpected"
            reason = "unsupported source extension"
        if language != "unexpected" and executable not in MSVC_COMPILERS:
            language = "unexpected"
            reason = "C/C++ source is not compiled by cl.exe"
            if suffix in C_EXTENSIONS:
                counts["c"] -= 1
            else:
                counts["cxx"] -= 1
        if language == "unexpected":
            counts["unexpected"] += 1
            if len(unexpected) < sample_limit:
                unexpected.append(f"- {source}: suffix={suffix or '<none>'}, executable={executable or '<missing>'}, reason={reason}")
            continue
        has_z7 = any(flag in EMBEDDED_FLAGS for flag in flags)
        has_zi = any(flag in PROGRAM_DATABASE_FLAGS for flag in flags)
        has_zI = any(flag in EDIT_AND_CONTINUE_FLAGS for flag in flags)
        counts["z7"] += int(has_z7)
        counts["zi"] += int(has_zi)
        counts["zI"] += int(has_zI)
        counts["missing"] += int(not has_z7)
        if (has_zi or has_zI or not has_z7) and len(offenders) < sample_limit:
            found = [flag for flag in flags if flag in DEBUG_INFORMATION_FLAGS]
            offenders.append(f"- {source}: {', '.join(found) if found else '<no accepted debug flag>'}")
    report = "\n".join([
        f"Total compile database entries: {len(entries)}", f"Validated C entries: {counts['c']}",
        f"Validated C++ entries: {counts['cxx']}",
        f"Windows resource entries excluded from Z7 validation: {counts['resource']}",
        f"Unexpected entries: {counts['unexpected']}",
        f"C/C++ entries with embedded debug information (Z7): {counts['z7']}",
        f"Offending Program Database entries (Zi): {counts['zi']}",
        f"Offending Edit-and-Continue entries (ZI): {counts['zI']}",
        f"C/C++ entries missing embedded debug information: {counts['missing']}",
        "Offending sample:", *(offenders or ["- none"]), "",
        "Unexpected sample:", *(unexpected or ["- none"]), "",
    ])
    cxx_entries = counts["c"] + counts["cxx"]
    return report, (cxx_entries > 0 and counts["unexpected"] == 0 and counts["zi"] == 0
                    and counts["zI"] == 0 and counts["missing"] == 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report, valid = validate(args.compile_commands)
    except ValueError as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    if not valid:
        raise SystemExit("MSVC compile commands must use Z7 embedded debug information and must not use Zi or ZI.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
