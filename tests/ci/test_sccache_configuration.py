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
        self.assertEqual(values["cache_warming"], "false")

    def test_trusted_current_main_push_uses_gha_and_warms_cache(self) -> None:
        values = SCOPE.select_scope("push", "refs/heads/main", "same", "same", "")
        self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]), ("on", "gha-main"))
        self.assertEqual(values["cache_warming"], "true")

    def test_stale_or_non_main_push_uses_ephemeral_disk(self) -> None:
        for ref, source_sha in (("refs/heads/main", "stale"), ("refs/heads/topic", "same")):
            values = SCOPE.select_scope("push", ref, source_sha, "same", "")
            self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]),
                             ("off", "disk-ephemeral"))
            self.assertEqual(values["cache_warming"], "false")

    def test_trusted_current_main_dispatch_uses_gha(self) -> None:
        values = SCOPE.select_scope("workflow_dispatch", "refs/heads/main", "same", "same", "main")
        self.assertEqual((values["sccache_gha_enabled"], values["sccache_cache_scope"]), ("on", "gha-main"))

    def test_arbitrary_manual_and_workflow_call_use_ephemeral_disk(self) -> None:
        for event, ref, requested, source_sha in (("workflow_dispatch", "refs/heads/main", "topic", "same"),
                                                  ("workflow_dispatch", "refs/heads/main", "", "arbitrary"),
                                                  ("workflow_dispatch", "refs/heads/topic", "", "same"),
                                                  ("workflow_call", "refs/heads/main", "", "same")):
            values = SCOPE.select_scope(event, ref, source_sha, "same", requested)
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

    def test_full_windows_configuration_is_deterministic(self) -> None:
        text = INITIAL_CACHE.cache_text(
            "C:/Tools/sccache/sccache.exe",
            c_compiler="C:/Program Files/MSVC/Hostx64/x64/cl.exe",
            cxx_compiler="C:/Program Files/MSVC/Hostx64/x64/cl.exe",
            bash_executable="C:/Program Files/Git/bin/bash.exe",
            policy_default_cmp0141="NEW",
            msvc_debug_information_format="Embedded",
        )
        names = [line.split("(", 1)[1].split(" ", 1)[0] for line in text.splitlines()]
        self.assertEqual(names, ["CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "BASH_EXECUTABLE",
                                 "PERASTAGE_COMPILER_CACHE_PROGRAM", "CMAKE_C_COMPILER_LAUNCHER",
                                 "CMAKE_CXX_COMPILER_LAUNCHER", "CMAKE_POLICY_DEFAULT_CMP0141",
                                 "CMAKE_MSVC_DEBUG_INFORMATION_FORMAT"])
        self.assertNotIn("\\", text)
        self.assertEqual(text.count(".exe"), 6)
        self.assertIn(
            "set(BASH_EXECUTABLE [[C:/Program Files/Git/bin/bash.exe]] CACHE FILEPATH \"\" FORCE)",
            text,
        )

    def test_bash_entry_is_optional_and_preserves_compilers_and_launchers(self) -> None:
        without_bash = INITIAL_CACHE.cache_text(
            "/usr/bin/sccache", c_compiler="/usr/bin/cc", cxx_compiler="/usr/bin/c++"
        )
        self.assertNotIn("BASH_EXECUTABLE", without_bash)
        for name in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_C_COMPILER_LAUNCHER",
                     "CMAKE_CXX_COMPILER_LAUNCHER", "PERASTAGE_COMPILER_CACHE_PROGRAM"):
            self.assertIn(f"set({name} ", without_bash)

    def test_bash_bracket_quoting_prevents_cache_code_injection(self) -> None:
        bash = "C:/Program Files/Git/bin/bash.exe]]\nset(INJECTED yes)\n[["
        text = INITIAL_CACHE.cache_text("C:/Tools/sccache.exe", bash_executable=bash)
        self.assertIn(f'set(BASH_EXECUTABLE [=[{bash}]=] CACHE FILEPATH "" FORCE)', text)
        self.assertEqual(text.count("\nset(INJECTED yes)\n"), 1)
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "verify-cache.cmake"
            script.write_text(
                text + 'if(DEFINED INJECTED)\n  message(FATAL_ERROR "cache value executed")\nendif()\n',
                encoding="utf-8",
            )
            result = subprocess.run(["cmake", "-P", str(script)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)

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
