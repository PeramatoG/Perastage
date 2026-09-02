#!/usr/bin/env python3
"""Exercise positive and negative fixtures for the ORG-001 structure audit."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
AUDIT = REPOSITORY_ROOT / "tests/check_repository_structure_baseline.py"
BASELINE = REPOSITORY_ROOT / "docs/developer/repository_structure_baseline.json"


class RepositoryStructureBaselineTests(unittest.TestCase):
    """Verify current-tree success and representative baseline failures."""

    def run_audit(self, root: Path) -> subprocess.CompletedProcess[str]:
        """Run the audit against a supplied repository fixture root."""
        return subprocess.run(
            [sys.executable, str(AUDIT), "--repo-root", str(root), "--baseline", str(BASELINE)],
            check=False,
            capture_output=True,
            text=True,
        )

    def create_fixture(self, root: Path) -> None:
        """Create the smallest tree that satisfies the recorded baseline."""
        baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
        for paths in baseline["top_level_directories"].values():
            for relative in paths:
                (root / relative).mkdir(parents=True, exist_ok=True)
        required_files = baseline["root_file_roles"].values()
        entry_points = baseline["development_entry_points"].values()
        for paths in [*required_files, *entry_points]:
            for relative in paths:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
        registration = baseline["source_registration"]
        root_groups = " ".join(f"{group}/placeholder.cpp" for group in registration["root_registered_source_groups"])
        cmake_lines = [f"add_executable(Perastage main.cpp {root_groups})"]
        for directory in registration["module_cmake_directories"]:
            (root / directory / "CMakeLists.txt").touch()
            cmake_lines.append(f"add_subdirectory({directory})")
        for directory in registration["conditional_subdirectories"]:
            cmake_lines.append(f"add_subdirectory({directory})")
        (root / "CMakeLists.txt").write_text("\n".join(cmake_lines), encoding="utf-8")

    def test_current_repository_passes(self) -> None:
        """Accept the checked-in repository state."""
        result = self.run_audit(REPOSITORY_ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unexpected_root_source_is_rejected(self) -> None:
        """Reject an unregistered root source without changing the working tree."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "unexpected_root_helper.CPP").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("unexpected root project source: unexpected_root_helper.CPP", result.stderr)

    def test_missing_directory_is_actionable(self) -> None:
        """Report a missing required component by its repository-relative path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "models").rmdir()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("required top-level directory is missing: models/", result.stderr)

    def test_unlisted_non_source_root_file_is_accepted(self) -> None:
        """Avoid turning root policy into a complete filename allowlist."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "local-maintainer-note.txt").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
