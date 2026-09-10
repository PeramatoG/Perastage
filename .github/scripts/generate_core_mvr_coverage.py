#!/usr/bin/env python3
"""Generate informational gcovr reports scoped to Core and MVR production code."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--commit", default="local")
    parser.add_argument("--summary", type=Path)
    return parser.parse_args()


def totals(report: dict, prefix: str | None = None) -> tuple[int, int]:
    covered = total = 0
    for entry in report["files"]:
        name = entry["file"].replace("\\", "/")
        if prefix is not None and not name.startswith(prefix):
            continue
        for line in entry["lines"]:
            if line.get("gcovr/noncode"):
                continue
            total += 1
            if line["count"] > 0:
                covered += 1
    return covered, total


def percent(values: tuple[int, int]) -> str:
    covered, total = values
    return f"{covered * 100.0 / total:.2f}%" if total else "n/a"


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    base = [
        sys.executable,
        "-m",
        "gcovr",
        "--root",
        str(root),
        "--filter",
        r"^(core|mvr)/",
        "--exclude-unreachable-branches",
        "--gcov-ignore-parse-errors=negative_hits.warn_once_per_file",
        str(args.build_dir.resolve()),
    ]
    subprocess.run(
        base
        + [
            "--txt",
            str(output / "coverage.txt"),
            "--html-details",
            str(output / "index.html"),
            "--html-title",
            "Perastage Core/MVR coverage",
            "--json",
            str(output / "coverage.json"),
            "--xml-pretty",
            "--xml",
            str(output / "coverage.xml"),
        ],
        check=True,
    )

    report = json.loads((output / "coverage.json").read_text(encoding="utf-8"))
    core = totals(report, "core/")
    mvr = totals(report, "mvr/")
    combined = (core[0] + mvr[0], core[1] + mvr[1])
    gaps = sorted(
        ((totals({"files": [entry]}), entry["file"]) for entry in report["files"]),
        key=lambda item: item[0][1] - item[0][0],
        reverse=True,
    )[:5]
    summary = [
        "## Core/MVR coverage",
        "",
        f"Tested commit: `{args.commit}`",
        "",
        "| Scope | Covered / total lines | Line coverage |",
        "|---|---:|---:|",
        f"| `core/` | {core[0]} / {core[1]} | {percent(core)} |",
        f"| `mvr/` | {mvr[0]} / {mvr[1]} | {percent(mvr)} |",
        f"| Combined | {combined[0]} / {combined[1]} | {percent(combined)} |",
        "",
        "Largest uncovered files:",
    ]
    summary.extend(
        f"- `{name}`: {values[1] - values[0]} uncovered lines"
        for values, name in gaps
    )
    summary.append("\nCoverage is informational; no percentage threshold is enforced.\n")
    rendered = "\n".join(summary)
    (output / "summary.md").write_text(rendered, encoding="utf-8")
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(rendered, encoding="utf-8")
    github_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if github_summary:
        with open(github_summary, "a", encoding="utf-8") as stream:
            stream.write(rendered)
    print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
