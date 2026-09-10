#!/usr/bin/env python3
"""Exercise tracked-artifact and tracked-file size policy behavior."""

from __future__ import annotations

import json
import re
import tempfile
import unittest
from pathlib import Path

import check_repository_hygiene as guard


class RepositoryHygienePolicyTest(unittest.TestCase):
    """Validate artifact, asset, size, and policy configuration cases."""

    def setUp(self) -> None:
        """Create an isolated repository and valid compact policy."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.policy = {
            "schema_version": 1,
            "default_max_bytes": 10,
            "prohibited_suffixes": [".dll", ".o", ".zip"],
            "prohibited_filename_patterns": [r"^CMakeCache\.txt$", r"^.+\.so(?:\.[0-9]+)+$"],
            "prohibited_path_components": [".cache", "__pycache__", "cmakefiles"],
            "restricted_asset_suffixes": [".gdtf"],
            "asset_classes": [
                {"name": "fixture GDTFs", "path_prefix": "library/fixtures/", "suffixes": [".gdtf"], "max_bytes": 20}
            ],
            "exact_exceptions": {},
        }
        # Tests exercise audit directly, so compile the same configured patterns as load_policy.
        self.policy["_compiled_patterns"] = [re.compile(item, re.IGNORECASE) for item in self.policy["prohibited_filename_patterns"]]

    def tearDown(self) -> None:
        """Remove the isolated repository after each test."""
        self.temporary_directory.cleanup()

    def write(self, relative: str, size: int) -> None:
        """Write a fixture with an exact byte size."""
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"x" * size)

    def errors_for(self, relative: str, size: int) -> list[str]:
        """Audit one tracked fixture and return its diagnostics."""
        self.write(relative, size)
        errors, _ = guard.audit(self.root, [relative], self.policy)
        return errors

    def test_prohibited_compiled_artifact_fails(self) -> None:
        """Reject a tracked compiled library regardless of its size."""
        self.assertIn("prohibited artifact extension .dll", "\n".join(self.errors_for("bin/app.dll", 1)))

    def test_prohibited_build_or_cache_path_fails(self) -> None:
        """Reject generated files nested in known build/cache components."""
        self.assertIn("prohibited build/cache path", "\n".join(self.errors_for("build/CMakeFiles/state.dat", 1)))

    def test_generated_build_filename_fails(self) -> None:
        """Reject a generated CMake filename outside a conventional build directory."""
        self.assertIn("generated build filename", "\n".join(self.errors_for("scratch/CMakeCache.txt", 1)))

    def test_ordinary_source_and_small_binary_resource_pass(self) -> None:
        """Accept ordinary small tracked content independent of binary encoding."""
        self.write("src/main.cpp", 5)
        self.write("resources/icon.png", 9)
        errors, inspected = guard.audit(self.root, ["resources/icon.png", "src/main.cpp"], self.policy)
        self.assertEqual([], errors)
        self.assertEqual(2, inspected)

    def test_approved_bundled_gdtf_passes(self) -> None:
        """Accept a bounded GDTF only in its configured bundled-library scope."""
        self.assertEqual([], self.errors_for("library/fixtures/demo.gdtf", 20))

    def test_oversized_approved_gdtf_fails(self) -> None:
        """Reject growth beyond the approved GDTF class limit."""
        self.assertIn("fixture GDTFs maximum of 20", "\n".join(self.errors_for("library/fixtures/demo.gdtf", 21)))

    def test_gdtf_outside_approved_scope_fails(self) -> None:
        """Reject even a small GDTF outside the explicit bundled-library scope."""
        self.assertIn("outside its approved path scope", "\n".join(self.errors_for("misc/demo.gdtf", 1)))

    def test_unexpected_oversized_binary_fails(self) -> None:
        """Reject an unclassified binary payload beyond the ordinary limit."""
        self.assertIn("default tracked-file limit", "\n".join(self.errors_for("misc/payload.bin", 11)))

    def test_exact_exception_passes_and_stale_exception_fails(self) -> None:
        """Accept a bounded exact exception and diagnose it when stale."""
        self.policy["exact_exceptions"] = {"docs/intentional.bin": 30}
        self.assertEqual([], self.errors_for("docs/intentional.bin", 30))
        errors, _ = guard.audit(self.root, [], self.policy)
        self.assertIn("stale exact exception", "\n".join(errors))

    def test_malformed_policy_fails(self) -> None:
        """Reject unknown configuration keys and invalid byte limits."""
        policy_path = self.root / "policy.json"
        malformed = {key: value for key, value in self.policy.items() if not key.startswith("_")}
        malformed["default_max_bytes"] = 0
        policy_path.write_text(json.dumps(malformed), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "positive integer"):
            guard.load_policy(policy_path)

    def test_path_normalization_and_traversal_fail(self) -> None:
        """Reject traversal and platform-dependent separators in configured paths."""
        for relative in ("../outside.bin", "docs\\asset.bin"):
            policy_path = self.root / "policy.json"
            malformed = {key: value for key, value in self.policy.items() if not key.startswith("_")}
            malformed["exact_exceptions"] = {relative: 20}
            policy_path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "normalized repository-relative"):
                guard.load_policy(policy_path)


if __name__ == "__main__":
    unittest.main()
