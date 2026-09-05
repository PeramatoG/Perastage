#!/usr/bin/env python3
"""Exercise the module dependency direction checker with synthetic trees."""

from __future__ import annotations

import importlib.util
import tempfile
import sys
import unittest
from pathlib import Path

CHECK_PATH = Path(__file__).with_name("check_module_dependency_directions.py")
SPEC = importlib.util.spec_from_file_location("module_directions", CHECK_PATH)
assert SPEC and SPEC.loader
checker = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = checker
SPEC.loader.exec_module(checker)


class ModuleDependencyDirectionFixtures(unittest.TestCase):
    """Verify resolution, graph enforcement, and production-file scope."""

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        for module in checker.MODULES:
            (self.root / module).mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write(self, relative: str, contents: str = "") -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")

    def test_accepted_direction_passes(self) -> None:
        self.write("models/model.h")
        self.write("core/use.cpp", '#include "model.h"\n')
        _, errors = checker.validate(self.root, frozenset({("core", "models")}))
        self.assertEqual(errors, [])

    def test_unaccepted_direction_fails_with_evidence(self) -> None:
        self.write("models/model.h")
        self.write("core/use.cpp", '#include "model.h"\n')
        _, errors = checker.validate(self.root, frozenset())
        message = "\n".join(errors)
        self.assertIn("core -> models", message)
        self.assertIn('include: "model.h"', message)
        self.assertIn("introduced by: core/use.cpp", message)
        self.assertIn("resolved to: models/model.h", message)
        self.assertNotIn("\\", message)

    def test_same_module_include_does_not_create_an_edge(self) -> None:
        self.write("core/local.h")
        self.write("core/use.cpp", '#include "local.h"\n')
        evidence, errors = checker.validate(self.root, frozenset())
        self.assertEqual((evidence, errors), ([], []))

    def test_relative_cross_module_include_is_detected(self) -> None:
        self.write("models/model.h")
        self.write("core/use.cpp", '#include "../models/model.h"\n')
        evidence, _ = checker.audit(self.root)
        self.assertEqual((evidence[0].consumer, evidence[0].provider), ("core", "models"))

    def test_unqualified_include_root_is_resolved(self) -> None:
        self.write("viewer_common/shared.h")
        self.write("gui/use.cpp", '#include "shared.h"\n')
        evidence, _ = checker.audit(self.root)
        self.assertEqual(evidence[0].resolved, Path("viewer_common/shared.h"))

    def test_display_path_normalizes_root_before_relativizing(self) -> None:
        self.write("models/model.h")
        lexical_root = self.root / "core" / ".."
        resolved_path = (self.root / "models/model.h").resolve()
        self.assertEqual(checker.repository_path_for_display(lexical_root, resolved_path), "models/model.h")

    def test_outside_display_path_uses_a_normalized_fallback(self) -> None:
        outside = self.root.parent / "outside.h"
        rendered = checker.repository_path_for_display(self.root, outside)
        self.assertTrue(rendered.endswith("/outside.h"))
        self.assertNotIn("\\", rendered)

    def test_external_and_commented_includes_are_ignored(self) -> None:
        self.write("core/use.cpp", '#include <vector>\n#include "external_api.h"\n// #include "models/missing.h"\n')
        evidence, errors = checker.audit(self.root)
        self.assertEqual((evidence, errors), ([], []))

    def test_ambiguous_project_include_fails_clearly(self) -> None:
        self.write("core/shared.h")
        self.write("models/shared.h")
        self.write("gui/use.cpp", '#include "shared.h"\n')
        _, errors = checker.audit(self.root)
        self.assertIn("Ambiguous include", errors[0])
        self.assertIn("core/shared.h, models/shared.h", errors[0])
        self.assertNotIn("\\", errors[0])

    def test_test_only_source_is_excluded(self) -> None:
        self.write("models/model.h")
        self.write("core/tests/use.cpp", '#include "model.h"\n')
        evidence, errors = checker.audit(self.root)
        self.assertEqual((evidence, errors), ([], []))


if __name__ == "__main__":
    unittest.main()
