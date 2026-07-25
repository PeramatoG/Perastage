#!/usr/bin/env python3
"""Validate sccache JSON statistics and write a concise CI step summary."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Any


class StatisticsError(ValueError):
    """Report statistics that cannot safely be interpreted."""


def _non_negative_int(value: Any, field: str, *, optional: bool = False) -> int | None:
    if value is None and optional:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise StatisticsError(f"{field} must be a non-negative integer")
    return value


def _language_total(value: Any, field: str) -> int:
    if not isinstance(value, dict):
        raise StatisticsError(f"{field} must be an object")
    counts = value.get("counts", {})
    if not isinstance(counts, dict):
        raise StatisticsError(f"{field}.counts must be an object")
    return sum(_non_negative_int(item, f"{field}.counts.{name}") or 0 for name, item in counts.items())


def parse_statistics(path: Path) -> dict[str, int | str | None]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise StatisticsError(f"cannot read valid sccache JSON from {path}: {error}") from error
    if not isinstance(payload, dict) or not isinstance(payload.get("stats"), dict):
        raise StatisticsError("sccache JSON must contain a stats object")
    stats = payload["stats"]
    requests = _non_negative_int(stats.get("compile_requests"), "stats.compile_requests")
    hits = _language_total(stats.get("cache_hits"), "stats.cache_hits")
    misses = _language_total(stats.get("cache_misses"), "stats.cache_misses")
    errors = _language_total(stats.get("cache_errors"), "stats.cache_errors")
    errors += _non_negative_int(stats.get("cache_read_errors"), "stats.cache_read_errors", optional=True) or 0
    errors += _non_negative_int(stats.get("cache_write_errors"), "stats.cache_write_errors", optional=True) or 0
    cacheable = hits + misses + errors
    return {
        "version": str(payload.get("version") or "not reported"),
        "backend": str(payload.get("cache_location") or "not reported"),
        "requests": requests,
        "cacheable": cacheable,
        "hits": hits,
        "misses": misses,
        "non_cacheable": _non_negative_int(
            stats.get("non_cacheable_compilations"), "stats.non_cacheable_compilations", optional=True
        ),
        "errors": errors,
    }


def compile_command_counts(path: Path) -> tuple[int, int]:
    try:
        entries = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise StatisticsError(f"cannot read compile commands from {path}: {error}") from error
    if not isinstance(entries, list):
        raise StatisticsError("compile_commands.json must contain an array")
    c_count = 0
    cxx_count = 0
    for entry in entries:
        source = str(entry.get("file", "")) if isinstance(entry, dict) else ""
        suffix = Path(source).suffix.lower()
        if suffix == ".c":
            c_count += 1
        elif suffix in {".cc", ".cpp", ".cxx", ".c++", ".cp", ".mm"}:
            cxx_count += 1
    return c_count, cxx_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stats-json", type=Path, required=True)
    parser.add_argument("--compile-commands", type=Path, required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--mode", choices=("READ_ONLY", "READ_WRITE"), required=True)
    parser.add_argument("--namespace", required=True)
    parser.add_argument("--compiler-identity", required=True)
    parser.add_argument("--base-directory", type=Path, required=True)
    parser.add_argument("--launcher-validation", choices=("passed", "failed"), required=True)
    parser.add_argument("--build-succeeded", choices=("true", "false"), required=True)
    args = parser.parse_args()

    if not args.base_directory.is_absolute():
        raise StatisticsError("sccache base directory must be absolute")
    values = parse_statistics(args.stats_json)
    try:
        c_requests, cxx_requests = compile_command_counts(args.compile_commands)
    except StatisticsError:
        if args.build_succeeded == "true":
            raise
        c_requests, cxx_requests = None, None
    denominator = int(values["hits"]) + int(values["misses"])
    hit_rate = f"{100 * int(values['hits']) / denominator:.1f}%" if denominator else "not applicable"
    optional = lambda value: "not reported" if value is None else str(value)
    lines = [
        f"## sccache summary: {args.platform}", "",
        f"- Platform: {args.platform}", f"- Runner architecture: {args.architecture}",
        f"- sccache version: {values['version']}", f"- Backend: {values['backend']}",
        f"- Read/write mode: {args.mode}", f"- Namespace/schema: `{args.namespace}`",
        f"- Compiler identity: {args.compiler_identity}", f"- Workspace base directory: `{args.base_directory}`",
        f"- C compile commands: {optional(c_requests)}", f"- C++ compile commands: {optional(cxx_requests)}",
        f"- Total compile requests: {values['requests']}", f"- Cacheable compilations: {values['cacheable']}",
        f"- Cache hits: {values['hits']}", f"- Cache misses: {values['misses']}",
        f"- Non-cacheable compilations: {optional(values['non_cacheable'])}",
        f"- Cache errors: {values['errors']}", f"- Hit rate: {hit_rate}",
        f"- Launcher validation: {args.launcher_validation}", f"- Build succeeded: {args.build_succeeded}", "",
    ]
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with Path(summary).open("a", encoding="utf-8") as output:
            output.write("\n".join(lines))
    else:
        print("\n".join(lines))
    if args.build_succeeded == "true" and values["requests"] == 0:
        raise StatisticsError("the build succeeded but sccache recorded zero compiler requests; verify the CMake launcher")
    if args.build_succeeded == "true" and args.launcher_validation != "passed":
        raise StatisticsError("the configured compiler launcher was not validated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
