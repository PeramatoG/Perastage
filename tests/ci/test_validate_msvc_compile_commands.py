#!/usr/bin/env python3
"""Test structural MSVC debug-information flag validation."""
from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).parents[2] / ".github/scripts/validate_msvc_compile_commands.py"
SPEC = importlib.util.spec_from_file_location("validate_msvc_compile_commands", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CompileCommandTests(unittest.TestCase):
    def validate(self, entries: list[dict], limit: int = 5) -> tuple[str, bool]:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "compile_commands.json"
            path.write_text(json.dumps(entries), encoding="utf-8")
            return MODULE.validate(path, sample_limit=limit)

    def test_command_and_arguments_accept_exact_z7(self) -> None:
        report, valid = self.validate([
            {"file": "a.c", "command": 'cl.exe /c /Z7 /Fdout.pdb "C:/paths/Zi name/a.c"'},
            {"file": "b.cpp", "arguments": ["cl.exe", "/c", "/Z7", "/FdZi.pdb", "b.cpp"]},
        ])
        self.assertTrue(valid)
        self.assertIn("C entries: 1", report)
        self.assertIn("C++ entries: 1", report)

    def test_rejects_exact_zi_zi_and_missing_flags(self) -> None:
        report, valid = self.validate([
            {"file": "lower.cpp", "arguments": ["cl.exe", "/Zi", "lower.cpp"]},
            {"file": "upper.cpp", "command": "cl.exe /ZI upper.cpp"},
            {"file": "missing.cpp", "arguments": ["cl.exe", "/FdZi.pdb", "missing.cpp"]},
        ])
        self.assertFalse(valid)
        self.assertIn("Offending /Zi entries: 1", report)
        self.assertIn("Offending /ZI entries: 1", report)
        self.assertIn("Entries missing /Z7: 3", report)

    def test_diagnostic_samples_are_bounded(self) -> None:
        entries = [{"file": f"bad-{index}.cpp", "arguments": ["cl.exe", "/Zi"]} for index in range(10)]
        report, valid = self.validate(entries, limit=3)
        self.assertFalse(valid)
        self.assertEqual(report.count("- bad-"), 3)


if __name__ == "__main__":
    unittest.main()
