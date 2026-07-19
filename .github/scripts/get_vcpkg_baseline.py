#!/usr/bin/env python3
"""Print the vcpkg builtin-baseline from a manifest and validate its shape."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

BASELINE_RE = re.compile(r"^[0-9a-fA-F]{40}$")


def read_baseline(manifest: Path) -> str:
    with manifest.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    baseline = data.get("builtin-baseline")
    if not isinstance(baseline, str) or not BASELINE_RE.fullmatch(baseline):
        raise ValueError(f"{manifest} must contain a 40-character builtin-baseline")
    return baseline.lower()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path, nargs="?", default=Path("vcpkg.json"))
    parser.add_argument("--github-output", type=Path, help="Optional GITHUB_OUTPUT file to receive baseline=<value>.")
    args = parser.parse_args(argv)
    try:
        baseline = read_baseline(args.manifest)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(baseline)
    if args.github_output is not None:
        with args.github_output.open("a", encoding="utf-8") as handle:
            handle.write(f"baseline={baseline}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
