#!/usr/bin/env python3
"""Exercise the built CLI executable through a cross-platform subprocess."""

from __future__ import annotations

import subprocess
import sys

HELP = """Usage: perastage-cli [--help | --version]
       perastage-cli inspect <file> [--view <view> | --json]

Options:
  -h, --help  Show this help and exit.
  --version   Show the CLI version and exit.

Commands:
  inspect     Inspect a GDTF or MVR package.
"""


def check(binary: str, args: list[str], code: int, stdout: str, stderr: str) -> None:
    """Run one command with a timeout and assert its exact process result."""
    result = subprocess.run(
        [binary, *args], capture_output=True, text=True, timeout=5, check=False
    )
    actual = (result.returncode, result.stdout, result.stderr)
    expected = (code, stdout, stderr)
    if actual != expected:
        raise AssertionError(f"command {args!r}: expected {expected!r}, got {actual!r}")


def main() -> int:
    """Run the required real-executable smoke-test cases."""
    if len(sys.argv) != 3:
        raise SystemExit("usage: perastage_cli_smoke_test.py BINARY VERSION")
    binary, version = sys.argv[1:]
    check(binary, [], 0, HELP, "")
    check(binary, ["--help"], 0, HELP, "")
    check(binary, ["--version"], 0, f"Perastage CLI {version}\n", "")
    check(binary, ["--invalid"], 2, "",
          "perastage-cli: unexpected argument: --invalid\n"
          "Try 'perastage-cli --help' for usage.\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
