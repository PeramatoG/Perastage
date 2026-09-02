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
        manifest = root / "tracked-files.txt"
        manifest.write_text(
            "\n".join(
                sorted(
                    path.relative_to(root).as_posix()
                    for path in root.rglob("*")
                    if path.is_file() and path != manifest
                )
            ),
            encoding="utf-8",
        )
        return subprocess.run(
            [sys.executable, str(AUDIT), "--repo-root", str(root), "--baseline", str(BASELINE),
             "--tracked-files-from", str(manifest)],
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
        result = subprocess.run([sys.executable, str(AUDIT)], check=False, capture_output=True, text=True)
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

    def test_unexpected_root_header_is_rejected(self) -> None:
        """Reject an unregistered root header case-insensitively."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "temporary_api.HXX").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("unexpected root project source: temporary_api.HXX", result.stderr)

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

    def assert_machine_path_rejected(self, value: str) -> None:
        """Verify a new shared-config machine path produces an actionable error."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/new-build-config.json").write_text(value, encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("scripts/new-build-config.json:1 contains machine-specific absolute path", result.stderr)
        self.assertIn(value, result.stderr)

    def test_windows_machine_path_is_rejected(self) -> None:
        """Reject a new Windows drive-based development path."""
        self.assert_machine_path_rejected("D:/Development/toolchain/sdk")

    def test_linux_home_path_is_rejected(self) -> None:
        """Reject a new user-specific Linux home path."""
        self.assert_machine_path_rejected("/home/alice/toolchains/sdk")

    def test_macos_home_path_is_rejected(self) -> None:
        """Reject a new user-specific macOS home path."""
        self.assert_machine_path_rejected("/Users/alice/toolchains/sdk")

    def test_grandfathered_legacy_configuration_passes(self) -> None:
        """Accept only the recorded count of the legacy CMakeSettings path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "CMakeSettings.json").write_text(
                "C:/vcpkg/scripts/buildsystems/vcpkg.cmake\n" * 2, encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_additional_legacy_configuration_path_is_rejected(self) -> None:
        """Reject occurrences beyond the exact grandfathered legacy count."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "CMakeSettings.json").write_text(
                "C:/vcpkg/scripts/buildsystems/vcpkg.cmake\n" * 3, encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("1 unapproved occurrence(s)", result.stderr)

    def test_checkout_path_and_url_are_not_machine_paths(self) -> None:
        """Ignore fixture locations and URL schemes while scanning configuration."""
        with tempfile.TemporaryDirectory(prefix="home-user-src-Perastage-") as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/portable-config.json").write_text(
                f"https://example.com/tool\ncheckout={root}\n", encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_explicit_source_registration_passes(self) -> None:
        """Accept explicit source registration and unrelated resource globs."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/CMakeLists.txt").write_text(
                'target_sources(perastage PRIVATE widget.cpp)\nfile(GLOB icons "*.svg")\n', encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_production_source_glob_is_rejected(self) -> None:
        """Reject a generic CMake production-source glob."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/CMakeLists.txt").write_text('file(GLOB sources "*.cpp")\n', encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("forbidden production-source discovery", result.stderr)

    def test_recursive_source_glob_case_and_whitespace_is_rejected(self) -> None:
        """Reject case and whitespace variations of recursive source discovery."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/CMakeLists.txt").write_text(
                'FiLe (  GLOB_RECURSE\n sources CONFIGURE_DEPENDS "*.HPP" )\n', encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("GLOB_RECURSE", result.stderr)


if __name__ == "__main__":
    unittest.main()
