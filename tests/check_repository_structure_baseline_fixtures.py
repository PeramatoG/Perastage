#!/usr/bin/env python3
"""Exercise ORG-001 baseline and ORG-002 structural regression fixtures."""

from __future__ import annotations

import json
import shutil
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

    def run_audit(self, root: Path, baseline_path: Path = BASELINE) -> subprocess.CompletedProcess[str]:
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
            [sys.executable, str(AUDIT), "--repo-root", str(root), "--baseline", str(baseline_path),
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
        for item in baseline["structural_guard"]["machine_path_scan"]["grandfathered_occurrences"]:
            path = root / item["file"]
            existing = path.read_text(encoding="utf-8") if path.is_file() else ""
            occurrences = (item["value"] + "\n") * item["count"]
            path.write_text(existing + occurrences, encoding="utf-8")

    def write_baseline(self, root: Path, baseline: dict) -> Path:
        """Write a fixture-specific declarative baseline outside the tracked manifest."""
        path = root / "fixture-baseline.json"
        path.write_text(json.dumps(baseline), encoding="utf-8")
        return path

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
            shutil.rmtree(root / "models")
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

    def test_declared_modules_tests_third_party_and_support_data_pass(self) -> None:
        """Accept classified source trees and source-free unclassified support data."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/current.cpp").touch()
            (root / "tests/policy_test.cpp").touch()
            (root / "third_party/vendor.hpp").write_text(
                "// Vendored from https://example.invalid/upstream\n",
                encoding="utf-8",
            )
            (root / "maintainer-data/owners.txt").parent.mkdir()
            (root / "maintainer-data/owners.txt").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unregistered_top_level_source_module_is_rejected(self) -> None:
        """Reject production implementation files under an unclassified top-level directory."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "network/foo.cpp").parent.mkdir()
            (root / "network/foo.cpp").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("unregistered top-level source module: network/", result.stderr)
        self.assertIn("documented ownership", result.stderr)

    def test_unregistered_top_level_header_module_is_rejected(self) -> None:
        """Reject header-only module code under an unclassified top-level directory."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "newmodule/include/foo.hpp").parent.mkdir(parents=True)
            (root / "newmodule/include/foo.hpp").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("unregistered top-level source module: newmodule/", result.stderr)

    def test_intentionally_classified_source_module_passes(self) -> None:
        """Allow architecture evolution when the declarative classification changes with it."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "network/foo.cpp").parent.mkdir()
            (root / "network/foo.cpp").touch()
            baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
            baseline["top_level_directories"]["source_modules"].append("network")
            baseline["source_registration"]["module_cmake_directories"].append("network")
            (root / "network/CMakeLists.txt").touch()
            with (root / "CMakeLists.txt").open("a", encoding="utf-8") as stream:
                stream.write("\nadd_subdirectory(network)\n")
            result = self.run_audit(root, self.write_baseline(root, baseline))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_documented_source_module_without_cmake_ownership_is_rejected(self) -> None:
        """Reject a documented source module that lacks module CMake ownership."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
            baseline["top_level_directories"]["source_modules"].append("network")
            result = self.run_audit(root, self.write_baseline(root, baseline))
        self.assertEqual(result.returncode, 1)
        self.assertIn("source-module classification differs from module CMake ownership", result.stderr)

    def test_cmake_module_without_source_classification_is_rejected(self) -> None:
        """Reject module CMake ownership without source-module classification."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
            baseline["source_registration"]["module_cmake_directories"].append("network")
            result = self.run_audit(root, self.write_baseline(root, baseline))
        self.assertEqual(result.returncode, 1)
        self.assertIn("source-module classification differs from module CMake ownership", result.stderr)

    def test_root_owned_feature_source_is_rejected(self) -> None:
        """Reject feature implementation ownership in the root application target."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            cmake = root / "CMakeLists.txt"
            cmake.write_text(
                cmake.read_text(encoding="utf-8").replace(
                    "main.cpp",
                    "main.cpp core/feature.cpp",
                    1,
                ),
                encoding="utf-8",
            )
            (root / "core/feature.cpp").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("root add_executable retains feature source ownership for core/", result.stderr)

    def test_vendored_marker_in_first_party_module_is_rejected(self) -> None:
        """Reject conservative provenance evidence outside third_party ownership."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/copied.cpp").write_text(
                "// Vendored from https://example.invalid/upstream\n",
                encoding="utf-8",
            )
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("third-party ownership violation: core/copied.cpp", result.stderr)

    def test_vendor_style_source_directory_outside_owner_is_rejected(self) -> None:
        """Reject obvious vendor-style source placement outside third_party."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/vendor/library.cpp").parent.mkdir()
            (root / "core/vendor/library.cpp").touch()
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("third-party ownership violation: core/vendor/library.cpp", result.stderr)

    def test_first_party_license_header_is_not_rejected(self) -> None:
        """Avoid interpreting an ordinary first-party copyright header as vendored evidence."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "core/owned.cpp").write_text(
                "// Copyright 2026 Perastage contributors\n"
                "// Permission is hereby granted, free of charge, to any person obtaining a copy\n",
                encoding="utf-8",
            )
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
        self.assertIn(repr(value), result.stderr)

    def test_windows_machine_path_is_rejected(self) -> None:
        """Reject a new Windows drive-based development path."""
        self.assert_machine_path_rejected("D:/Development/toolchain/sdk")

    def test_windows_user_path_with_backslashes_is_rejected(self) -> None:
        """Reject deterministic Windows user paths written with backslashes."""
        self.assert_machine_path_rejected(
            r"C:\Users\RUNNER~1\AppData\Local\Temp\checkout"
        )

    def test_linux_home_path_is_rejected(self) -> None:
        """Reject a new user-specific Linux home path."""
        self.assert_machine_path_rejected("/home/alice/toolchains/sdk")

    def test_macos_home_path_is_rejected(self) -> None:
        """Reject a new user-specific macOS home path."""
        self.assert_machine_path_rejected("/Users/alice/toolchains/sdk")

    def test_portable_environment_variable_path_is_accepted(self) -> None:
        """Accept a toolchain path derived from a documented environment variable."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/portable-config.json").write_text(
                "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake\n", encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_machine_path_is_rejected_without_legacy_exceptions(self) -> None:
        """Reject the former fixed vcpkg path after removing legacy exceptions."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/windows-config.json").write_text(
                "C:/vcpkg/scripts/buildsystems/vcpkg.cmake\n", encoding="utf-8")
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("1 unapproved occurrence(s)", result.stderr)

    def test_stale_fixture_exception_is_rejected(self) -> None:
        """Keep proving that an exact-count exception cannot outlive its path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
            baseline["structural_guard"]["machine_path_scan"]["grandfathered_occurrences"] = [{
                "file": "scripts/legacy-config.json", "value": "C:/vcpkg", "count": 1
            }]
            (root / "scripts/legacy-config.json").write_text("{}\n", encoding="utf-8")
            result = self.run_audit(root, self.write_baseline(root, baseline))
        self.assertEqual(result.returncode, 1)
        self.assertIn("stale grandfathered machine-path baseline for scripts/legacy-config.json", result.stderr)

    def test_portable_wsl_ignore_paths_are_accepted(self) -> None:
        """Accept standard WSL Windows-drive isolation paths used by supported presets."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/wsl-config.json").write_text(
                "/mnt/c;/mnt/c/vcpkg/installed/x64-windows\n", encoding="utf-8"
            )
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_user_specific_wsl_path_is_rejected(self) -> None:
        """Reject a user-specific path reached through a WSL drive mount."""
        self.assert_machine_path_rejected("/mnt/d/Users/alice/toolchain/sdk")

    def test_checkout_location_does_not_trigger_machine_path_guard(self) -> None:
        """Ignore the physical fixture location when it is not stored in shared configuration."""
        with tempfile.TemporaryDirectory(prefix="home-user-src-Perastage-") as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_urls_are_not_treated_as_machine_paths(self) -> None:
        """Accept normal URLs in scanned shared configuration."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/portable-config.json").write_text(
                "https://example.com/tool\n", encoding="utf-8"
            )
            result = self.run_audit(root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_portable_project_path_indirection_is_accepted(self) -> None:
        """Accept the existing project-directory variable form instead of a literal checkout path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.create_fixture(root)
            (root / "scripts/portable-config.json").write_text(
                "${projectDir}\\out\\build\n", encoding="utf-8"
            )
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
