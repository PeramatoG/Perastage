#!/usr/bin/env python3
"""Test strict parsing and reporting of sccache v0.15.0 statistics."""
from __future__ import annotations

import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).parents[2] / ".github/scripts/write_sccache_summary.py"
SPEC = importlib.util.spec_from_file_location("write_sccache_summary", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture(requests: int = 10, hits: int = 0, misses: int = 8, errors: int = 0) -> dict:
    return {
        "stats": {
            "compile_requests": requests,
            "cache_hits": {"counts": {"C/C++": hits}, "adv_counts": {}},
            "cache_misses": {"counts": {"C/C++": misses}, "adv_counts": {}},
            "cache_errors": {"counts": {"C/C++": errors}, "adv_counts": {}},
            "non_cacheable_compilations": max(0, requests - hits - misses),
            "cache_read_errors": 0,
            "cache_write_errors": 0,
        },
        "cache_location": "GitHub Actions Cache",
        "version": "0.15.0",
        "basedirs": ["/workspace/Perastage"],
    }


class SccacheSummaryTests(unittest.TestCase):
    def invoke_summary(
        self,
        root: Path,
        *,
        build_succeeded: str,
        launcher_validation: str,
        requests: int = 10,
        compile_commands_available: bool = True,
    ) -> int:
        stats = root / "stats.json"
        commands = root / "compile_commands.json"
        stats.write_text(json.dumps(fixture(requests=requests, misses=requests)), encoding="utf-8")
        if compile_commands_available:
            commands.write_text(json.dumps([{"file": "a.c"}, {"file": "b.cpp"}]), encoding="utf-8")
        old_argv = os.sys.argv
        os.sys.argv = [str(SCRIPT), "--stats-json", str(stats), "--compile-commands", str(commands),
                       "--platform", "linux", "--architecture", "X64", "--mode", "READ_ONLY",
                       "--namespace", "perastage-ci-debug-v1-linux", "--compiler-identity", "gcc-14",
                       "--base-directory", str(root), "--launcher-validation", launcher_validation,
                       "--build-succeeded", build_succeeded]
        try:
            return MODULE.main()
        finally:
            os.sys.argv = old_argv

    def test_cold_warm_errors_and_hit_rate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stats.json"
            path.write_text(json.dumps(fixture()), encoding="utf-8")
            self.assertEqual(MODULE.parse_statistics(path)["misses"], 8)
            path.write_text(json.dumps(fixture(hits=7, misses=1, errors=2)), encoding="utf-8")
            values = MODULE.parse_statistics(path)
            self.assertEqual((values["hits"], values["misses"], values["errors"]), (7, 1, 2))

    def test_missing_optional_fields_and_zero_cacheable(self) -> None:
        data = fixture(requests=2, misses=0)
        del data["stats"]["non_cacheable_compilations"]
        del data["stats"]["cache_read_errors"]
        del data["stats"]["cache_write_errors"]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stats.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            values = MODULE.parse_statistics(path)
            self.assertIsNone(values["non_cacheable"])
            self.assertEqual(values["cacheable"], 0)

    def test_malformed_json_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stats.json"
            path.write_text("not json", encoding="utf-8")
            with self.assertRaises(MODULE.StatisticsError):
                MODULE.parse_statistics(path)

    def test_compile_command_metadata_for_all_platforms(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "compile_commands.json"
            path.write_text(json.dumps([{"file": "a.c"}, {"file": r"C:\\src\\b.cpp"}, {"file": "/src/c.mm"}]), encoding="utf-8")
            self.assertEqual(MODULE.compile_command_counts(path), (1, 2))

    def test_step_summary_and_zero_request_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            stats = root / "stats.json"
            commands = root / "compile_commands.json"
            summary = root / "summary.md"
            commands.write_text(json.dumps([{"file": "a.c"}, {"file": "b.cpp"}]), encoding="utf-8")
            stats.write_text(json.dumps(fixture(hits=4, misses=4)), encoding="utf-8")
            old_argv, old_summary = os.sys.argv, os.environ.get("GITHUB_STEP_SUMMARY")
            os.environ["GITHUB_STEP_SUMMARY"] = str(summary)
            os.sys.argv = [str(SCRIPT), "--stats-json", str(stats), "--compile-commands", str(commands),
                           "--platform", "linux", "--architecture", "X64", "--mode", "READ_ONLY",
                           "--namespace", "perastage-ci-debug-v1-linux", "--compiler-identity", "gcc-14",
                           "--base-directory", str(root), "--launcher-validation", "passed", "--build-succeeded", "true"]
            try:
                self.assertEqual(MODULE.main(), 0)
                text = summary.read_text(encoding="utf-8")
                self.assertIn("Hit rate: 50.0%", text)
                self.assertNotIn("TOKEN", text)
                stats.write_text(json.dumps(fixture(requests=0, misses=0)), encoding="utf-8")
                with self.assertRaises(MODULE.StatisticsError):
                    MODULE.main()
            finally:
                os.sys.argv = old_argv
                if old_summary is None:
                    os.environ.pop("GITHUB_STEP_SUMMARY", None)
                else:
                    os.environ["GITHUB_STEP_SUMMARY"] = old_summary

    def test_prior_configure_or_build_failure_is_diagnostic_only(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            self.assertEqual(self.invoke_summary(root, build_succeeded="false", launcher_validation="failed", requests=0,
                                                 compile_commands_available=False), 0)
            self.assertEqual(self.invoke_summary(root, build_succeeded="false", launcher_validation="passed"), 0)

    def test_successful_build_requires_valid_launcher(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(MODULE.StatisticsError):
                self.invoke_summary(Path(directory).resolve(), build_succeeded="true", launcher_validation="failed")


if __name__ == "__main__":
    unittest.main()
