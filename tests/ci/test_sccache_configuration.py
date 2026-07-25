#!/usr/bin/env python3
"""Test sccache scope selection and CMake initial-cache generation."""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


def load_script(name: str):
    path = Path(__file__).parents[2] / ".github/scripts" / name
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SCOPE = load_script("resolve_sccache_scope.py")
INITIAL_CACHE = load_script("write_cmake_compiler_cache_init.py")
INITIAL_CACHE_PATH = Path(__file__).parents[2] / ".github/scripts/write_cmake_compiler_cache_init.py"


class ScopeTests(unittest.TestCase):
    def test_pull_request_uses_merge_ref_scope(self) -> None:
        values = SCOPE.select_scope("pull_request", "refs/pull/2220/merge", "pr", "main", "")
        self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]), ("on", "gha-pr-merge-ref"))

    def test_trusted_current_main_dispatch_uses_gha(self) -> None:
        values = SCOPE.select_scope("workflow_dispatch", "refs/heads/main", "same", "same", "main")
        self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]), ("on", "gha-main"))

    def test_arbitrary_manual_and_workflow_call_use_ephemeral_disk(self) -> None:
        for event, ref, requested in (("workflow_dispatch", "refs/heads/main", "topic"),
                                      ("workflow_call", "refs/heads/main", "")):
            values = SCOPE.select_scope(event, ref, "same", "same", requested)
            self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]), ("off", "disk-ephemeral"))


class InitialCacheTests(unittest.TestCase):
    def test_platform_paths_and_exact_entries(self) -> None:
        paths = ("/opt/sccache", "/Applications/Tools/sccache", r"C:\Program Files\sccache\sccache.exe")
        for path in paths:
            text = INITIAL_CACHE.cache_text(path)
            self.assertEqual(text.count("set("), 3)
            self.assertIn(path, text)
            self.assertEqual(text.count(" CACHE "), 3)

    def test_bracket_quoting_blocks_cmake_injection(self) -> None:
        malicious = "path]]\nmessage(FATAL_ERROR injected)\n[[sccache"
        text = INITIAL_CACHE.cache_text(malicious)
        self.assertIn("[=[", text)
        self.assertIn("]=] CACHE", text)
        self.assertEqual(text.count("set("), 3)
        self.assertEqual(text.count("message("), 3)

    def test_exact_file_validation_rejects_extensionless_and_missing_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Program Files" / "sccache.exe"
            executable.parent.mkdir()
            executable.write_text("binary", encoding="utf-8")
            output = root / "initial-cache.cmake"
            valid = subprocess.run([sys.executable, str(INITIAL_CACHE_PATH), "--launcher", str(executable),
                                    "--output", str(output)], text=True, capture_output=True)
            self.assertEqual(valid.returncode, 0, valid.stderr)
            generated = output.read_text(encoding="utf-8")
            self.assertEqual(generated.count(str(executable.resolve())), 3)
            for invalid in (executable.with_suffix(""), root / "missing.exe"):
                rejected = subprocess.run([sys.executable, str(INITIAL_CACHE_PATH), "--launcher", str(invalid),
                                           "--output", str(output)], text=True, capture_output=True)
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn("does not exist", rejected.stderr)


if __name__ == "__main__":
    unittest.main()
