#!/usr/bin/env python3
"""Verify representative failures of the dedicated CLI boundary guard."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import check_cli_boundary


class CliBoundaryFixtureTests(unittest.TestCase):
    """Exercise focused synthetic violations without copying the repository."""

    def setUp(self) -> None:
        """Create the smallest valid repository shape accepted by the guard."""
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        (self.root / "cli").mkdir()
        (self.root / "cmake").mkdir()
        (self.root / "packaging").mkdir()
        (self.root / "CMakeLists.txt").write_text("add_subdirectory(cli)\n", encoding="utf-8")
        (self.root / "cli/CMakeLists.txt").write_text(
            "add_library(perastage_cli_support STATIC)\n"
            "add_executable(perastage_cli)\n"
            'set_target_properties(perastage_cli PROPERTIES OUTPUT_NAME "perastage-cli")\n',
            encoding="utf-8",
        )
        for name in ("PerastageInstall.cmake", "PerastageRuntimeStaging.cmake", "PerastagePackaging.cmake"):
            (self.root / "cmake" / name).write_text("", encoding="utf-8")
        self.assertEqual(check_cli_boundary.check(self.root), [])

    def tearDown(self) -> None:
        """Remove the temporary repository fixture."""
        self.temporary_directory.cleanup()

    def assert_source_rejected(self, include: str) -> None:
        """Require a synthetic direct include to violate the CLI boundary."""
        (self.root / "cli/main.cpp").write_text(f"#include {include}\n", encoding="utf-8")
        self.assertTrue(check_cli_boundary.check(self.root))

    def test_wx_include_is_rejected(self) -> None:
        """Reject a direct wxWidgets include in CLI source."""
        self.assert_source_rejected("<wx/init.h>")

    def test_application_header_is_rejected(self) -> None:
        """Reject the application bootstrap header by basename."""
        self.assert_source_rejected('"perastage_app.h"')

    def test_main_window_header_is_rejected(self) -> None:
        """Reject the GUI main-window header by basename."""
        self.assert_source_rejected('"mainwindow.h"')

    def test_state_and_localization_headers_are_rejected(self) -> None:
        """Reject representative configuration and localization bootstrap headers."""
        self.assert_source_rejected('"configmanager.h"')
        self.assert_source_rejected('"localization/localization_manager.h"')

    def test_gui_target_contamination_is_rejected_in_root(self) -> None:
        """Reject direct CLI source registration into the GUI target from root CMake."""
        (self.root / "CMakeLists.txt").write_text(
            "add_subdirectory(cli)\ntarget_sources(${PROJECT_NAME} PRIVATE cli/main.cpp)\n",
            encoding="utf-8",
        )
        self.assertTrue(check_cli_boundary.check(self.root))

    def test_packaging_owner_reference_is_rejected(self) -> None:
        """Reject adding the development CLI to the canonical packaging owner."""
        (self.root / "cmake/PerastagePackaging.cmake").write_text(
            "set(CPACK_COMPONENTS_ALL perastage_cli)\n", encoding="utf-8"
        )
        self.assertTrue(check_cli_boundary.check(self.root))


if __name__ == "__main__":
    unittest.main()
