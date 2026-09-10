#!/usr/bin/env python3
"""Exercise source-size limit and hotspot ratchet behavior."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import check_source_file_size as guard


class SourceFileSizePolicyTest(unittest.TestCase):
    """Validate ordinary, grandfathered, excluded, and malformed policy cases."""

    def setUp(self) -> None:
        """Create a minimal isolated repository and policy for each test."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.policy = {
            "schema_version": 1,
            "default_max_lines": 5,
            "source_suffixes": [".cpp", ".h"],
            "excluded_prefixes": ["third_party/", "tests/fixtures/"],
            "hotspots": {"src/hotspot.cpp": 8},
        }

    def tearDown(self) -> None:
        """Remove the isolated repository after each test."""
        self.temporary_directory.cleanup()

    def write_lines(self, relative: str, count: int) -> None:
        """Write a source fixture with the requested physical line count."""
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("line\n" * count, encoding="utf-8")

    def errors_for(self, relative: str, count: int) -> list[str]:
        """Audit one source fixture and return its diagnostics."""
        self.write_lines(relative, count)
        if relative != "src/hotspot.cpp":
            self.write_lines("src/hotspot.cpp", 8)
        errors, _ = guard.audit(self.root, sorted({relative, "src/hotspot.cpp"}), self.policy)
        return errors

    def test_ordinary_file_below_threshold_passes(self) -> None:
        """Accept an ordinary source below the default maximum."""
        self.assertEqual([], self.errors_for("src/ordinary.cpp", 4))

    def test_ordinary_file_above_threshold_fails(self) -> None:
        """Reject an ordinary source above the default maximum."""
        self.assertIn("default limit of 5", "\n".join(self.errors_for("src/ordinary.cpp", 6)))

    def test_hotspot_at_baseline_passes(self) -> None:
        """Accept a hotspot exactly at its recorded baseline."""
        self.assertEqual([], self.errors_for("src/hotspot.cpp", 8))

    def test_hotspot_below_baseline_passes(self) -> None:
        """Accept a hotspot that has shrunk without changing its baseline."""
        self.assertEqual([], self.errors_for("src/hotspot.cpp", 7))

    def test_hotspot_above_baseline_fails(self) -> None:
        """Reject growth beyond a hotspot's recorded baseline."""
        self.assertIn("hotspot baseline of 8", "\n".join(self.errors_for("src/hotspot.cpp", 9)))

    def test_excluded_and_non_source_paths_are_ignored(self) -> None:
        """Ignore vendor, fixture, and non-C/C++ tracked paths."""
        files = ["third_party/vendor.cpp", "tests/fixtures/large.cpp", "docs/large.md", "src/hotspot.cpp"]
        for relative in files:
            self.write_lines(relative, 20 if relative != "src/hotspot.cpp" else 8)
        errors, inspected = guard.audit(self.root, files, self.policy)
        self.assertEqual([], errors)
        self.assertEqual(1, inspected)

    def test_malformed_policy_is_rejected(self) -> None:
        """Reject invalid hotspot limits in the versioned policy data."""
        policy_path = self.root / "policy.json"
        malformed = dict(self.policy)
        malformed["hotspots"] = {"src/hotspot.cpp": "eight"}
        policy_path.write_text(json.dumps(malformed), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "integer limit above"):
            guard.load_policy(policy_path)


if __name__ == "__main__":
    unittest.main()
