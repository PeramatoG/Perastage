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

    def test_command_and_arguments_accept_both_z7_prefixes(self) -> None:
        report, valid = self.validate([
            {"file": "a.c", "command": '"C:/Program Files/MSVC/cl.exe" -MDd -Z7 /showIncludes -Fdout.pdb "C:/paths/Zi name/a.c"'},
            {"file": "b.cpp", "arguments": ["cl.exe", "/c", "/Z7", "-FdZi.pdb", "b.cpp"]},
            {"file": "c.cpp", "command": "cl.exe /c /Z7 c.cpp"},
            {"file": "d.cpp", "arguments": ["cl.exe", "/c", "-Z7", "d.cpp"]},
        ])
        self.assertTrue(valid)
        self.assertIn("Validated C entries: 1", report)
        self.assertIn("Validated C++ entries: 3", report)
        self.assertIn("C/C++ entries with embedded debug information (Z7): 4", report)

    def test_real_build_shape_excludes_resource_compilation(self) -> None:
        report, valid = self.validate([
            {"file": "src/first.cpp", "command": '"C:/Program Files/MSVC/cl.exe" -MDd -Z7 /showIncludes src/first.cpp'},
            {"file": "src/second.cpp", "arguments": ["C:/Program Files/MSVC/cl.exe", "-MDd", "-Z7", "src/second.cpp"]},
            {"file": "D:/a/Perastage/Perastage/resources/Perastage.rc",
             "command": '"C:/Program Files/Windows Kits/10/bin/x64/rc.exe" /fo build/Perastage.rc.res resources/Perastage.rc'},
        ])
        self.assertTrue(valid)
        self.assertIn("Total compile database entries: 3", report)
        self.assertIn("Validated C++ entries: 2", report)
        self.assertIn("Windows resource entries excluded from Z7 validation: 1", report)
        self.assertIn("C/C++ entries missing embedded debug information: 0", report)

    def test_rejects_exact_zi_zi_and_missing_flags(self) -> None:
        report, valid = self.validate([
            {"file": "slash-lower.cpp", "arguments": ["cl.exe", "/Zi", "slash-lower.cpp"]},
            {"file": "dash-lower.cpp", "arguments": ["cl.exe", "-Zi", "dash-lower.cpp"]},
            {"file": "slash-upper.cpp", "command": "cl.exe /ZI slash-upper.cpp"},
            {"file": "dash-upper.cpp", "command": "cl.exe -ZI dash-upper.cpp"},
            {"file": "missing.cpp", "arguments": ["cl.exe", "/FdZi.pdb", "missing.cpp"]},
            {"file": "mixed.cpp", "arguments": ["cl.exe", "-Z7", "mixed.cpp"]},
        ])
        self.assertFalse(valid)
        self.assertIn("Offending Program Database entries (Zi): 2", report)
        self.assertIn("Offending Edit-and-Continue entries (ZI): 2", report)
        self.assertIn("C/C++ entries missing embedded debug information: 5", report)
        self.assertIn("-Zi", report)
        self.assertIn("/ZI", report)

    def test_empty_and_malformed_databases_are_rejected(self) -> None:
        report, valid = self.validate([])
        self.assertFalse(valid)
        self.assertIn("Total compile database entries: 0", report)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "compile_commands.json"
            path.write_text("not json", encoding="utf-8")
            with self.assertRaises(ValueError):
                MODULE.validate(path)
        with self.assertRaises(ValueError):
            self.validate([{"file": "missing-command.cpp"}])

    def test_diagnostic_samples_are_bounded(self) -> None:
        entries = [{"file": f"bad-{index}.cpp", "arguments": ["cl.exe", "/Zi"]} for index in range(10)]
        report, valid = self.validate(entries, limit=3)
        self.assertFalse(valid)
        self.assertEqual(report.count("- bad-"), 3)

    def test_unexpected_source_and_compiler_combinations_fail(self) -> None:
        cases = [
            {"file": "wrong.cpp", "arguments": ["rc.exe", "-Z7", "wrong.cpp"]},
            {"file": "wrong.rc", "arguments": ["cl.exe", "wrong.rc"]},
            {"file": "unknown.asm", "arguments": ["cl.exe", "-Z7", "unknown.asm"]},
        ]
        for entry in cases:
            report, valid = self.validate([entry])
            self.assertFalse(valid)
            self.assertIn("Unexpected entries: 1", report)
            self.assertIn("suffix=", report)
            self.assertIn("executable=", report)
            self.assertIn("reason=", report)

    def test_unexpected_diagnostics_are_bounded(self) -> None:
        entries = [{"file": f"bad-{index}.xyz", "arguments": ["tool.exe"]} for index in range(8)]
        report, valid = self.validate(entries, limit=2)
        self.assertFalse(valid)
        self.assertEqual(report.count("reason=unsupported source extension"), 2)


if __name__ == "__main__":
    unittest.main()
