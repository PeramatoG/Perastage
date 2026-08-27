#!/usr/bin/env python3
"""Collect durable, best-effort performance telemetry for Debug CI jobs."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _command_version(command: list[str]) -> str | None:
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=10, check=False)
        output = (result.stdout or result.stderr).strip().splitlines()
        return output[0] if output else None
    except (OSError, subprocess.SubprocessError):
        return None


def _memory() -> dict[str, int | None]:
    result: dict[str, int | None] = {"total_bytes": None, "available_bytes": None}
    try:
        if sys.platform == "win32":
            import ctypes

            class MemoryStatus(ctypes.Structure):
                _fields_ = [("length", ctypes.c_ulong), ("load", ctypes.c_ulong),
                            ("total", ctypes.c_ulonglong), ("available", ctypes.c_ulonglong),
                            ("total_page", ctypes.c_ulonglong), ("available_page", ctypes.c_ulonglong),
                            ("total_virtual", ctypes.c_ulonglong), ("available_virtual", ctypes.c_ulonglong),
                            ("available_extended", ctypes.c_ulonglong)]

            status = MemoryStatus()
            status.length = ctypes.sizeof(status)
            if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
                result = {"total_bytes": status.total, "available_bytes": status.available}
        elif sys.platform == "darwin":
            total_text = subprocess.run(
                ["sysctl", "-n", "hw.memsize"], capture_output=True, text=True, timeout=10, check=False
            ).stdout.strip()
            result["total_bytes"] = int(total_text) if total_text else None
            vm_stat = subprocess.run(
                ["vm_stat"], capture_output=True, text=True, timeout=10, check=False
            ).stdout
            import re

            page_match = re.search(r"page size of (\d+) bytes", vm_stat)
            counts = {key: int(value.replace(".", "")) for key, value in re.findall(
                r"^(Pages free|Pages inactive|Pages speculative):\s+([0-9.]+)", vm_stat, re.MULTILINE
            )}
            if page_match:
                result["available_bytes"] = sum(counts.values()) * int(page_match.group(1))
        else:
            values: dict[str, int] = {}
            for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
                key, value = line.split(":", 1)
                values[key] = int(value.strip().split()[0]) * 1024
            result = {"total_bytes": values.get("MemTotal"), "available_bytes": values.get("MemAvailable")}
    except (OSError, ValueError, AttributeError):
        pass
    return result


def directory_info(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"path": str(path), "exists": False, "size_bytes": 0, "file_count": 0}
    size = 0
    count = 0
    try:
        for root, _, files in os.walk(path):
            for name in files:
                count += 1
                try:
                    size += (Path(root) / name).stat().st_size
                except OSError:
                    continue
    except OSError:
        pass
    return {"path": str(path), "exists": True, "size_bytes": size, "file_count": count}


def parse_vcpkg_log(path: Path) -> dict[str, int | None]:
    metrics = {"already_installed": None, "restored": None, "built": None, "processed": None, "retries": None}
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return metrics
    import re

    patterns = {
        "restored": r"Restored\s+(\d+)\s+package\(s\)",
        "built": r"built\s+(\d+)\s+package\(s\)",
        "already_installed": r"(\d+)\s+package\(s\) (?:are|is) already installed",
        "processed": r"Total packages processed:\s*(\d+)",
    }
    for key, pattern in patterns.items():
        matches = re.findall(pattern, text, flags=re.IGNORECASE)
        if matches:
            metrics[key] = int(matches[-1])
    retry_matches = re.findall(r"retry(?:ing)?|attempt\s+[2-9]\d*", text, flags=re.IGNORECASE)
    metrics["retries"] = len(retry_matches) if retry_matches else 0
    return metrics


def _load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _save(path: Path, state: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def _snapshot(state: dict[str, Any], label: str, workspace: Path, directories: list[Path]) -> None:
    try:
        disk = shutil.disk_usage(workspace)
        disk_info = {"total_bytes": disk.total, "used_bytes": disk.used, "free_bytes": disk.free}
    except OSError:
        disk_info = {"total_bytes": None, "used_bytes": None, "free_bytes": None}
    state["snapshots"].append({"label": label, "timestamp": _utc_now(), "disk": disk_info,
                               "memory": _memory(), "directories": [directory_info(path) for path in directories]})


def _human_bytes(value: int | None) -> str:
    if value is None:
        return "unavailable"
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if value < 1024 or unit == "TiB":
            return f"{value:.1f} {unit}"
        value /= 1024
    return "unavailable"


def render_markdown(state: dict[str, Any]) -> str:
    lines = [f"## CI Debug performance telemetry — {state['display_name']}", "", "| Phase | Duration | Status |",
             "|---|---:|---|"]
    for phase in state["phases"]:
        lines.append(f"| {phase['name']} | {phase['duration_seconds']:.2f} s | {phase['status']} |")
    total = (time.perf_counter_ns() - state["started_monotonic_ns"]) / 1_000_000_000
    lines.extend([f"| **Total measured** | **{total:.2f} s** | |", "", "### vcpkg"])
    metadata = state.get("metadata", {})
    lines.extend([
        f"- Downloads cache: **{metadata.get('downloads_cache', 'unavailable')}**",
        f"- Compiled/installed cache: **{metadata.get('compiled_cache', 'unavailable')}**",
        f"- Cache identity: `{metadata.get('cache_primary_key', 'unavailable')[-32:]}`",
        f"- Remote GitHub Packages cache: **{metadata.get('remote_cache', 'unavailable')}**",
        f"- Remote setup / local fallback: **{metadata.get('remote_setup_result', 'unavailable')} / {metadata.get('fallback_local_only', 'unavailable')}**",
        f"- Downloads / compiled cache save: **{metadata.get('downloads_cache_save', 'skipped')} / {metadata.get('compiled_cache_save', 'skipped')}**",
    ])
    vcpkg = state.get("vcpkg", {})
    for label, key in (("Packages restored", "restored"), ("Packages built", "built"), ("Packages already installed", "already_installed")):
        lines.append(f"- {label}: **{vcpkg.get(key) if vcpkg.get(key) is not None else 'unavailable'}**")
    final_dirs = state["snapshots"][-1]["directories"] if state["snapshots"] else []
    for item in final_dirs:
        lines.append(f"- `{item['path']}`: {_human_bytes(item['size_bytes'])}, {item['file_count']} files")
    lines.extend(["", "### Compiler cache"])
    sccache = state.get("sccache", {})
    for key in ("compile_requests", "cache_hits", "cache_misses", "non_cacheable_compilations", "hit_rate"):
        lines.append(f"- {key.replace('_', ' ').title()}: **{sccache.get(key, 'unavailable')}**")
    lines.extend(["", "### Runner resources", f"- OS / architecture: {state['runner']['os']} / {state['runner']['architecture']}",
                  f"- Logical CPUs: {state['runner']['logical_cpu_count']}",
                  f"- Physical RAM: {_human_bytes(state['runner']['memory']['total_bytes'])}"])
    for snapshot in state["snapshots"]:
        lines.append(f"- Disk free ({snapshot['label']}): {_human_bytes(snapshot['disk']['free_bytes'])}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", required=True, type=Path)
    subparsers = parser.add_subparsers(dest="command", required=True)
    init = subparsers.add_parser("init")
    init.add_argument("--platform", required=True)
    init.add_argument("--display-name", required=True)
    init.add_argument("--workspace", required=True, type=Path)
    checkpoint = subparsers.add_parser("checkpoint")
    checkpoint.add_argument("--phase", required=True)
    checkpoint.add_argument("--status", default="success", choices=("success", "failure", "cancelled", "unknown", "skipped"))
    snapshot = subparsers.add_parser("snapshot")
    snapshot.add_argument("--label", required=True)
    snapshot.add_argument("--workspace", required=True, type=Path)
    snapshot.add_argument("--directory", action="append", default=[], type=Path)
    mark = subparsers.add_parser("mark")
    mark.add_argument("--value", action="append", default=[])
    finalize = subparsers.add_parser("finalize")
    finalize.add_argument("--output", required=True, type=Path)
    finalize.add_argument("--summary", type=Path)
    finalize.add_argument("--workspace", required=True, type=Path)
    finalize.add_argument("--directory", action="append", default=[], type=Path)
    finalize.add_argument("--vcpkg-log", type=Path)
    finalize.add_argument("--sccache-json", type=Path)
    args = parser.parse_args()
    now = time.perf_counter_ns()
    if args.command == "init":
        state = {"schema_version": SCHEMA_VERSION, "platform": args.platform, "display_name": args.display_name,
                 "started_at": _utc_now(), "started_monotonic_ns": now, "last_checkpoint_monotonic_ns": now,
                 "runner": {"os": platform.platform(), "architecture": platform.machine(),
                            "logical_cpu_count": os.cpu_count(), "memory": _memory(), "python": platform.python_version(),
                            "cmake": _command_version(["cmake", "--version"]), "ninja": _command_version(["ninja", "--version"])},
                 "phases": [], "snapshots": [], "metadata": {}}
    else:
        state = _load(args.state)
    if args.command == "checkpoint":
        duration = (now - state["last_checkpoint_monotonic_ns"]) / 1_000_000_000
        cumulative = (now - state["started_monotonic_ns"]) / 1_000_000_000
        state["phases"].append({"name": args.phase, "duration_seconds": duration,
                                "cumulative_seconds": cumulative, "status": args.status, "completed_at": _utc_now()})
        state["last_checkpoint_monotonic_ns"] = now
    elif args.command == "snapshot":
        _snapshot(state, args.label, args.workspace, args.directory)
        # Directory traversal is diagnostic overhead and must not inflate the following measured phase.
        state["last_checkpoint_monotonic_ns"] = time.perf_counter_ns()
    elif args.command == "mark":
        for value in args.value:
            key, separator, item = value.partition("=")
            if separator:
                state["metadata"][key] = item
    elif args.command == "finalize":
        _snapshot(state, "final", args.workspace, args.directory)
        state["finished_at"] = _utc_now()
        state["total_duration_seconds"] = (now - state["started_monotonic_ns"]) / 1_000_000_000
        state["vcpkg"] = parse_vcpkg_log(args.vcpkg_log) if args.vcpkg_log else {}
        if args.sccache_json:
            try:
                raw = json.loads(args.sccache_json.read_text(encoding="utf-8-sig"))
                stats = raw.get("stats", raw)
                def aggregate(value: Any) -> int | None:
                    if isinstance(value, int) and not isinstance(value, bool):
                        return value
                    if isinstance(value, dict) and isinstance(value.get("counts"), dict):
                        values = value["counts"].values()
                        return sum(item for item in values if isinstance(item, int) and not isinstance(item, bool))
                    return None

                hits = aggregate(stats.get("cache_hits"))
                misses = aggregate(stats.get("cache_misses"))
                denominator = (hits or 0) + (misses or 0)
                state["sccache"] = {
                    "compile_requests": aggregate(stats.get("compile_requests")), "cache_hits": hits,
                    "cache_misses": misses, "non_cacheable_compilations": aggregate(stats.get("non_cacheable_compilations")),
                    "hit_rate": f"{100 * hits / denominator:.1f}%" if hits is not None and denominator else "not applicable",
                }
            except (OSError, ValueError, AttributeError):
                state["sccache"] = {}
        markdown = render_markdown(state)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if args.summary:
            args.summary.parent.mkdir(parents=True, exist_ok=True)
            with args.summary.open("a", encoding="utf-8") as stream:
                stream.write(markdown)
    _save(args.state, state)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
