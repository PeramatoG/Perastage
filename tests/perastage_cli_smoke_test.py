#!/usr/bin/env python3
"""Exercise the built CLI executable through a cross-platform subprocess."""

from __future__ import annotations

import json
import subprocess
import sys

HELP = """Usage: perastage-cli [--help | --version]
       perastage-cli inspect <file> [--view <view> | --json]
       perastage-cli capabilities [--json]

Options:
  -h, --help  Show this help and exit.
  --version   Show the CLI version and exit.

Commands:
  capabilities  List semantic operations and current frontend exposure.
  inspect     Inspect a GDTF or MVR package.
"""

CAPABILITIES_HELP = """Usage: perastage-cli capabilities [--json]

Options:
  --json      Emit discovery schema version 1.
  -h, --help  Show this help and exit.
"""

CAPABILITY_IDS = [
    "scene.convert.fixture_to_support",
    "scene.convert.scene_objects_to_trusses",
    "scene.group.create",
    "scene.group.ungroup",
    "scene.groups.list",
    "scene.layers.list",
    "scene.object.get",
    "scene.objects.list",
    "scene.patch.status",
    "scene.selection.clear",
    "scene.selection.get",
    "scene.selection.update",
    "scene.summary",
    "scene.transform.position",
    "scene.transform.rotation",
]


def run(binary: str, args: list[str]) -> subprocess.CompletedProcess[str]:
    """Run one CLI subprocess with stable text capture and timeout behavior."""
    return subprocess.run(
        [binary, *args], capture_output=True, text=True, timeout=5, check=False
    )


def check(binary: str, args: list[str], code: int, stdout: str, stderr: str) -> None:
    """Run one command with a timeout and assert its exact process result."""
    result = run(binary, args)
    actual = (result.returncode, result.stdout, result.stderr)
    expected = (code, stdout, stderr)
    if actual != expected:
        raise AssertionError(f"command {args!r}: expected {expected!r}, got {actual!r}")


def check_capabilities(binary: str) -> None:
    """Verify human and JSON discovery through the real CLI executable."""
    human = run(binary, ["capabilities"])
    if human.returncode != 0 or human.stderr:
        raise AssertionError(f"human capability discovery failed: {human!r}")
    if "discovery only; development_cli does not execute" not in human.stdout:
        raise AssertionError("human discovery does not state its non-execution scope")
    for operation_id in CAPABILITY_IDS:
        if operation_id not in human.stdout:
            raise AssertionError(f"human discovery omitted {operation_id}")

    structured = run(binary, ["capabilities", "--json"])
    if structured.returncode != 0 or structured.stderr:
        raise AssertionError(f"JSON capability discovery failed: {structured!r}")
    document = json.loads(structured.stdout)
    operations = document.get("operations")
    if document.get("schema_version") != 1 or not isinstance(operations, list):
        raise AssertionError("capability discovery has an invalid schema envelope")
    operation_ids = [operation.get("operation_id") for operation in operations]
    if operation_ids != CAPABILITY_IDS or operation_ids != sorted(operation_ids):
        raise AssertionError(f"unexpected capability inventory: {operation_ids!r}")
    for operation in operations:
        frontends = operation.get("frontend_exposure", [])
        if any(frontend.get("id") == "development_cli" for frontend in frontends):
            raise AssertionError("development CLI must not claim operation execution")


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
    check(binary, ["capabilities", "--help"], 0, CAPABILITIES_HELP, "")
    check(binary, ["capabilities", "--invalid"], 2, "",
          "perastage-cli capabilities: unknown option: --invalid\n"
          "Try 'perastage-cli capabilities --help' for usage.\n")
    check_capabilities(binary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
