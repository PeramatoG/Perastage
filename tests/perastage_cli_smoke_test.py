#!/usr/bin/env python3
"""Exercise the built CLI executable through a cross-platform subprocess."""

from __future__ import annotations

import json
import subprocess
import sys

HELP = """Usage: perastage-cli [--help | --version]
       perastage-cli inspect <file> [--view <view> | --json]
       perastage-cli capabilities [--json]
       perastage-cli live <command <text> | query <id> | execute <id>> [--args <json>] [--port <port>]
       perastage-cli scene <input.mvr> --output <output.mvr> --command <text> [--command <text> ...] [--overwrite] [--json]

Options:
  -h, --help  Show this help and exit.
  --version   Show the CLI version and exit.

Commands:
  capabilities  List semantic operations and current frontend exposure.
  inspect     Inspect a GDTF or MVR package.
  live        Invoke supported operations on the running local app.
  scene       Mutate an isolated MVR and publish an explicit output.
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
    "scene.transform.batch",
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
    if "Semantic capabilities and current frontend exposure" not in human.stdout:
        raise AssertionError("human discovery does not state frontend exposure")
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
    batch = operations[operation_ids.index("scene.transform.batch")]
    if batch.get("kind") != "command" or batch.get("effect") != "mutating":
        raise AssertionError("batch transforms must be a mutating Command")
    batch_arguments = [
        (argument.get("id"), argument.get("type"), argument.get("required"))
        for argument in batch.get("arguments", [])
    ]
    expected_batch_arguments = [
        ("target_kinds", "string_list", True),
        ("target_uuids", "string_list", True),
        ("component_kinds", "string_list", True),
        ("axes", "string_list", True),
        ("values", "float64_list", True),
        ("modes", "string_list", True),
        ("spaces", "string_list", True),
    ]
    if batch_arguments != expected_batch_arguments:
        raise AssertionError(f"unexpected batch argument contract: {batch_arguments!r}")
    if batch.get("frontend_exposure") != [
        {"id": "local_live_cli", "state": "full"},
        {"id": "mcp", "state": "full"},
    ]:
        raise AssertionError("batch CLI/MCP exposure is not truthful")
    executable = {
        "scene.selection.clear": "full",
        "scene.selection.update": "partial",
        "scene.transform.position": "full",
        "scene.transform.rotation": "full",
    }
    for operation in operations:
        cli_exposure = [frontend.get("state") for frontend in
                        operation.get("frontend_exposure", [])
                        if frontend.get("id") == "development_cli"]
        expected = executable.get(operation.get("operation_id"))
        if cli_exposure != ([expected] if expected else []):
            raise AssertionError("development CLI exposure is not truthful")


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
