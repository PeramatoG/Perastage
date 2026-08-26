import importlib.util
import json
import subprocess
import sys
from pathlib import Path


SCRIPT = Path(__file__).parents[2] / ".github" / "scripts" / "ci_telemetry.py"
SPEC = importlib.util.spec_from_file_location("ci_telemetry", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def test_directory_info_handles_present_and_missing_directories(tmp_path):
    present = tmp_path / "present"
    present.mkdir()
    (present / "data.bin").write_bytes(b"1234")
    assert MODULE.directory_info(present)["file_count"] == 1
    assert MODULE.directory_info(present)["size_bytes"] == 4
    assert MODULE.directory_info(tmp_path / "missing")["exists"] is False


def test_vcpkg_parser_is_conservative(tmp_path):
    log = tmp_path / "vcpkg.log"
    log.write_text("Restored 12 package(s) from cache\nInstalling example:x64-linux\n", encoding="utf-8")
    metrics = MODULE.parse_vcpkg_log(log)
    assert metrics["restored"] == 12
    assert metrics["built"] is None


def test_cli_creates_checkpoints_json_and_markdown(tmp_path):
    state = tmp_path / "state.json"
    output = tmp_path / "telemetry.json"
    summary = tmp_path / "summary.md"
    base = [sys.executable, str(SCRIPT), "--state", str(state)]
    subprocess.run(base + ["init", "--platform", "linux-debug", "--display-name", "Linux", "--workspace", str(tmp_path)], check=True)
    subprocess.run(base + ["checkpoint", "--phase", "Environment preparation"], check=True)
    subprocess.run(base + ["snapshot", "--label", "before caches", "--workspace", str(tmp_path), "--directory", str(tmp_path / "missing")], check=True)
    subprocess.run(base + ["finalize", "--output", str(output), "--summary", str(summary), "--workspace", str(tmp_path)], check=True)
    payload = json.loads(output.read_text(encoding="utf-8"))
    assert payload["schema_version"] == 1
    assert payload["phases"][0]["duration_seconds"] >= 0
    assert payload["phases"][0]["cumulative_seconds"] >= 0
    assert "## CI Debug performance telemetry — Linux" in summary.read_text(encoding="utf-8")
    assert "### Runner resources" in summary.read_text(encoding="utf-8")
