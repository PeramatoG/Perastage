#!/usr/bin/env python3
"""Validate cache-compatible MSVC debug flags in a CMake compile database."""
from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path


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
    counts = {"c": 0, "cxx": 0, "z7": 0, "zi": 0, "zI": 0, "missing": 0}
    offenders: list[str] = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ValueError(f"compile entry {index} must be an object")
        source = str(entry.get("file", "<unknown>"))
        suffix = Path(source).suffix.lower()
        if suffix == ".c":
            counts["c"] += 1
        else:
            counts["cxx"] += 1
        flags = tokens(entry)
        has_z7 = "/Z7" in flags
        has_zi = "/Zi" in flags
        has_zI = "/ZI" in flags
        counts["z7"] += int(has_z7)
        counts["zi"] += int(has_zi)
        counts["zI"] += int(has_zI)
        counts["missing"] += int(not has_z7)
        if (has_zi or has_zI or not has_z7) and len(offenders) < sample_limit:
            found = [flag for flag in flags if flag in {"/Z7", "/Zi", "/ZI"}]
            offenders.append(f"- {source}: {', '.join(found) if found else '<no accepted debug flag>'}")
    report = "\n".join([
        f"Total entries: {len(entries)}", f"C entries: {counts['c']}", f"C++ entries: {counts['cxx']}",
        f"Entries with /Z7: {counts['z7']}", f"Offending /Zi entries: {counts['zi']}",
        f"Offending /ZI entries: {counts['zI']}", f"Entries missing /Z7: {counts['missing']}",
        "Offending sample:", *(offenders or ["- none"]), "",
    ])
    return report, counts["zi"] == 0 and counts["zI"] == 0 and counts["missing"] == 0 and bool(entries)


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
        raise SystemExit("MSVC compile commands must use /Z7 and must not use /Zi or /ZI.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
