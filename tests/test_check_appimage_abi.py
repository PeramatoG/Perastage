#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).parents[1] / ".github/scripts/check_appimage_abi.py"
SPEC = importlib.util.spec_from_file_location("check_appimage_abi", SCRIPT)
abi = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(abi)


class AppImageAbiTest(unittest.TestCase):
    def test_limits_accept_floor_and_reject_newer_versions(self):
        cases = [
            ("GLIBC", "2.35", False),
            ("GLIBC", "2.36", True),
            ("GLIBCXX", "3.4.29", False),
            ("GLIBCXX", "3.4.30", True),
            ("CXXABI", "1.3.13", False),
            ("CXXABI", "1.3.14", True),
        ]
        for family, version, expected in cases:
            with self.subTest(family=family, version=version):
                self.assertEqual(abi.exceeds_limit(family, abi.parse_version(version)), expected)

    def test_versions_compare_numerically(self):
        self.assertGreater(abi.parse_version("3.4.30"), abi.parse_version("3.4.9"))
        self.assertGreater(abi.parse_version("1.10.0"), abi.parse_version("1.9.99"))

    def test_parser_reads_only_version_needs(self):
        fixture = """Version symbols section '.gnu.version' contains 2 entries:
  000: 0 (*local*) 2 (GLIBC_2.36)
Version definition section '.gnu.version_d' contains 2 entries:
  Name: GLIBC_9.99
Version needs section '.gnu.version_r' contains 2 entries:
  Version: 1  File: libc.so.6  Cnt: 2
  0x0010:   Name: GLIBC_2.35  Flags: none  Version: 4
  0x0020:   Name: GLIBCXX_3.4.29  Flags: none  Version: 3
No version information found in this file.
"""
        self.assertEqual(
            abi.parse_required_versions(fixture),
            [("GLIBC", (2, 35), "GLIBC_2.35"), ("GLIBCXX", (3, 4, 29), "GLIBCXX_3.4.29")],
        )


if __name__ == "__main__":
    unittest.main()
