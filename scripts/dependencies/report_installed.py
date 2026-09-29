#!/usr/bin/env python3
"""Report installed direct vcpkg versions against canonical repository state."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def status_versions(path: Path) -> dict[str, str]:
    """Parse package versions from a vcpkg installed status file."""
    result: dict[str, str] = {}
    for paragraph in path.read_text(encoding="utf-8", errors="replace").split("\n\n"):
        fields = {}
        for line in paragraph.splitlines():
            if ": " in line:
                key, value = line.split(": ", 1)
                fields[key] = value
        if fields.get("Package") and fields.get("Version") and not fields.get("Feature"):
            revision = fields.get("Port-Version", "0")
            suffix = f"#{revision}" if revision != "0" and "#" not in fields["Version"] else ""
            result[fields["Package"]] = fields["Version"] + suffix
    return result


def main() -> int:
    """Print a compact inventory and optionally require canonical agreement."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", type=Path, default=Path("dependencies/resolved-vcpkg.json"))
    parser.add_argument("--status", type=Path, required=True)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--include-host", action="store_true")
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args()
    state = json.loads(args.state.read_text(encoding="utf-8"))
    installed = status_versions(args.status)
    lines = ["| Dependency | Role | Canonical | Installed | Result |", "| --- | --- | --- | --- | --- |"]
    mismatch = False
    for package in state["packages"]:
        if package["role"] == "host" and not args.include_host:
            continue
        canonical = package["version"] + (f"#{package['port-version']}" if package["port-version"] else "")
        actual = installed.get(package["name"], "missing")
        result = "match" if actual == canonical else "different"
        mismatch |= result != "match"
        lines.append(f"| {package['name']} | {package['role']} | {canonical} | {actual} | {result} |")
    output = "\n".join(lines) + "\n"
    print(output, end="")
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as stream:
            stream.write("\n### Direct vcpkg dependency inventory\n\n" + output)
    return 1 if args.strict and mismatch else 0


if __name__ == "__main__":
    raise SystemExit(main())
