#!/usr/bin/env python3
"""Run vcpkg install with bounded retries for transient network failures."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

TRANSIENT_PATTERNS = [
    re.compile(r"\b(?:HTTP|response code)\s*(?:408|425|429|500|502|503|504)\b", re.I),
    re.compile(r"curl.*(?:timed? out|timeout|operation timed out|couldn't connect|connection.*(?:reset|refused)|temporarily unavailable)", re.I),
    re.compile(r"(?:could not|couldn't|failed to) resolve (?:host|hostname)", re.I),
    re.compile(r"(?:connection (?:reset|refused)|network is unreachable|temporarily unavailable|temporary failure|proxy.*temporary)", re.I),
    re.compile(r"(?:TLS|SSL) connection timeout", re.I),
]
PERMANENT_PATTERNS = [
    re.compile(r"\b(?:configure|configuration|compil(?:e|ation)|link(?:er|ing)?|patch|ABI|manifest|validation) (?:error|failed|failure)\b", re.I),
    re.compile(r"\berror C\d{4}\b|undefined reference|No such file or directory", re.I),
]

@dataclass(frozen=True)
class Classification:
    transient: bool
    reason: str


def classify_failure(output: str) -> Classification:
    for pattern in TRANSIENT_PATTERNS:
        match = pattern.search(output)
        if match:
            return Classification(True, match.group(0))
    for pattern in PERMANENT_PATTERNS:
        match = pattern.search(output)
        if match:
            return Classification(False, match.group(0))
    return Classification(False, "no transient network/download signature found")


def build_command(args: argparse.Namespace) -> list[str]:
    return [
        str(args.vcpkg), "install", "--triplet", args.triplet,
        f"--x-manifest-root={args.manifest_root}",
        f"--x-install-root={args.install_root}",
        f"--x-packages-root={args.packages_root}",
        f"--downloads-root={args.downloads_root}",
    ] + list(args.extra_args)


def run_attempt(command: list[str], log_path: Path, attempt: int) -> tuple[int, str]:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    collected: list[str] = []
    with log_path.open("a", encoding="utf-8", errors="replace") as log:
        header = f"\n=== vcpkg install attempt {attempt} ===\nCommand: {' '.join(command)}\n"
        print(header, end="")
        log.write(header)
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            log.write(line)
            collected.append(line)
        return_code = process.wait()
        footer = f"=== attempt {attempt} exit code: {return_code} ===\n"
        print(footer, end="")
        log.write(footer)
    return return_code, "".join(collected)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg", required=True, type=Path)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--manifest-root", required=True, type=Path)
    parser.add_argument("--install-root", required=True, type=Path)
    parser.add_argument("--packages-root", required=True, type=Path)
    parser.add_argument("--downloads-root", required=True, type=Path)
    parser.add_argument("--attempts", type=int, default=4)
    parser.add_argument("--initial-delay-seconds", type=float, default=30.0)
    parser.add_argument("--max-delay-seconds", type=float, default=120.0)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("extra_args", nargs=argparse.REMAINDER)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.attempts < 1:
        print("error: --attempts must be at least 1", file=sys.stderr)
        return 2
    for directory in (args.install_root, args.packages_root, args.downloads_root, args.log.parent):
        directory.mkdir(parents=True, exist_ok=True)
    command = build_command(args)
    last_code = 0
    for attempt in range(1, args.attempts + 1):
        last_code, output = run_attempt(command, args.log, attempt)
        if last_code == 0:
            print(f"vcpkg install succeeded on attempt {attempt}. Log: {args.log}")
            return 0
        classification = classify_failure(output)
        if not classification.transient:
            print(f"vcpkg install failed permanently: {classification.reason}. Log: {args.log}")
            return last_code
        if attempt == args.attempts:
            print(f"vcpkg install failed after {args.attempts} transient attempts: {classification.reason}. Log: {args.log}")
            return last_code
        delay = min(args.initial_delay_seconds * (2 ** (attempt - 1)), args.max_delay_seconds)
        print(f"Transient vcpkg failure detected ({classification.reason}); retrying in {delay:g} seconds...")
        time.sleep(delay)
    return last_code


if __name__ == "__main__":
    raise SystemExit(main())
