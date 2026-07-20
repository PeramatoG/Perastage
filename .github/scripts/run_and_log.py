#!/usr/bin/env python3
"""Run a command while teeing combined stdout/stderr to a log file."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, help="Path to the log file to write.")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="Command and arguments to run.")
    args = parser.parse_args()
    if not args.command:
        parser.error("a command after -- is required")
    if args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("a command after -- is required")
    return args


def main() -> int:
    args = parse_args()
    log_path = Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8", errors="replace") as log_file:
        process = subprocess.Popen(
            args.command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            log_file.write(line)
        return process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
